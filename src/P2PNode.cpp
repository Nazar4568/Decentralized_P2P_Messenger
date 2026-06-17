#include "../include/P2PNode.h"
#include "../include/P2PWxEvents.h"
#include <filesystem>
#include "../include/Types.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <iostream>
#include <sstream>
#include <wx/event.h>

wxIMPLEMENT_DYNAMIC_CLASS(P2PMessageThreadEvent, wxThreadEvent);

wxDEFINE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, P2PMessageThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_NETWORK_STATUS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_ERROR, wxThreadEvent);

namespace {

constexpr char kDhtFieldSep = '\x1F';

/// OpenDHT stores strings msgpack-encoded; raw value->data is not the payload text.
std::string UnpackDhtString(const std::shared_ptr<dht::Value>& value)
{
    if (!value) {
        return {};
    }
    try {
        return dht::Value::unpack<std::string>(*value);
    } catch (const std::exception&) {
        return std::string(value->data.begin(), value->data.end());
    }
}

bool ParseDhtMessagePayload(const std::string& payload,
                            std::string& outSenderId,
                            std::string& outBody)
{
    auto sep = payload.find(kDhtFieldSep);
    if (sep == std::string::npos) {
        // Legacy Sprint 2 format: "senderId:plaintext" (first colon only).
        sep = payload.find(':');
        if (sep == std::string::npos) {
            return false;
        }
    }

    outSenderId = payload.substr(0, sep);
    outBody = payload.substr(sep + 1);

    outSenderId.erase(outSenderId.begin(),
                      std::find_if(outSenderId.begin(), outSenderId.end(),
                                   [](unsigned char ch) { return !std::isspace(ch); }));
    outSenderId.erase(
        std::find_if(outSenderId.rbegin(), outSenderId.rend(),
                     [](unsigned char ch) { return !std::isspace(ch); })
            .base(),
        outSenderId.end());

    return !outSenderId.empty();
}

std::string BuildInboxDedupeKey(const std::string& senderId, const std::string& messageId)
{
    return senderId + kDhtFieldSep + messageId;
}

bool ParseSecureBody(const std::string& encryptedBody, EncryptedPacket& outPacket)
{
    std::vector<std::string> parts;
    std::istringstream stream(encryptedBody);
    std::string part;
    while (std::getline(stream, part, '|')) {
        parts.push_back(part);
    }

    if (parts.size() < 4) {
        return false;
    }

    outPacket.messageId = parts[0];
    outPacket.nonce = parts[1];
    try {
        outPacket.timestamp = std::stoll(parts[2]);
    } catch (...) {
        return false;
    }

    outPacket.ciphertext = parts[3];
    for (std::size_t i = 4; i < parts.size(); ++i) {
        outPacket.ciphertext += '|' + parts[i];
    }
    return true;
}

bool IsBenignCryptoError(const std::string& message)
{
    return message.find("validation failed") != std::string::npos ||
           message.find("REPLAY") != std::string::npos;
}

} // namespace

P2PNode::P2PNode() = default;

P2PNode::~P2PNode()
{
    stopNode();
}

