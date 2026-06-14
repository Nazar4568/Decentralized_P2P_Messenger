#pragma once

#include "IP2PNode.h"
#include "Types.h"

#ifdef HAS_OPENDHT
#include <opendht.h>
#endif
#include "CryptoService.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "../include/Types.h"
class wxEvtHandler;

/**
 * Concrete P2P node — OpenDHT when HAS_OPENDHT is defined, mock worker otherwise.
 * Network callbacks run on background threads; UI updates are queued via wxQueueEvent.
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

    void setNodeConfig(uint16_t port, const std::string& id);
    void bootstrap(const std::string& ip, const std::string& port);
    void publishProfile(const UserProfile& profile);
    void findPeer(const std::string& userId);
    void sendPacket(const std::string& receiverId, const EncryptedPacket& packet);

    std::string localPeerId() const { return m_myId; }
    uint16_t localPort() const { return m_port; }
    bool isRunning() const { return m_running; }

private:
    std::shared_ptr<CryptoService> m_crypto;
    KeyPair m_myKeys;
    void notifyMessageReceived(const std::string& fromPeerId, const std::string& text);
    void notifyPeerFound(const std::string& peerId);
    void notifyNetworkStatus(const std::string& status);
    void notifyError(const std::string& code, const std::string& message);
    void queueMessageEventToUi(const std::string& fromPeerId, const std::string& text);
    void queuePeerFoundEventToUi(const std::string& peerId);
    void queueNetworkStatusEventToUi(const std::string& status);
    void queueErrorEventToUi(const std::string& code, const std::string& message);

#ifdef HAS_OPENDHT
    dht::DhtRunner m_dht;
#else
    void mockWorkerLoop();
    std::unique_ptr<std::thread> m_mockThread;
#endif

    wxEvtHandler* m_uiTarget{nullptr};

    MessageReceivedHandler m_messageHandler;
    PeerFoundHandler m_peerFoundHandler;
    std::mutex m_handlerMutex;

    uint16_t m_port{4222};
    std::string m_myId{"default_node"};
    bool m_running{false};
};
