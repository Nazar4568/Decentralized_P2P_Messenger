#pragma once

#include "AppController.h"

#include <memory>

#include <wx/button.h>
#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

/**
 * Main chat window (wxFrame).
 *
 * Thread-safety contract:
 *   - P2PNode OpenDHT callbacks call wxQueueEvent(m_uiTarget, ...).
 *   - This frame is m_uiTarget (wxEvtHandler).
 *   - Bind() handlers run on the GUI thread and may safely modify controls.
 */
class MainWindow : public wxFrame {
public:
    MainWindow(std::shared_ptr<AppController> controller,
               const std::string& peerId,
               uint16_t port,
               const std::string& bootstrapPort);

private:
    void BuildUi();
    void WireP2PEvents();

    void OnSendClicked(wxCommandEvent& event);
    void OnAddContactClicked(wxCommandEvent& event);
    void OnBootstrapClicked(wxCommandEvent& event);
    void OnContactSelected(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    void OnP2PMessageReceived(wxThreadEvent& event);
    void OnP2PPeerFound(wxThreadEvent& event);
    void OnP2PNetworkStatus(wxThreadEvent& event);

    void AppendChatLine(const wxString& line);
    void AddContactToList(const wxString& peerId);
    void UpdateNetworkStatus(const wxString& status);

    std::shared_ptr<AppController> m_controller;
    std::string m_bootstrapPort;

    wxStaticText* m_statusText{nullptr};
    wxListBox* m_contactsList{nullptr};
    wxListBox* m_chatLog{nullptr};
    wxTextCtrl* m_addContactInput{nullptr};
    wxTextCtrl* m_peerIdInput{nullptr};
    wxTextCtrl* m_messageInput{nullptr};
    wxTextCtrl* m_bootstrapPortInput{nullptr};
    wxButton* m_addContactButton{nullptr};
    wxButton* m_bootstrapButton{nullptr};
    wxButton* m_sendButton{nullptr};
};