void P2PNode::setNodeConfig(uint16_t port, const std::string& id)
{
    m_port = port;
    if (!id.empty()) {
        m_myId = id;
        m_myId.erase(m_myId.begin(),
                     std::find_if(m_myId.begin(), m_myId.end(),
                                  [](unsigned char ch) { return !std::isspace(ch); }));
        m_myId.erase(
            std::find_if(m_myId.rbegin(), m_myId.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            m_myId.end());
    }
}

void P2PNode::bindUiTarget(wxEvtHandler* target)
{
    m_uiTarget = target;
}

std::string P2PNode::localPublicKeyBase64() const
{
    std::lock_guard<std::mutex> lock(m_identityMutex);
    return m_publicKeyB64;
}

void P2PNode::setMessageReceivedHandler(MessageReceivedHandler handler)
{
    std::lock_guard<std::mutex> lock(m_handlerMutex);
    m_messageHandler = std::move(handler);
}

void P2PNode::setPeerFoundHandler(PeerFoundHandler handler)
{
    std::lock_guard<std::mutex> lock(m_handlerMutex);
    m_peerFoundHandler = std::move(handler);
}

void P2PNode::startNode()
{
    if (m_running) {
        return;
    }

#ifdef HAS_OPENDHT
    m_crypto = std::make_shared<CryptoService>();

    // --- key initialization (sprint 4) ---
    std::string pubKeyFile = m_myId + "_public.key";
    std::string privKeyFile = m_myId + "_private.key";

    // Check if the key files already exist on hard drive.
    if (std::filesystem::exists(pubKeyFile) && std::filesystem::exists(privKeyFile)) {
        try {
            m_myKeys = m_crypto->loadKeyPair(pubKeyFile, privKeyFile);
            std::cout << "[Crypto] Keys have been successfully taken from disk for the user.: " << m_myId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[Crypto] Error reading keys: " << e.what() << ". Generating new...\n";
            m_myKeys = m_crypto->generateKeyPair();
            m_crypto->saveKeyPair(m_myKeys, pubKeyFile, privKeyFile);
        }
    } else {
        // No files (first run). Generate new keys and save them to disk.
        std::cout << "[Crypto] First launch. Generating and saving keys for: " << m_myId << "\n";
        m_myKeys = m_crypto->generateKeyPair();
        if (m_crypto->saveKeyPair(m_myKeys, pubKeyFile, privKeyFile)) {
            std::cout << "[Crypto] The keys have been successfully saved to disk.\n";
        } else {
            std::cerr << "[Crypto] WARNING: Failed to save keys to disk.!\n";
        }
    }

    // Cache the base64 public key so the GUI export feature can read it safely.
    {
        std::lock_guard<std::mutex> lock(m_identityMutex);
        m_publicKeyB64 = m_crypto->toBase64(m_myKeys.publicKey);
    }

    const auto id = dht::crypto::generateIdentity();
    m_dht.run(m_port, id, true);
    m_running = true;
    notifyNetworkStatus("Running on port " + std::to_string(m_port) + " as " + m_myId);

    const dht::InfoHash listenKey = dht::InfoHash::get(m_myId + "_inbox");
    std::cout << "[P2PNode] Listening on inbox: " << m_myId << "_inbox\n";

    // ETERNAL LISTENER: This lambda wakes up with every new message
    dht::ValueCallback onMessageReceived = [this](const std::vector<std::shared_ptr<dht::Value>>& values, bool expired) {
        if (expired) return true; // Ignoring old packets removed from the network

        for (const auto& value : values) {
            const std::string payload = UnpackDhtString(value);

            // PARSE THE HEADLINE (Separate Alice from the encrypted body)
            std::string senderId;
            std::string encryptedBody;
            if (!ParseDhtMessagePayload(payload, senderId, encryptedBody)) {
                std::cerr << "[DHT] Dropping inbox value: unparseable payload\n";
                continue;
            }
            if (senderId.empty() || senderId == m_myId) {
                continue;
            }

            EncryptedPacket packet;
            packet.version = 1;
            packet.senderId = senderId;
            packet.receiverId = m_myId;
            if (!ParseSecureBody(encryptedBody, packet)) {
                std::cerr << "[DHT] Dropping inbox value from " << senderId
                          << ": malformed secure body\n";
                continue;
            }

            if (wasInboxMessageDelivered(senderId, packet.messageId)) {
                continue;
            }

            deliverSecureInboxMessage(senderId, packet);
        }
        return true;
    };

    // We put our listener on permanent background work
    m_dht.listen(listenKey, onMessageReceived);
    notifyNetworkStatus("Listening for secure messages on " + m_myId + "_inbox");
    // Hire a background worker and give it a task (backgroundPublishLoop method)
    m_publishThread = std::make_unique<std::thread>(&P2PNode::backgroundPublishLoop, this);
#else
    m_running = true;
    std::cout << "[P2PNode] Mock node started.\n";
    m_mockThread = std::make_unique<std::thread>([this]() { mockWorkerLoop(); });
#endif
}

void P2PNode::stopNode()
{
    if (!m_running) return;


    // This is a signal for our while(m_running) loop to terminate immediately.
    m_running = false;

#ifdef HAS_OPENDHT
    // We wait until the background thread finishes its work and exits the function.
    if (m_publishThread && m_publishThread->joinable()) {
        m_publishThread->join();
    }
    // Only now can we safely turn off the DHT network
    m_dht.join();
#else
    if (m_mockThread && m_mockThread->joinable()) {
        m_mockThread->join();
    }
    m_mockThread.reset();
#endif

    notifyNetworkStatus("Node stopped");
    std::cout << "[P2PNode] Node stopped.\n";
}

void P2PNode::sendMessage(const std::string& toPeerId, const std::string& text)
{
#ifdef HAS_OPENDHT
    if (m_myId.empty()) {
        notifyError(P2PErrorCode::NetworkUnavailable,
                    "Cannot send: local peer ID is not configured (set P2P_PEER_ID)");
        return;
    }

    EncryptedPacket packet;
    packet.senderId = m_myId;
    packet.receiverId = toPeerId;
    packet.ciphertext = text;
    sendPacket(toPeerId, packet);
#else
    std::cout << "[P2PNode] Mock send to '" << toPeerId << "': " << text << '\n';
    notifyNetworkStatus("Mock send to " + toPeerId);

    std::thread([this, toPeerId, text]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        notifyMessageReceived(toPeerId, "[echo] " + text);
    }).detach();
#endif
}

