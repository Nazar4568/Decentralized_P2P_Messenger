#pragma once

#include "IP2PNode.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

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

private:
    void mockWorkerLoop();
    void notifyMessageReceived(const std::string& fromPeerId, const std::string& text);
    void notifyPeerFound(const std::string& peerId);
    void queueMessageEventToUi(const std::string& fromPeerId, const std::string& text);
    void queuePeerFoundEventToUi(const std::string& peerId);

    wxEvtHandler* m_uiTarget{nullptr};

    MessageReceivedHandler m_messageHandler;
    PeerFoundHandler m_peerFoundHandler;
    std::mutex m_handlerMutex;

    std::atomic<bool> m_running{false};
    std::unique_ptr<std::thread> m_mockThread;
};
