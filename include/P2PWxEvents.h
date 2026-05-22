#pragma once

#include <wx/event.h>
#include <wx/wx.h>

// Custom event type IDs (must be unique within the application).
wxDECLARE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);

/// Parse payload queued by P2PNode::queueMessageEventToUi (format: "fromPeerId|text").
inline void ParseMessageReceivedEvent(const wxThreadEvent& event,
                                      wxString& outFromPeerId,
                                      wxString& outText)
{
    const wxString payload = event.GetString();
    const int sep = payload.Find(wxT('|'));
    if (sep == wxNOT_FOUND) {
        outFromPeerId = payload;
        outText.Clear();
        return;
    }
    outFromPeerId = payload.Left(sep);
    outText = payload.Mid(sep + 1);
}