void P2PNode::setBootstrapHost(const std::string& host)
{
    if (!host.empty()) {
        m_bootstrapHost = host;
    }
}

void P2PNode::bootstrap(const std::string& ip, const std::string& port)
{
    if (!ip.empty()) {
        m_bootstrapHost = ip;
    }
#ifdef HAS_OPENDHT
    std::cout << "[P2PNode] Bootstrapping to " << ip << ':' << port << "...\n";
    notifyNetworkStatus("Bootstrapping to " + ip + ":" + port);
    m_dht.bootstrap(ip, port);
#else
    std::cout << "[P2PNode] Mock bootstrap to " << ip << ':' << port << '\n';
    notifyNetworkStatus("Mock bootstrap to " + ip + ":" + port);
#endif
}

void P2PNode::publishProfile(const UserProfile& profile)
{
    m_currentProfile = profile;
#ifdef HAS_OPENDHT
    const dht::InfoHash key = dht::InfoHash::get(profile.userId);

    // binary key to Base64
    std::string b64PubKey = m_crypto->toBase64(m_myKeys.publicKey);

    const std::string payload = profile.displayName + "|" + profile.tcpEndpoint + "|" + b64PubKey;

    std::cout << "[P2PNode] Publishing profile for " << profile.userId << "...\n";
    notifyNetworkStatus("Publishing profile for " + profile.userId);

    m_dht.put(key, payload, [this, profile](bool success) {
        if (success) {
            std::cout << "[DHT] Profile published.\n";
            notifyNetworkStatus("Profile published to DHT");
            cachePeerPublicKey(profile.userId, m_myKeys.publicKey);
        } else {
            // Transient on startup: the DHT has no peers yet, so the first
            // publish often "fails" before bootstrap completes. AppController
            // re-publishes after bootstrap and the background loop refreshes it,
            // so this is not a user-facing error — log and update status only.
            std::cerr << "[DHT] Profile publish not yet confirmed (will retry).\n";
            notifyNetworkStatus("Profile publish pending — retrying after bootstrap");
        }
    });
#else
    std::cout << "[P2PNode] Mock publish profile for " << profile.userId << '\n';
    notifyNetworkStatus("Mock profile published for " + profile.userId);
#endif
}

