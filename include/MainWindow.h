#pragma once

#include "IP2PNode.h"

#include <memory>

#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>

/**
 * Main chat window (wxFrame).
 *
 * Thread-safety contract:
 *   - P2PNode mock/network threads call wxQueueEvent(m_uiTarget, ...).
 *   - This frame is m_uiTarget (wxEvtHandler).
 *   - Bind() handlers (OnP2PMessageReceived / OnP2PPeerFound) run on the GUI thread
 *     and may safely modify wxListBox, wxTextCtrl, etc.
 */
class MainWindow : public wxFrame {
public:
    MainWindow(std::shared_ptr<IP2PNode> node);

private:
    void BuildUi();
    void WireP2PEvents();

    void OnSendClicked(wxCommandEvent& event);
    void OnP2PMessageReceived(wxThreadEvent& event);
    void OnP2PPeerFound(wxThreadEvent& event);

    void AppendChatLine(const wxString& line);

    std::shared_ptr<IP2PNode> m_node;

    wxListBox* m_chatLog{nullptr};
    wxTextCtrl* m_peerIdInput{nullptr};
    wxTextCtrl* m_messageInput{nullptr};
    wxButton* m_sendButton{nullptr};
};
