#include "../include/P2PNode.h"
#include "../include/P2PWxEvents.h"

#include "../include/Types.h"
#include <chrono>
#include <iostream>
#include <wx/event.h>

wxDEFINE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_NETWORK_STATUS, wxThreadEvent);

P2PNode::P2PNode() = default;

P2PNode::~P2PNode()
{
    stopNode();
}

void P2PNode::setNodeConfig(uint16_t port, const std::string& id)
{
    m_port = port;
    m_myId = id;
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
    const auto id = dht::crypto::generateIdentity();
    m_dht.run(m_port, id, true);
    m_running = true;

    notifyNetworkStatus("Running on port " + std::to_string(m_port) + " as " + m_myId);

    const dht::InfoHash listenKey = dht::InfoHash::get(m_myId + "_inbox");
    std::cout << "[P2PNode] Listening on inbox key: " << m_myId << "_inbox\n";

    dht::ValueCallback listenCallback =
        [this](const std::vector<std::shared_ptr<dht::Value>>& values, bool expired) {
            if (expired) {
                return true;
            }
            for (const auto& value : values) {
                const std::string payload(value->data.begin(), value->data.end());
                const size_t delimiter = payload.find(':');
                if (delimiter == std::string::npos) {
                    continue;
                }

                const std::string senderId = payload.substr(0, delimiter);
                const std::string text = payload.substr(delimiter + 1);

                if (senderId.empty() || senderId == m_myId) {
                    continue;
                }

                std::cout << "[DHT] Incoming message from " << senderId << ": " << text << '\n';
                notifyMessageReceived(senderId, text);
            }
            return true;
        };

    m_dht.listen(listenKey, std::move(listenCallback));
    notifyNetworkStatus("Listening for messages on " + m_myId + "_inbox");
#else
    m_running = true;
    std::cout << "[P2PNode] Mock node started (install libopendht-dev for real DHT).\n";
    notifyNetworkStatus("Mock mode — install libopendht-dev for real networking");
    m_mockThread = std::make_unique<std::thread>([this]() { mockWorkerLoop(); });
    notifyPeerFound("mock-peer-alice");
#endif
}

void P2PNode::stopNode()
{
    if (!m_running) {
        return;
    }

#ifdef HAS_OPENDHT
    m_dht.join();
#else
    if (m_mockThread && m_mockThread->joinable()) {
        m_mockThread->join();
    }
    m_mockThread.reset();
#endif

    m_running = false;
    notifyNetworkStatus("Node stopped");
    std::cout << "[P2PNode] Node stopped.\n";
}

void P2PNode::sendMessage(const std::string& toPeerId, const std::string& text)
{
#ifdef HAS_OPENDHT
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
#ifdef HAS_OPENDHT
    const dht::InfoHash key = dht::InfoHash::get(profile.userId);
    const std::string payload = profile.displayName + "|" + profile.tcpEndpoint;

    std::cout << "[P2PNode] Publishing profile for " << profile.userId << "...\n";
    notifyNetworkStatus("Publishing profile for " + profile.userId);

    m_dht.put(key, payload, [this](bool success) {
        if (success) {
            std::cout << "[DHT] Profile published.\n";
            notifyNetworkStatus("Profile published to DHT");
        } else {
            std::cerr << "[DHT] Failed to publish profile.\n";
            notifyNetworkStatus("Failed to publish profile");
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
    const dht::InfoHash key = dht::InfoHash::get(receiverId + "_inbox");
    const std::string payload = packet.senderId + ":" + packet.ciphertext;

    std::cout << "[P2PNode] Sending to " << receiverId << "...\n";
    notifyNetworkStatus("Sending message to " + receiverId);

    m_dht.put(key, payload, [this, receiverId](bool success) {
        if (success) {
            std::cout << "[DHT] Message delivered to DHT.\n";
            notifyNetworkStatus("Message delivered to " + receiverId);
        } else {
            std::cerr << "[DHT] Failed to deliver message.\n";
            notifyNetworkStatus("Failed to deliver message to " + receiverId);
        }
    });
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

void P2PNode::queueMessageEventToUi(const std::string& fromPeerId, const std::string& text)
{
    if (!m_uiTarget) {
        return;
    }

    const wxString fromWx = wxString::FromUTF8(fromPeerId.c_str(), static_cast<int>(fromPeerId.size()));
    const wxString textWx = wxString::FromUTF8(text.c_str(), static_cast<int>(text.size()));

    auto* event = new wxThreadEvent(wxEVT_P2P_MESSAGE_RECEIVED);
    event->SetString(fromWx + wxT("|") + textWx);
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

void P2PNode::bootstrap(const std::string& ip, const std::string& port)
{
    std::cout << "[P2PNode] Connecting to " << ip << ":" << port << "...\n";
    m_dht.bootstrap(ip, port);
}
void P2PNode::publishProfile(const UserProfile& profile)
{

    dht::InfoHash key = dht::InfoHash::get(profile.userId);

    std::string payload = profile.displayName + "|" + profile.tcpEndpoint;

    std::cout << "[P2PNode] Publishing a profile by key: " << profile.userId << "...\n";


    m_dht.put(key, payload, [](bool success) {
        if (success) {
            std::cout << "[DHT] Success! The profile has been replicated across the network..\n";
        } else {
            std::cerr << "[DHT] Error: Failed to publish profile.\n";
        }
    });
}

void P2PNode::findPeer(const std::string& userId) {
    dht::InfoHash key = dht::InfoHash::get(userId);
    std::cout << "[P2PNode] Searching for peer: " << userId << "...\n";

    dht::GetCallback callback = [this, userId](const std::vector<std::shared_ptr<dht::Value>>& values) {
        for (const auto& value : values) {
            std::string payload(value->data.begin(), value->data.end());
            std::cout << "[DHT] Peer found (" << userId << "): " << payload << "\n";
            this->notifyPeerFound(userId);
        }
        return true;
    };
    m_dht.get(key, callback);
}
void P2PNode::sendPacket(const std::string& receiverId, const EncryptedPacket& packet)
{
    dht::InfoHash key = dht::InfoHash::get(receiverId + "_inbox");

    std::string payload = packet.senderId + ":" + packet.ciphertext;

    std::cout << "[P2PNode] Sending packet to " << receiverId << "...\n";

    m_dht.put(key, payload, [](bool success) {
        if (success) {
            std::cout << "[DHT] Packet delivered to DHT network.\n";
        } else {
            std::cerr << "[DHT] Error: Failed to deliver packet.\n";
        }
    });
}