void P2PNode::findPeer(const std::string& userId)
{
#ifdef HAS_OPENDHT
    const dht::InfoHash key = dht::InfoHash::get(userId);
    std::cout << "[P2PNode] Searching for peer: " << userId << "...\n";
    notifyNetworkStatus("Searching DHT for " + userId);

    dht::GetCallback callback =
        [this, userId](const std::vector<std::shared_ptr<dht::Value>>& values) {
            if (values.empty()) {
                notifyNetworkStatus("Peer not found on DHT: " + userId);
                notifyError(P2PErrorCode::PeerNotFound,
                            "Peer not found on DHT: " + userId);
                return true;
            }

            for (const auto& value : values) {
                const std::string payload = UnpackDhtString(value);
                std::cout << "[DHT] Peer found (" << userId << "): " << payload << '\n';

                const size_t lastPipe = payload.find_last_of('|');
                if (lastPipe != std::string::npos) {
                    try {
                        const std::string b64PubKey = payload.substr(lastPipe + 1);
                        cachePeerPublicKey(userId, m_crypto->fromBase64(b64PubKey));
                    } catch (const std::exception& e) {
                        std::cerr << "[DHT] Could not cache public key for " << userId
                                  << ": " << e.what() << '\n';
                    }
                }

                notifyPeerFound(userId);
            }
            notifyNetworkStatus("Peer found: " + userId);
            return true;
        };

    m_dht.get(key, std::move(callback));
#else
    std::cout << "[P2PNode] Mock find peer: " << userId << '\n';
    notifyNetworkStatus("Mock lookup for " + userId);
    notifyPeerFound(userId);
#endif
}

void P2PNode::sendPacket(const std::string& receiverId, const EncryptedPacket& packet)
{
#ifdef HAS_OPENDHT
    const auto sendEncrypted = [this, receiverId, packet](
                                   const std::vector<unsigned char>& receiverPublicKey) {
        EncryptedPacket securePacket = m_crypto->encrypt(
            packet.ciphertext,
            receiverPublicKey,
            m_myKeys.privateKey,
            m_myId,
            receiverId);

        const std::string secureBody = securePacket.messageId + "|" +
                                       securePacket.nonce + "|" +
                                       std::to_string(securePacket.timestamp) + "|" +
                                       securePacket.ciphertext;

        const std::string fullPayload = m_myId + kDhtFieldSep + secureBody;
        const dht::InfoHash inboxKey = dht::InfoHash::get(receiverId + "_inbox");

        m_dht.put(inboxKey, fullPayload, [receiverId](bool success) {
            if (success) {
                std::cout << "[DHT] Encrypted message stored on " << receiverId
                          << "_inbox\n";
            } else {
                std::cerr << "[DHT] Failed to store message on " << receiverId
                          << "_inbox\n";
            }
        });
    };

    const dht::InfoHash profileKey = dht::InfoHash::get(receiverId);
    const auto sentFlag = std::make_shared<std::atomic<bool>>(false);

    dht::GetCallback onProfileFound = [this, receiverId, sendEncrypted, sentFlag](
                                          const std::vector<std::shared_ptr<dht::Value>>& values) {
        if (sentFlag->exchange(true)) {
            return true;
        }

        if (values.empty()) {
            std::cerr << "[DHT] Cannot send: recipient profile not found for "
                      << receiverId << '\n';
            notifyError(P2PErrorCode::PeerNotFound,
                        "Recipient profile not found on DHT: " + receiverId);
            return true;
        }

        const std::string profilePayload = UnpackDhtString(values.front());
        const size_t lastPipe = profilePayload.find_last_of('|');
        if (lastPipe == std::string::npos) {
            notifyError(P2PErrorCode::PeerNotFound,
                        "Recipient profile on DHT is malformed: " + receiverId);
            return true;
        }

        try {
            const std::string b64PubKey = profilePayload.substr(lastPipe + 1);
            const std::vector<unsigned char> receiverPublicKey = m_crypto->fromBase64(b64PubKey);
            cachePeerPublicKey(receiverId, receiverPublicKey);
            sendEncrypted(receiverPublicKey);
        } catch (const std::exception& e) {
            std::cerr << "[Crypto] Encryption error: " << e.what() << '\n';
            notifyError(P2PErrorCode::DeliveryFailed, e.what());
        }
        return true;
    };

    m_dht.get(profileKey, onProfileFound);
#else
    sendMessage(receiverId, packet.ciphertext);
#endif
}

