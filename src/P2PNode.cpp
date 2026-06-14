#include "../include/P2PNode.h"
#include "../include/P2PWxEvents.h"
#include <filesystem>
#include "../include/Types.h"
#include <chrono>
#include <iostream>
#include <wx/event.h>

wxIMPLEMENT_DYNAMIC_CLASS(P2PMessageThreadEvent, wxThreadEvent);

wxDEFINE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, P2PMessageThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_NETWORK_STATUS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_ERROR, wxThreadEvent);

namespace {

constexpr char kDhtFieldSep = '\x1F';

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
    return true;
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
    }
}

void P2PNode::bindUiTarget(wxEvtHandler* target)
{
    m_uiTarget = target;
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

    const auto id = dht::crypto::generateIdentity();
    m_dht.run(m_port, id, true);
    m_running = true;
    notifyNetworkStatus("Running on port " + std::to_string(m_port) + " as " + m_myId);

    const dht::InfoHash listenKey = dht::InfoHash::get(m_myId + "_inbox");
    std::cout << "[P2PNode] Слушаем ящик: " << m_myId << "_inbox\n";

    // ETERNAL LISTENER: This lambda wakes up with every new message
    dht::ValueCallback onMessageReceived = [this](const std::vector<std::shared_ptr<dht::Value>>& values, bool expired) {
        if (expired) return true; // Ignoring old packets removed from the network

        for (const auto& value : values) {
            const std::string payload(value->data.begin(), value->data.end());

            // PARSE THE HEADLINE (Separate Alice from the encrypted body)
            std::string senderId;
            std::string encryptedBody;
            if (!ParseDhtMessagePayload(payload, senderId, encryptedBody)) continue;
            if (senderId.empty() || senderId == m_myId) continue; // Защита от эха

            // 5.PARSE BODY: Split the string at '|' using a stringstream
            std::vector<std::string> parts;
            std::istringstream stream(encryptedBody);
            std::string part;
            while (std::getline(stream, part, '|')) {
                parts.push_back(part);
            }

            if (parts.size() < 4) continue; // If there are less than 4 pieces, the packet is broken, ignore it

            // We are putting together a structure to transfer to a security(Roman).
            EncryptedPacket packet;
            packet.version = 1;
            packet.senderId = senderId;
            packet.receiverId = m_myId;
            packet.messageId = parts[0];
            packet.nonce = parts[1];
            try {
                packet.timestamp = std::stoll(parts[2]);
            } catch (...) {
                continue; // Protection: If a hacker sends letters instead of a time, ignore it.
            }
            packet.ciphertext = parts[3];

            // 6. KEY REQUEST: We can't decrypt the packet without Alice's key!!
            dht::InfoHash senderProfileKey = dht::InfoHash::get(senderId);

            // We create an internal "Time Capsule" for decryption
            dht::GetCallback onSenderProfileFound = [this, senderId, packet](const std::vector<std::shared_ptr<dht::Value>>& profileValues) {
                if (profileValues.empty()) return true;

                // We get Alice's profile from the network
                std::string profilePayload(profileValues.front()->data.begin(), profileValues.front()->data.end());

                // As in sendPacket, we look for the last '|' to get the key
                size_t lastPipe = profilePayload.find_last_of('|');
                if (lastPipe == std::string::npos) return true;
                std::string b64PubKey = profilePayload.substr(lastPipe + 1);

                try {
                    // Converting text to raw bytes
                    std::vector<unsigned char> senderPubKey = m_crypto->fromBase64(b64PubKey);

                    // decryption!
                    std::string plainText = m_crypto->decrypt(
                        packet,               // Packet
                        m_myKeys.privateKey,  // Bob's private key
                        senderPubKey          // Alisa's pubkey
                    );

                    std::cout << "[DHT] Secure from " << senderId << ": " << plainText << '\n';

                    // Send the decrypted clear text to the interface (on the screen)!
                    this->notifyMessageReceived(senderId, plainText);

                } catch (const std::exception& e) {
                    // If a hacker changes even one byte in a message, decrypt will throw an error!!
                    std::cerr << "[Crypto] Hacking or decryption error: " << e.what() << "\n";
                }
                return true;
            };

            // start searching for Alice's profile. When it's found, the lambda above will be triggered!
            m_dht.get(senderProfileKey, onSenderProfileFound);
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

void P2PNode::bootstrap(const std::string& ip, const std::string& port)
{
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

    m_dht.put(key, payload, [this](bool success) {
        if (success) {
            std::cout << "[DHT] Profile published.\n";
            notifyNetworkStatus("Profile published to DHT");
        } else {
            std::cerr << "[DHT] Failed to publish profile.\n";
            notifyNetworkStatus("Failed to publish profile");
            notifyError(P2PErrorCode::NetworkUnavailable, "Failed to publish profile to DHT");
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
                const std::string payload(value->data.begin(), value->data.end());
                std::cout << "[DHT] Peer found (" << userId << "): " << payload << '\n';
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
    // We hash the recipient's name (for example, "bob")
    dht::InfoHash profileKey = dht::InfoHash::get(receiverId);

    // Creating a "Time Capsule" (lambda). It will trigger when the network finds Bob's profile.
    // We're CAPTURED the packet (message) into this capsule!
    dht::GetCallback onProfileFound = [this, receiverId, packet](const std::vector<std::shared_ptr<dht::Value>>& values) {
        if (values.empty()) return true; // Profile not found, cancel.

        // We take the profile string from the network. Format: Name|IP|Base64Key
        std::string profilePayload(values.front()->data.begin(), values.front()->data.end());

        // looking for the last dash '|', everything after it is the key!
        size_t lastPipe = profilePayload.find_last_of('|');
        if (lastPipe == std::string::npos) return true;
        std::string b64PubKey = profilePayload.substr(lastPipe + 1);

        try {
            // ENCRYPTION! Converting the key text into bytes and encrypt the message.
            std::vector<unsigned char> bobPubKey = m_crypto->fromBase64(b64PubKey);

            EncryptedPacket securePacket = m_crypto->encrypt(
                packet.ciphertext,    // Clean text
                bobPubKey,            // Bob's public key
                m_myKeys.privateKey,  // private key
                m_myId,               //from
                receiverId            // to
            );

            std::string secureBody = securePacket.messageId + "|" +
                                     securePacket.nonce + "|" +
                                     std::to_string(securePacket.timestamp) + "|" +
                                     securePacket.ciphertext;

            std::string fullPayload = m_myId + kDhtFieldSep + secureBody;

            dht::InfoHash inboxKey = dht::InfoHash::get(receiverId + "_inbox");
            m_dht.put(inboxKey, fullPayload, [receiverId](bool success) {
                if (success) std::cout << "[DHT] The encrypted package has been delivered!\n";
            });

        } catch (const std::exception& e) {
            std::cerr << "[Crypto] Encryption error: " << e.what() << "\n";
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
        if (secondsPassed >= 600) {
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