#include "../include/P2PNode.h"
#include "../include/P2PWxEvents.h"

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
    if (m_running.exchange(true)) {
        return;
    }

    std::cout << "[P2PNode] Mock node started (Sprint 1).\n";

    m_mockThread = std::make_unique<std::thread>([this]() { mockWorkerLoop(); });

    // Simulate an early peer discovery event shortly after startup.
    notifyPeerFound("mock-peer-alice");
}

void P2PNode::stopNode()
{
    if (!m_running.exchange(false)) {
        return;
    }

    if (m_mockThread && m_mockThread->joinable()) {
        m_mockThread->join();
    }
    m_mockThread.reset();

    std::cout << "[P2PNode] Mock node stopped.\n";
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

void P2PNode::mockWorkerLoop()
{
    int tick = 0;
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!m_running.load()) {
            break;
        }
        ++tick;
        notifyMessageReceived("mock-peer-bob",
                              "Periodic mock message #" + std::to_string(tick));
    }
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