#ifndef HAS_OPENDHT
void P2PNode::mockWorkerLoop()
{
    int tick = 0;
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(8));
        if (!m_running) {
            break;
        }
        ++tick;
        notifyMessageReceived("mock-peer-bob",
                              "Periodic mock message #" + std::to_string(tick));
    }
}
#endif

#ifdef HAS_OPENDHT
void P2PNode::cachePeerPublicKey(const std::string& userId,
                                 const std::vector<unsigned char>& publicKey)
{
    if (userId.empty() || publicKey.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_peerKeysMutex);
    m_peerPublicKeys[userId] = publicKey;
}

bool P2PNode::tryGetCachedPeerPublicKey(const std::string& userId,
                                        std::vector<unsigned char>& outPublicKey) const
{
    std::lock_guard<std::mutex> lock(m_peerKeysMutex);
    const auto it = m_peerPublicKeys.find(userId);
    if (it == m_peerPublicKeys.end()) {
        return false;
    }
    outPublicKey = it->second;
    return true;
}

bool P2PNode::wasInboxMessageDelivered(const std::string& senderId,
                                       const std::string& messageId) const
{
    std::lock_guard<std::mutex> lock(m_deliveredMutex);
    return m_deliveredInboxKeys.count(BuildInboxDedupeKey(senderId, messageId)) > 0;
}

void P2PNode::markInboxMessageDelivered(const std::string& senderId,
                                       const std::string& messageId)
{
    std::lock_guard<std::mutex> lock(m_deliveredMutex);
    m_deliveredInboxKeys.insert(BuildInboxDedupeKey(senderId, messageId));
}

void P2PNode::deliverSecureInboxMessage(const std::string& senderId,
                                        const EncryptedPacket& packet)
{
    const auto decryptWithSenderKey =
        [this, senderId, packet](const std::vector<unsigned char>& senderPublicKey) {
            EncryptedPacket inbound = packet;
            inbound.senderId = senderId;
            inbound.receiverId = m_myId;

            const std::string plainText = m_crypto->decrypt(
                inbound,
                m_myKeys.privateKey,
                senderPublicKey,
                m_myId);

            markInboxMessageDelivered(senderId, packet.messageId);
            std::cout << "[DHT] Secure from " << senderId << ": " << plainText << '\n';
            notifyMessageReceived(senderId, plainText);
        };

    const dht::InfoHash senderProfileKey = dht::InfoHash::get(senderId);
    const auto processedFlag = std::make_shared<std::atomic<bool>>(false);

    dht::GetCallback onSenderProfileFound =
        [this, senderId, packet, decryptWithSenderKey, processedFlag](
            const std::vector<std::shared_ptr<dht::Value>>& profileValues) {
            if (processedFlag->exchange(true)) {
                return true;
            }

            if (profileValues.empty()) {
                std::cerr << "[DHT] Cannot decrypt: sender profile not found for "
                          << senderId << '\n';
                notifyError(P2PErrorCode::PeerNotFound,
                            "Sender profile not found on DHT: " + senderId);
                return true;
            }

            const std::string profilePayload = UnpackDhtString(profileValues.front());
            const size_t lastPipe = profilePayload.find_last_of('|');
            if (lastPipe == std::string::npos) {
                return true;
            }

            try {
                const std::string b64PubKey = profilePayload.substr(lastPipe + 1);
                const std::vector<unsigned char> senderPublicKey =
                    m_crypto->fromBase64(b64PubKey);
                cachePeerPublicKey(senderId, senderPublicKey);
                decryptWithSenderKey(senderPublicKey);
            } catch (const std::exception& e) {
                const std::string error = e.what();
                std::cerr << "[Crypto] Decryption error from " << senderId << ": "
                          << error << '\n';
                if (!IsBenignCryptoError(error)) {
                    notifyError(P2PErrorCode::DecryptionFailed, error);
                }
            }
            return true;
        };

    m_dht.get(senderProfileKey, onSenderProfileFound);
}
#endif

