#include "../include/MainWindow.h"
#include "../include/AboutDialog.h"
#include "../include/ExportIdentityDialog.h"
#include "../include/P2PWxEvents.h"

#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>

#include <thread>
#include <utility>

namespace {

// Outgoing bubble: right-aligned, light blue background, dark blue text.
// Change SetBackgroundColour / SetTextColour to tune the "sent" appearance.
const wxColour kOutgoingBg(220, 235, 255);
const wxColour kOutgoingFg(20, 60, 140);

// Incoming bubble: left-aligned, light green background, dark green text.
// Change these constants to restyle received messages.
const wxColour kIncomingBg(232, 245, 233);
const wxColour kIncomingFg(27, 94, 32);

const wxColour kSystemFg(100, 100, 100);
const wxColour kTimestampFg(130, 130, 130);

enum {
    ID_EXPORT_IDENTITY = wxID_HIGHEST + 100,
    ID_CHANGE_PEER_ID,
};

} // namespace

MainWindow::MainWindow(std::shared_ptr<AppController> controller,
                       const std::string& peerId,
                       uint16_t port,
                       const std::string& bootstrapHost,
                       const std::string& bootstrapPort)
    : wxFrame(nullptr, wxID_ANY, wxT("Decentralized P2P Messenger"),
              wxDefaultPosition, wxSize(960, 640))
    , m_controller(std::move(controller))
    , m_bootstrapHost(bootstrapHost)
    , m_bootstrapPort(bootstrapPort)
{
    CreateStatusBar(1);
    GetStatusBar()->SetStatusText(wxT("Ready"));

    BuildMenuBar();
    BuildUi();
    WireP2PEvents();

    m_controller->bindUiTarget(this);
    m_controller->configure(peerId, port);
    // Seed the bootstrap host (from P2P_BOOTSTRAP_HOST) so the initial start and
    // any later restart dial the right peer instead of defaulting to loopback.
    m_controller->setBootstrapHost(m_bootstrapHost);

    UpdateNetworkStatus(wxString::Format(
        wxT("Peer: %s | Port: %u | Status: Starting..."),
        wxString::FromUTF8(peerId.c_str(), static_cast<int>(peerId.size())),
        port));

    // Start the network OFF the GUI thread: identity (RSA) generation and DHT
    // bring-up are slow and would otherwise freeze the window on launch.
    // All status/peer/message updates flow back via wxQueueEvent (thread-safe).
    auto controllerRef = m_controller;
    const std::string bootstrapHostValue = m_controller->bootstrapHost();
    const std::string bootstrapPortValue = m_bootstrapPort;
    RunNetworkTaskAsync([controllerRef, bootstrapHostValue, bootstrapPortValue]() {
        controllerRef->start(bootstrapHostValue, bootstrapPortValue);
    });
}

void MainWindow::RunNetworkTaskAsync(std::function<void()> task)
{
    // Ensure the previous network task's thread object is joined before reuse
    // (initial start has normally finished long before a restart is requested).
    if (m_networkThread.joinable()) {
        m_networkThread.join();
    }
    m_networkThread = std::thread(std::move(task));
}

void MainWindow::BuildMenuBar()
{
    auto* menuBar = new wxMenuBar();

    auto* fileMenu = new wxMenu();
    fileMenu->Append(ID_CHANGE_PEER_ID, wxT("&Change Peer ID…\tCtrl+I"),
                     wxT("Restart the node under a new Peer ID / identity"));
    fileMenu->Append(ID_EXPORT_IDENTITY, wxT("&Export Identity…\tCtrl+E"),
                     wxT("Copy or save your User ID and public key"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, wxT("E&xit\tCtrl+Q"));

    auto* helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, wxT("&About…"),
                     wxT("About this project"));

    menuBar->Append(fileMenu, wxT("&File"));
    menuBar->Append(helpMenu, wxT("&Help"));

    SetMenuBar(menuBar);
}

