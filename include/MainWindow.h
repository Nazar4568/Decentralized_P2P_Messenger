#pragma once

#include "AppController.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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
               const std::string& bootstrapHost,
               const std::string& bootstrapPort);

private:
    enum class ChatMessageKind { Incoming, Outgoing, System };

    // One stored chat line. We keep per-contact history in memory so switching
    // between conversations (and back) re-renders the full thread instead of
    // wiping it.
    struct ChatEntry {
        ChatMessageKind kind;
        wxString peer;
        wxString text;
        wxString timestamp;
    };

    void BuildUi();
    void BuildMenuBar();
    void WireP2PEvents();

    void OnSendClicked(wxCommandEvent& event);
    void OnAddContactClicked(wxCommandEvent& event);
    void OnBootstrapClicked(wxCommandEvent& event);
    void OnContactSelected(wxCommandEvent& event);
    void OnMessageKeyDown(wxKeyEvent& event);
    void OnExportIdentity(wxCommandEvent& event);
    void OnChangePeerId(wxCommandEvent& event);
    void OnShowAbout(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
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

    // Per-contact history management.
    void RecordMessage(const std::string& contactId, ChatMessageKind kind,
                       const wxString& peer, const wxString& text);
    void RenderHistory(const std::string& contactId);
    void SwitchActiveContact(const std::string& contactId);

    // Runs a blocking network task (start / restart) off the GUI thread.
    void RunNetworkTaskAsync(std::function<void()> task);

    wxString CurrentTimestamp() const;

    std::shared_ptr<AppController> m_controller;
    std::string m_bootstrapHost;
    std::string m_bootstrapPort;
    std::string m_activeContactId;

    // In-memory chat history keyed by contact (peer) ID.
    std::map<std::string, std::vector<ChatEntry>> m_history;

    // Network start()/restart (key generation + DHT init) runs here so the GUI
    // thread never blocks. Joined during the asynchronous shutdown sequence.
    std::thread m_networkThread;
    bool m_shuttingDown{false};

    wxStaticText* m_statusText{nullptr};
    wxSplitterWindow* m_splitter{nullptr};
    wxListBox* m_contactsList{nullptr};
    wxRichTextCtrl* m_chatLog{nullptr};
    wxTextCtrl* m_addContactInput{nullptr};
    wxTextCtrl* m_peerIdInput{nullptr};
    wxTextCtrl* m_messageInput{nullptr};
    wxTextCtrl* m_bootstrapHostInput{nullptr};
    wxTextCtrl* m_bootstrapPortInput{nullptr};
    wxButton* m_addContactButton{nullptr};
    wxButton* m_bootstrapButton{nullptr};
    wxButton* m_sendButton{nullptr};
    wxButton* m_identityButton{nullptr};
};
