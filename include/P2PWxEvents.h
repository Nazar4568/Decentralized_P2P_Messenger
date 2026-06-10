#pragma once

#include <wx/event.h>
#include <wx/wx.h>

wxDECLARE_EVENT(wxEVT_P2P_MESSAGE_RECEIVED, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_P2P_PEER_FOUND, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_P2P_NETWORK_STATUS, wxThreadEvent);

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