void MainWindow::BuildUi()
{
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    m_statusText = new wxStaticText(this, wxID_ANY, wxT("Network status: Initializing..."));
    rootSizer->Add(m_statusText, 0, wxEXPAND | wxALL, 8);

    auto* bootstrapRow = new wxBoxSizer(wxHORIZONTAL);
    bootstrapRow->Add(new wxStaticText(this, wxID_ANY, wxT("Bootstrap host:")),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_bootstrapHostInput = new wxTextCtrl(this, wxID_ANY,
                                          wxString::FromUTF8(m_bootstrapHost.c_str(),
                                                             static_cast<int>(m_bootstrapHost.size())));
    m_bootstrapHostInput->SetMinSize(wxSize(140, -1));
    bootstrapRow->Add(m_bootstrapHostInput, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    bootstrapRow->Add(new wxStaticText(this, wxID_ANY, wxT("Bootstrap port:")),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_bootstrapPortInput = new wxTextCtrl(this, wxID_ANY,
                                          wxString::FromUTF8(m_bootstrapPort.c_str(),
                                                             static_cast<int>(m_bootstrapPort.size())));
    m_bootstrapPortInput->SetMinSize(wxSize(80, -1));
    bootstrapRow->Add(m_bootstrapPortInput, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_bootstrapButton = new wxButton(this, wxID_ANY, wxT("Connect"));
    bootstrapRow->Add(m_bootstrapButton, 0, wxALIGN_CENTER_VERTICAL);
    bootstrapRow->AddStretchSpacer();
    rootSizer->Add(bootstrapRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // wxSplitterWindow: resizable left sidebar (contacts) + right panel (active chat).
    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxSP_LIVE_UPDATE | wxSP_3D);
    m_splitter->SetMinimumPaneSize(180);

    auto* contactsPanel = new wxPanel(m_splitter);
    auto* contactsSizer = new wxBoxSizer(wxVERTICAL);

    contactsSizer->Add(new wxStaticText(contactsPanel, wxID_ANY, wxT("Contacts")),
                       0, wxBOTTOM, 4);

    auto* addContactRow = new wxBoxSizer(wxHORIZONTAL);
    m_addContactInput = new wxTextCtrl(contactsPanel, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_PROCESS_ENTER);
    addContactRow->Add(m_addContactInput, 1, wxEXPAND | wxRIGHT, 6);
    m_addContactButton = new wxButton(contactsPanel, wxID_ANY, wxT("Add Contact"));
    addContactRow->Add(m_addContactButton, 0);
    contactsSizer->Add(addContactRow, 0, wxEXPAND | wxBOTTOM, 8);

    m_contactsList = new wxListBox(contactsPanel, wxID_ANY);
    contactsSizer->Add(m_contactsList, 1, wxEXPAND);

    m_identityButton = new wxButton(contactsPanel, wxID_ANY,
                                    wxT("My Identity / Export"));
    contactsSizer->Add(m_identityButton, 0, wxEXPAND | wxTOP, 8);

    contactsPanel->SetSizer(contactsSizer);

    auto* chatPanel = new wxPanel(m_splitter);
    auto* chatSizer = new wxBoxSizer(wxVERTICAL);

    chatSizer->Add(new wxStaticText(chatPanel, wxID_ANY, wxT("Chat")),
                   0, wxBOTTOM, 4);

    m_chatLog = new wxRichTextCtrl(chatPanel, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_READONLY | wxTE_MULTILINE | wxBORDER_NONE);
    m_chatLog->SetMinSize(wxSize(400, 300));
    chatSizer->Add(m_chatLog, 1, wxEXPAND | wxBOTTOM, 8);

    m_messageInput = new wxTextCtrl(chatPanel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxTE_PROCESS_ENTER);
    m_messageInput->SetMinSize(wxSize(-1, 72));
    chatSizer->Add(m_messageInput, 0, wxEXPAND | wxBOTTOM, 8);

    auto* peerRow = new wxBoxSizer(wxHORIZONTAL);
    peerRow->Add(new wxStaticText(chatPanel, wxID_ANY, wxT("Send to:")),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_peerIdInput = new wxTextCtrl(chatPanel, wxID_ANY);
    peerRow->Add(m_peerIdInput, 1, wxEXPAND | wxRIGHT, 8);
    m_sendButton = new wxButton(chatPanel, wxID_ANY, wxT("Send"));
    peerRow->Add(m_sendButton, 0, wxALIGN_CENTER_VERTICAL);
    chatSizer->Add(peerRow, 0, wxEXPAND);

    chatPanel->SetSizer(chatSizer);

    m_splitter->SplitVertically(contactsPanel, chatPanel, 240);

    rootSizer->Add(m_splitter, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    SetSizer(rootSizer);
    Layout();
    Centre();
}

void MainWindow::WireP2PEvents()
{
    m_sendButton->Bind(wxEVT_BUTTON, &MainWindow::OnSendClicked, this);
    m_addContactButton->Bind(wxEVT_BUTTON, &MainWindow::OnAddContactClicked, this);
    m_bootstrapButton->Bind(wxEVT_BUTTON, &MainWindow::OnBootstrapClicked, this);
    m_addContactInput->Bind(wxEVT_TEXT_ENTER, &MainWindow::OnAddContactClicked, this);
    m_messageInput->Bind(wxEVT_TEXT_ENTER, &MainWindow::OnSendClicked, this);
    m_messageInput->Bind(wxEVT_KEY_DOWN, &MainWindow::OnMessageKeyDown, this);
    m_contactsList->Bind(wxEVT_LISTBOX, &MainWindow::OnContactSelected, this);
    m_identityButton->Bind(wxEVT_BUTTON, &MainWindow::OnExportIdentity, this);

    Bind(wxEVT_MENU, &MainWindow::OnChangePeerId, this, ID_CHANGE_PEER_ID);
    Bind(wxEVT_MENU, &MainWindow::OnExportIdentity, this, ID_EXPORT_IDENTITY);
    Bind(wxEVT_MENU, &MainWindow::OnShowAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);

    Bind(wxEVT_P2P_MESSAGE_RECEIVED, &MainWindow::OnP2PMessageReceived, this);
    Bind(wxEVT_P2P_PEER_FOUND, &MainWindow::OnP2PPeerFound, this);
    Bind(wxEVT_P2P_NETWORK_STATUS, &MainWindow::OnP2PNetworkStatus, this);
    Bind(wxEVT_P2P_ERROR, &MainWindow::OnP2PError, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnClose, this);
}

wxString MainWindow::CurrentTimestamp() const
{
    return wxDateTime::Now().Format(wxT("%H:%M:%S"));
}

void MainWindow::AppendChatMessage(ChatMessageKind kind,
                                   const wxString& peerLabel,
                                   const wxString& text,
                                   const wxString& timestamp)
{
    const wxString ts = timestamp.IsEmpty() ? CurrentTimestamp() : timestamp;

    wxRichTextAttr textAttr;
    wxTextAttrAlignment alignment = wxTEXT_ALIGNMENT_LEFT;

    if (kind == ChatMessageKind::Outgoing) {
        // Right-aligned outgoing block — customize kOutgoingBg / kOutgoingFg above.
        alignment = wxTEXT_ALIGNMENT_RIGHT;
        textAttr.SetBackgroundColour(kOutgoingBg);
        textAttr.SetTextColour(kOutgoingFg);
    } else if (kind == ChatMessageKind::Incoming) {
        // Left-aligned incoming block — customize kIncomingBg / kIncomingFg above.
        alignment = wxTEXT_ALIGNMENT_LEFT;
        textAttr.SetBackgroundColour(kIncomingBg);
        textAttr.SetTextColour(kIncomingFg);
    } else {
        alignment = wxTEXT_ALIGNMENT_CENTRE;
        textAttr.SetTextColour(kSystemFg);
        textAttr.SetFontStyle(wxFONTSTYLE_ITALIC);
    }

    m_chatLog->BeginSuppressUndo();
    m_chatLog->BeginAlignment(alignment);

    wxRichTextAttr metaAttr;
    metaAttr.SetTextColour(kTimestampFg);
    metaAttr.SetFontSize(8);
    m_chatLog->BeginStyle(metaAttr);
    m_chatLog->WriteText(ts + wxT("  "));
    m_chatLog->EndStyle();

    m_chatLog->BeginStyle(textAttr);
    if (kind == ChatMessageKind::Outgoing) {
        m_chatLog->WriteText(wxT("[you -> ") + peerLabel + wxT("] ") + text);
    } else if (kind == ChatMessageKind::Incoming) {
        m_chatLog->WriteText(wxT("[") + peerLabel + wxT(" -> you] ") + text);
    } else {
        m_chatLog->WriteText(text);
    }
    m_chatLog->EndStyle();

    m_chatLog->EndAlignment();
    m_chatLog->Newline();
    m_chatLog->EndSuppressUndo();

    m_chatLog->ShowPosition(m_chatLog->GetLastPosition());
}

void MainWindow::AppendSystemLine(const wxString& text)
{
    AppendChatMessage(ChatMessageKind::System, wxEmptyString, text);
}

void MainWindow::SendCurrentMessage()
{
    wxString peerId = m_peerIdInput->GetValue().Trim(true).Trim(false);
    const wxString text = m_messageInput->GetValue();

    if (peerId.IsEmpty() || text.IsEmpty()) {
        AppendSystemLine(wxT("[system] Enter a recipient and message text."));
        GetStatusBar()->SetStatusText(wxT("Cannot send: missing recipient or message."));
        return;
    }

    const std::string recipient = std::string(peerId.utf8_str());
    m_controller->sendMessage(recipient, std::string(text.utf8_str()));

    AddContactToList(peerId);
    // Make the recipient the active conversation (re-renders prior history),
    // then append the outgoing line into that thread's history.
    SwitchActiveContact(recipient);
    RecordMessage(recipient, ChatMessageKind::Outgoing, peerId, text);

    m_messageInput->Clear();
    GetStatusBar()->SetStatusText(wxT("Message sent to ") + peerId);
}

void MainWindow::OnSendClicked(wxCommandEvent& /*event*/)
{
    SendCurrentMessage();
}

void MainWindow::OnMessageKeyDown(wxKeyEvent& event)
{
    const int key = event.GetKeyCode();
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
        if (event.ShiftDown()) {
            event.Skip(); // Shift+Enter: default behaviour inserts a newline.
            return;
        }
        SendCurrentMessage();
        return;
    }
    event.Skip();
}

void MainWindow::OnAddContactClicked(wxCommandEvent& /*event*/)
{
    const wxString peerId = m_addContactInput->GetValue().Trim();
    if (peerId.IsEmpty()) {
        AppendSystemLine(wxT("[system] Enter a peer ID to add."));
        return;
    }

    m_controller->addContact(std::string(peerId.utf8_str()));
    AddContactToList(peerId);
    m_peerIdInput->SetValue(peerId);
    m_addContactInput->Clear();
    AppendSystemLine(wxT("[system] Looking up peer: ") + peerId);
}

void MainWindow::OnBootstrapClicked(wxCommandEvent& /*event*/)
{
    wxString host = m_bootstrapHostInput->GetValue().Trim(true).Trim(false);
    const wxString port = m_bootstrapPortInput->GetValue().Trim();
    if (host.IsEmpty()) {
        host = wxT("127.0.0.1");
    }
    if (port.IsEmpty() || port == wxT("0")) {
        AppendSystemLine(wxT("[system] Enter a bootstrap port (not 0)."));
        return;
    }

    m_controller->bootstrap(std::string(host.utf8_str()), std::string(port.utf8_str()));
    AppendSystemLine(wxT("[system] Bootstrapping to ") + host + wxT(":") + port);
}

void MainWindow::OnContactSelected(wxCommandEvent& /*event*/)
{
    const int selection = m_contactsList->GetSelection();
    if (selection == wxNOT_FOUND) {
        return;
    }

    const wxString contactId = m_contactsList->GetString(selection);
    m_peerIdInput->SetValue(contactId);

    // SwitchActiveContact re-renders the stored thread and ignores re-selecting
    // the already-active contact, so the visible conversation is never wiped.
    SwitchActiveContact(std::string(contactId.utf8_str()));
}

void MainWindow::RecordMessage(const std::string& contactId, ChatMessageKind kind,
                               const wxString& peer, const wxString& text)
{
    if (contactId.empty()) {
        return;
    }

    ChatEntry entry{kind, peer, text, CurrentTimestamp()};
    m_history[contactId].push_back(entry);

    // Only paint it now if this contact's thread is the one on screen; otherwise
    // it stays in history and appears when the user switches to that contact.
    if (contactId == m_activeContactId) {
        AppendChatMessage(entry.kind, entry.peer, entry.text, entry.timestamp);
    }
}

void MainWindow::RenderHistory(const std::string& contactId)
{
    m_chatLog->Clear();
    AppendSystemLine(wxString::Format(
        wxT("[system] Conversation with %s"),
        wxString::FromUTF8(contactId.c_str(), static_cast<int>(contactId.size()))));
    AppendSystemLine(wxT("[system] Messages are end-to-end encrypted."));

    const auto it = m_history.find(contactId);
    if (it != m_history.end()) {
        for (const auto& entry : it->second) {
            AppendChatMessage(entry.kind, entry.peer, entry.text, entry.timestamp);
        }
    }
}

void MainWindow::SwitchActiveContact(const std::string& contactId)
{
    if (contactId.empty() || contactId == m_activeContactId) {
        return;
    }
    m_activeContactId = contactId;
    RenderHistory(contactId);
}

void MainWindow::OnExportIdentity(wxCommandEvent& /*event*/)
{
    const std::string userId = m_controller->localPeerId();
    const std::string publicKey = m_controller->localPublicKeyBase64();

    ExportIdentityDialog dialog(
        this,
        wxString::FromUTF8(userId.c_str(), static_cast<int>(userId.size())),
        wxString::FromUTF8(publicKey.c_str(), static_cast<int>(publicKey.size())));
    dialog.ShowModal();
}

void MainWindow::OnChangePeerId(wxCommandEvent& /*event*/)
{
    if (m_shuttingDown) {
        return;
    }

    const std::string currentIdStd = m_controller->localPeerId();
    const wxString currentId = wxString::FromUTF8(currentIdStd.c_str(),
                                                  static_cast<int>(currentIdStd.size()));

    wxTextEntryDialog dialog(this,
                             wxT("Enter a new Peer ID.\n\n"
                                 "This restarts networking and generates a new identity\n"
                                 "(new keys, inbox and published profile)."),
                             wxT("Change Peer ID"),
                             currentId);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString newId = dialog.GetValue().Trim(true).Trim(false);
    if (newId.IsEmpty()) {
        wxMessageBox(wxT("Peer ID cannot be empty."), wxT("Change Peer ID"),
                     wxOK | wxICON_WARNING, this);
        return;
    }
    if (newId == currentId) {
        return; // No change requested.
    }

    const std::string newIdStd = std::string(newId.utf8_str());

    // Reset the on-screen conversation: the local identity is changing, so the
    // active thread no longer reflects who "you" are. Stored per-contact history
    // is kept (those are remote peers and remain valid).
    m_activeContactId.clear();
    m_chatLog->Clear();
    AppendSystemLine(wxT("[system] Restarting network as ") + newId + wxT(" …"));
    GetStatusBar()->SetStatusText(wxT("Changing Peer ID to ") + newId + wxT("…"));

    auto controllerRef = m_controller;
    const std::string bootstrapHost = m_controller->bootstrapHost();
    const std::string bootstrapPortValue = m_bootstrapPort;
    RunNetworkTaskAsync([controllerRef, newIdStd, bootstrapHost, bootstrapPortValue]() {
        controllerRef->changePeerId(newIdStd, bootstrapHost, bootstrapPortValue);
    });
}

void MainWindow::OnShowAbout(wxCommandEvent& /*event*/)
{
    AboutDialog dialog(this);
    dialog.ShowModal();
}

void MainWindow::OnExit(wxCommandEvent& /*event*/)
{
    Close();
}

void MainWindow::OnClose(wxCloseEvent& event)
{
    if (m_shuttingDown) {
        // A shutdown is already in progress; ignore repeated close requests.
        event.Veto();
        return;
    }
    m_shuttingDown = true;

    GetStatusBar()->SetStatusText(wxT("Shutting down…"));
    Hide(); // Give the user an instant "closed" feel; the event loop keeps running.

    // Run the blocking shutdown (thread joins + DHT teardown) OFF the GUI thread,
    // then destroy the frame back on the GUI thread via CallAfter.
    std::thread([this]() {
        if (m_networkThread.joinable()) {
            m_networkThread.join();
        }
        m_controller->stop();
        CallAfter([this]() { Destroy(); });
    }).detach();

    event.Veto(); // We will Destroy() ourselves once shutdown completes.
}

void MainWindow::OnP2PMessageReceived(wxThreadEvent& event)
{
    wxString fromPeerId;
    wxString text;
    ParseMessageReceivedEvent(event, fromPeerId, text);

    wxString sender = fromPeerId.Trim();
    if (sender.IsEmpty() && !text.IsEmpty()) {
        // Fallback when legacy payloads omit the sender prefix.
        sender = m_peerIdInput->GetValue().Trim();
    }
    if (sender.IsEmpty()) {
        AppendSystemLine(wxT("[system] Received a message with an unknown sender."));
        return;
    }

    AddContactToList(sender);

    const std::string senderStd = std::string(sender.utf8_str());

    // If no conversation is open yet, surface the sender's thread automatically.
    if (m_activeContactId.empty()) {
        SwitchActiveContact(senderStd);
    }

    // Store under the sender; RecordMessage paints it only if that thread is
    // currently on screen (otherwise it waits in history).
    RecordMessage(senderStd, ChatMessageKind::Incoming, sender, text);

    if (m_activeContactId != senderStd) {
        GetStatusBar()->SetStatusText(wxT("New message from ") + sender);
    }
}

void MainWindow::OnP2PPeerFound(wxThreadEvent& event)
{
    const wxString peerId = event.GetString();
    AddContactToList(peerId);
    AppendSystemLine(wxT("[system] Peer found on DHT: ") + peerId);
}

void MainWindow::OnP2PNetworkStatus(wxThreadEvent& event)
{
    UpdateNetworkStatus(event.GetString());
}

void MainWindow::OnP2PError(wxThreadEvent& event)
{
    wxString code;
    wxString message;
    ParseP2PErrorEvent(event, code, message);
    ShowError(code, message);
}

void MainWindow::ShowError(const wxString& code, const wxString& message)
{
    AppendSystemLine(wxT("[error] ") + message);
    GetStatusBar()->SetStatusText(message);

    if (code == wxString::FromUTF8(P2PErrorCode::DecryptionFailed) ||
        code == wxString::FromUTF8(P2PErrorCode::NetworkUnavailable)) {
        wxMessageBox(message, wxT("P2P Error"), wxOK | wxICON_ERROR, this);
    } else if (code == wxString::FromUTF8(P2PErrorCode::PeerNotFound) ||
               code == wxString::FromUTF8(P2PErrorCode::DeliveryFailed)) {
        wxMessageBox(message, wxT("P2P Warning"), wxOK | wxICON_WARNING, this);
    }
}

void MainWindow::AddContactToList(const wxString& peerId)
{
    if (peerId.IsEmpty()) {
        return;
    }

    if (m_contactsList->FindString(peerId) != wxNOT_FOUND) {
        return;
    }

    m_contactsList->Append(peerId);
}

void MainWindow::UpdateNetworkStatus(const wxString& status)
{
    const wxString peerId = wxString::FromUTF8(m_controller->localPeerId().c_str(),
                                               static_cast<int>(m_controller->localPeerId().size()));
    m_statusText->SetLabel(wxString::Format(
        wxT("Peer: %s | Port: %u | %s"),
        peerId,
        m_controller->localPort(),
        status));
}
