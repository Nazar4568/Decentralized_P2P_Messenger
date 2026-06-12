#pragma once

#include <string>
#include <utility>

#include <wx/event.h>
#include <wx/wx.h>

wxDECLARE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_P2P_NETWORK_STATUS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_P2P_ERROR, wxThreadEvent);

/// Backend error categories queued from network threads (GUI thread only).
namespace P2PErrorCode {
inline const char* PeerNotFound        = "PEER_NOT_FOUND";
inline const char* DecryptionFailed    = "DECRYPTION_FAILED";
inline const char* NetworkUnavailable  = "NETWORK_UNAVAILABLE";
inline const char* DeliveryFailed      = "DELIVERY_FAILED";
} // namespace P2PErrorCode

/**
 * Carries sender + body as std::string fields (never packed into wxString).
 *
 * wxGTK 3.0 drops C0 control bytes (0x00–0x1F) in wxString UTF-8 round-trips,
 * so packing with unit separator 0x1F in SetString() corrupts the payload.
 */
class P2PMessageThreadEvent : public wxThreadEvent {
public:
    explicit P2PMessageThreadEvent(wxEventType eventType = wxEVT_THREAD, int id = wxID_ANY)
        : wxThreadEvent(eventType, id)
    {
    }

    P2PMessageThreadEvent(const P2PMessageThreadEvent& event)
        : wxThreadEvent(event)
        , m_fromPeerId(event.m_fromPeerId)
        , m_text(event.m_text)
    {
    }

    void SetMessageData(std::string fromPeerId, std::string text)
    {
        m_fromPeerId = std::move(fromPeerId);
        m_text = std::move(text);
        SetString(wxString::FromUTF8(m_fromPeerId.c_str()));
    }

    const std::string& GetFromPeerId() const { return m_fromPeerId; }
    const std::string& GetText() const { return m_text; }

    virtual wxEvent* Clone() const { return new P2PMessageThreadEvent(*this); }

private:
    std::string m_fromPeerId;
    std::string m_text;

    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(P2PMessageThreadEvent);
};

wxDECLARE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, P2PMessageThreadEvent);

inline void ParseMessageReceivedEvent(const wxThreadEvent& event,
                                      wxString& outFromPeerId,
                                      wxString& outText)
{
    if (const auto* msg = dynamic_cast<const P2PMessageThreadEvent*>(&event)) {
        outFromPeerId = wxString::FromUTF8(msg->GetFromPeerId().c_str());
        outText = wxString::FromUTF8(msg->GetText().c_str());
        return;
    }

    // Legacy fallback for older builds that packed fields into SetString().
    const std::string payload(event.GetString().utf8_str());

    std::size_t sep = payload.find('|');
    if (sep == std::string::npos) {
        sep = payload.find(':');
    }

    if (sep != std::string::npos && sep > 0) {
        const std::string fromPeer = payload.substr(0, sep);
        const std::string text = payload.substr(sep + 1);
        if (!fromPeer.empty()) {
            outFromPeerId = wxString::FromUTF8(fromPeer.c_str());
            outText = wxString::FromUTF8(text.c_str());
            return;
        }
    }

    outFromPeerId = event.GetString();
    outText.Clear();
}

/// Payload format from P2PNode::queueErrorEventToUi: "ERROR_CODE|human-readable message".
inline void ParseP2PErrorEvent(const wxThreadEvent& event,
                               wxString& outCode,
                               wxString& outMessage)
{
    const wxString payload = event.GetString();
    const int sep = payload.Find(wxT('|'));
    if (sep == wxNOT_FOUND) {
        outCode = wxT("UNKNOWN");
        outMessage = payload;
        return;
    }
    outCode = payload.Left(sep);
    outMessage = payload.Mid(sep + 1);
}