void P2PNode::notifyMessageReceived(const std::string& fromPeerId, const std::string& text)
{
    MessageReceivedHandler handler;
    {
        std::lock_guard<std::mutex> lock(m_handlerMutex);
        handler = m_messageHandler;
    }
    if (handler) {
        handler(IncomingMessage{fromPeerId, text});
    }

    queueMessageEventToUi(fromPeerId, text);
}

void P2PNode::notifyPeerFound(const std::string& peerId)
{
    PeerFoundHandler handler;
    {
        std::lock_guard<std::mutex> lock(m_handlerMutex);
        handler = m_peerFoundHandler;
    }
    if (handler) {
        handler(PeerFoundNotification{peerId});
    }

    queuePeerFoundEventToUi(peerId);
}

void P2PNode::notifyNetworkStatus(const std::string& status)
{
    queueNetworkStatusEventToUi(status);
}

void P2PNode::notifyError(const std::string& code, const std::string& message)
{
    queueErrorEventToUi(code, message);
}

void P2PNode::queueMessageEventToUi(const std::string& fromPeerId, const std::string& text)
{
    if (!m_uiTarget) {
        return;
    }

    auto* event = new P2PMessageThreadEvent(wxEVT_P2P_MESSAGE_RECEIVED);
    event->SetMessageData(fromPeerId, text);
    wxQueueEvent(m_uiTarget, event);
}

void P2PNode::queuePeerFoundEventToUi(const std::string& peerId)
{
    if (!m_uiTarget) {
        return;
    }

    auto* event = new wxThreadEvent(wxEVT_P2P_PEER_FOUND);
    event->SetString(wxString::FromUTF8(peerId.c_str(), static_cast<int>(peerId.size())));
    wxQueueEvent(m_uiTarget, event);
}

void P2PNode::queueNetworkStatusEventToUi(const std::string& status)
{
    if (!m_uiTarget) {
        return;
    }

    auto* event = new wxThreadEvent(wxEVT_P2P_NETWORK_STATUS);
    event->SetString(wxString::FromUTF8(status.c_str(), static_cast<int>(status.size())));
    wxQueueEvent(m_uiTarget, event);
}

void P2PNode::queueErrorEventToUi(const std::string& code, const std::string& message)
{
    if (!m_uiTarget) {
        return;
    }

    const wxString codeWx = wxString::FromUTF8(code.c_str(), static_cast<int>(code.size()));
    const wxString msgWx = wxString::FromUTF8(message.c_str(), static_cast<int>(message.size()));

    auto* event = new wxThreadEvent(wxEVT_P2P_ERROR);
    event->SetString(codeWx + wxT("|") + msgWx);
    wxQueueEvent(m_uiTarget, event);
}

void P2PNode::backgroundPublishLoop()
{
    int secondsPassed = 0;

    // While the m_running flag == true, the worker lives
    while (m_running) {
        // We sleep for exactly 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // If the user presses "Exit" during sleep, we immediately interrupt the cycle.
        if (!m_running) break;

        secondsPassed++;

        // wait 10 min
        if (secondsPassed >= 5) {
            secondsPassed = 0; //Reset the stopwatch

            // If the profile has already been created by the user, we publish it again!
            if (!m_currentProfile.userId.empty()) {
                std::cout << "[P2PNode] Background profile refresh (TTL refresh)...\n";

                // We call our own method. It will send the packet to the DHT.
                publishProfile(m_currentProfile);
            }
        }
    }
    std::cout << "[P2PNode] The background worker thread has completed successfully.\n";
}
