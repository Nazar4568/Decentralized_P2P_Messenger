#pragma once

#include <functional>
#include <string>

// Forward declaration: keeps the interface free of wxWidgets headers.
// The concrete node uses wxEvtHandler only as an opaque event-dispatch target.
class wxEvtHandler;

/// Payload for an incoming chat message (pure C++17, UI-agnostic).
struct IncomingMessage {
    std::string fromPeerId;
    std::string text;
};

/// Payload when a peer is discovered on the DHT.
struct PeerFoundNotification {
    std::string peerId;
};

/**
 * Abstract P2P node contract for Sprint 1+.
 *
 * Design: loosely coupled from wxWidgets. Network/mock threads must NOT call
 * wx API directly. Instead:
 *   1) UI calls bindUiTarget(this) once (MainWindow is a wxEvtHandler).
 *   2) Background work in P2PNode posts custom wxThreadEvent via wxQueueEvent.
 *   3) MainWindow Bind() handlers run on the GUI thread and update controls.
 *
 * Optional std::function handlers are an alternative Observer hook for tests
 * or non-wx clients; they are still invoked from worker threads and must not
 * touch wxWidgets.
 */
class IP2PNode {
public:
    using MessageReceivedHandler = std::function<void(IncomingMessage)>;
    using PeerFoundHandler = std::function<void(PeerFoundNotification)>;

    virtual ~IP2PNode() = default;

    virtual void sendMessage(const std::string& toPeerId, const std::string& text) = 0;
    virtual void startNode() = 0;
    virtual void stopNode() = 0;

    /// Registers the wx window/frame that will receive queued thread events.
    virtual void bindUiTarget(wxEvtHandler* target) = 0;

    virtual void setMessageReceivedHandler(MessageReceivedHandler handler) = 0;
    virtual void setPeerFoundHandler(PeerFoundHandler handler) = 0;
};
