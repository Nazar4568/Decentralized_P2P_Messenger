#include "../include/P2PNode.h"
#include "../include/P2PWxEvents.h"

#include "../include/Types.h"
#include <chrono>
#include <iostream>
#include <wx/event.h>

// Define custom wx event types (one translation unit).
wxDEFINE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);

P2PNode::P2PNode() = default;

P2PNode::~P2PNode()
{
    stopNode();
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
    auto id = dht::crypto::generateIdentity();

    m_dht.run(m_port, id, true);

    dht::InfoHash listenKey = dht::InfoHash::get(m_myId + "_inbox");

    std::cout << "[P2PNode] strat listening incoming from : " << m_myId << "_inbox\n";

    dht::ValueCallback listenCallback = [this](const std::vector<std::shared_ptr<dht::Value>>& values, bool expired) {
        if (expired) {
            return true;
        }
        for (const auto& value : values) {
            std::string payload(value->data.begin(), value->data.end());
            size_t delimiter = payload.find(':');
            if (delimiter != std::string::npos) {
                std::string senderId = payload.substr(0, delimiter);
                std::string text = payload.substr(delimiter + 1);

                if (senderId != m_myId) {
                    std::cout << "\n[DHT] Incoming message from " << senderId << ": " << text << "\n";
                    this->notifyMessageReceived(senderId, text);
                }
            }
        }
        return true;
    };
    m_dht.listen(listenKey, listenCallback);
}

void P2PNode::stopNode()
{
    m_dht.join();

}

void P2PNode::sendMessage(const std::string& toPeerId, const std::string& text)
{
    std::cout << "[P2PNode] Mock send to '" << toPeerId << "': " << text << '\n';

    // Simulate network latency, then an "incoming" echo from the peer on a worker thread.
    std::thread([this, toPeerId, text]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        const std::string reply = "[echo] " + text;
        notifyMessageReceived(toPeerId, reply);
    }).detach();
}



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

    // Thread-safe path to wxWidgets: never touch UI here — only queue an event.
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

void P2PNode::queueMessageEventToUi(const std::string& fromPeerId, const std::string& text)
{
    if (!m_uiTarget) {
        return;
    }

    const wxString fromWx = wxString::FromUTF8(fromPeerId.c_str(), fromPeerId.size());
    const wxString textWx = wxString::FromUTF8(text.c_str(), text.size());

    // wxQueueEvent takes ownership of a heap-allocated event. The GUI thread will
    // process it inside MainWindow::OnP2PMessageReceived (via Bind).
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
    event->SetString(wxString::FromUTF8(peerId.c_str(), peerId.size()));
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