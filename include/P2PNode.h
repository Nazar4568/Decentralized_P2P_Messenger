#pragma once

#include "IP2PNode.h"
#include <opendht.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "../include/Types.h"
class wxEvtHandler;

/**
 * Concrete P2P node — Sprint 1 mock implementation.
 * OpenDHT integration is deferred to Sprint 2; this class simulates async I/O.
 */
class P2PNode : public IP2PNode {
public:
    P2PNode();
    ~P2PNode() override;

    void sendMessage(const std::string& toPeerId, const std::string& text) override;
    void startNode() override;
    void stopNode() override;

    void bindUiTarget(wxEvtHandler* target) override;
    void setMessageReceivedHandler(MessageReceivedHandler handler) override;
    void setPeerFoundHandler(PeerFoundHandler handler) override;
    void bootstrap(const std::string& ip, const std::string& port);
    void publishProfile(const UserProfile& profile);
    void findPeer(const std::string& userId);
    void sendPacket(const std::string& receiverId, const EncryptedPacket& packet) ;
    void setNodeConfig(uint16_t port, const std::string& id) {
        m_port = port;
        m_myId = id;
    }
private:
    void notifyMessageReceived(const std::string& fromPeerId, const std::string& text);
    void notifyPeerFound(const std::string& peerId);
    void queueMessageEventToUi(const std::string& fromPeerId, const std::string& text);
    void queuePeerFoundEventToUi(const std::string& peerId);
    dht::DhtRunner m_dht;
    wxEvtHandler* m_uiTarget{nullptr};

    MessageReceivedHandler m_messageHandler;
    PeerFoundHandler m_peerFoundHandler;
    std::mutex m_handlerMutex;

    uint16_t m_port = 4222;
    std::string m_myId = "default_node";

};
