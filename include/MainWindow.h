#pragma once

#include "AppController.h"

#include <memory>
#include <string>

#include <wx/button.h>
#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

/**
 * Main chat window (wxFrame) — Sprint 3.
 *
 * Thread-safety: P2PNode queues wxThreadEvent; Bind() handlers run on the GUI thread only.
 * Crypto: UI handles plaintext only; encryption stays in the backend.
 */
class MainWindow : public wxFrame {
public:
    MainWindow(std::shared_ptr<AppController> controller,
               const std::string& peerId,
               uint16_t port,
               const std::string& bootstrapPort);

private:
    enum class ChatMessageKind { Incoming, Outgoing, System };

    void BuildUi();
    void WireP2PEvents();

    void OnSendClicked(wxCommandEvent& event);
    void OnAddContactClicked(wxCommandEvent& event);
    void OnBootstrapClicked(wxCommandEvent& event);
    void OnContactSelected(wxCommandEvent& event);
    void OnMessageKeyDown(wxKeyEvent& event);
    void OnClose(wxCloseEvent& event);

    void OnP2PMessageReceived(wxThreadEvent& event);
    void OnP2PPeerFound(wxThreadEvent& event);
    void OnP2PNetworkStatus(wxThreadEvent& event);
    void OnP2PError(wxThreadEvent& event);

    void SendCurrentMessage();
    void AppendChatMessage(ChatMessageKind kind,
                           const wxString& peerLabel,
                           const wxString& text,
                           const wxString& timestamp = wxEmptyString);
    void AppendSystemLine(const wxString& text);
    void AddContactToList(const wxString& peerId);
    void UpdateNetworkStatus(const wxString& status);
    void ShowError(const wxString& code, const wxString& message);
    void loadChatHistory(const std::string& contactId);
    wxString CurrentTimestamp() const;

    std::shared_ptr<AppController> m_controller;
    std::string m_bootstrapPort;
    std::string m_activeContactId;

    wxStaticText* m_statusText{nullptr};
    wxSplitterWindow* m_splitter{nullptr};
    wxListBox* m_contactsList{nullptr};
    wxRichTextCtrl* m_chatLog{nullptr};
    wxTextCtrl* m_addContactInput{nullptr};
    wxTextCtrl* m_peerIdInput{nullptr};
    wxTextCtrl* m_messageInput{nullptr};
    wxTextCtrl* m_bootstrapPortInput{nullptr};
    wxButton* m_addContactButton{nullptr};
    wxButton* m_bootstrapButton{nullptr};
    wxButton* m_sendButton{nullptr};
};
