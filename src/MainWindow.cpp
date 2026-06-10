#include "../include/MainWindow.h"
#include "../include/P2PWxEvents.h"

#include <wx/sizer.h>

MainWindow::MainWindow(std::shared_ptr<AppController> controller,
                       const std::string& peerId,
                       uint16_t port,
                       const std::string& bootstrapPort)
    : wxFrame(nullptr, wxID_ANY, wxT("Decentralized P2P Messenger — Sprint 2"),
              wxDefaultPosition, wxSize(900, 600))
    , m_controller(std::move(controller))
    , m_bootstrapPort(bootstrapPort)
{
    BuildUi();
    WireP2PEvents();

    m_controller->bindUiTarget(this);
    m_controller->configure(peerId, port);
    m_controller->start("127.0.0.1", m_bootstrapPort);

    UpdateNetworkStatus(wxString::Format(
        wxT("Peer: %s | Port: %u | Status: Starting..."),
        wxString::FromUTF8(peerId.c_str(), static_cast<int>(peerId.size())),
        port));
}

void MainWindow::BuildUi()
{
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    m_statusText = new wxStaticText(this, wxID_ANY, wxT("Network status: Initializing..."));
    rootSizer->Add(m_statusText, 0, wxEXPAND | wxALL, 8);

    auto* bootstrapRow = new wxBoxSizer(wxHORIZONTAL);
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

    auto* mainRow = new wxBoxSizer(wxHORIZONTAL);

    auto* contactsPanel = new wxBoxSizer(wxVERTICAL);
    contactsPanel->Add(new wxStaticText(this, wxID_ANY, wxT("Contacts")),
                       0, wxBOTTOM, 4);
    m_contactsList = new wxListBox(this, wxID_ANY);
    contactsPanel->Add(m_contactsList, 1, wxEXPAND | wxBOTTOM, 8);

    auto* addContactRow = new wxBoxSizer(wxHORIZONTAL);
    m_addContactInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_PROCESS_ENTER);
    addContactRow->Add(m_addContactInput, 1, wxEXPAND | wxRIGHT, 6);
    m_addContactButton = new wxButton(this, wxID_ANY, wxT("Add"));
    addContactRow->Add(m_addContactButton, 0);
    contactsPanel->Add(addContactRow, 0, wxEXPAND);

    mainRow->Add(contactsPanel, 1, wxEXPAND | wxRIGHT, 8);

    auto* chatPanel = new wxBoxSizer(wxVERTICAL);
    chatPanel->Add(new wxStaticText(this, wxID_ANY, wxT("Chat")),
                    0, wxBOTTOM, 4);
    m_chatLog = new wxListBox(this, wxID_ANY);
    chatPanel->Add(m_chatLog, 1, wxEXPAND);
    mainRow->Add(chatPanel, 2, wxEXPAND);

    rootSizer->Add(mainRow, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    auto* peerRow = new wxBoxSizer(wxHORIZONTAL);
    peerRow->Add(new wxStaticText(this, wxID_ANY, wxT("Send to:")),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_peerIdInput = new wxTextCtrl(this, wxID_ANY);
    peerRow->Add(m_peerIdInput, 1, wxEXPAND);
    rootSizer->Add(peerRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    m_messageInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxTE_MULTILINE);
    m_messageInput->SetMinSize(wxSize(-1, 80));
    rootSizer->Add(m_messageInput, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* actionRow = new wxBoxSizer(wxHORIZONTAL);
    actionRow->AddStretchSpacer();
    m_sendButton = new wxButton(this, wxID_ANY, wxT("Send"));
    actionRow->Add(m_sendButton, 0, wxALIGN_CENTER_VERTICAL);
    rootSizer->Add(actionRow, 0, wxEXPAND | wxALL, 8);

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
    m_contactsList->Bind(wxEVT_LISTBOX, &MainWindow::OnContactSelected, this);

    Bind(wxEVT_P2P_MESSAGE_RECEIVED, &MainWindow::OnP2PMessageReceived, this);
    Bind(wxEVT_P2P_PEER_FOUND, &MainWindow::OnP2PPeerFound, this);
    Bind(wxEVT_P2P_NETWORK_STATUS, &MainWindow::OnP2PNetworkStatus, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnClose, this);
}

void MainWindow::OnSendClicked(wxCommandEvent& /*event*/)
{
    const wxString peerId = m_peerIdInput->GetValue().Trim();
    const wxString text = m_messageInput->GetValue();

    if (peerId.IsEmpty() || text.IsEmpty()) {
        AppendChatLine(wxT("[system] Enter a recipient and message text."));
        return;
    }

    m_controller->sendMessage(std::string(peerId.utf8_str()), std::string(text.utf8_str()));

    AppendChatLine(wxT("[you -> ") + peerId + wxT("] ") + text);
    m_messageInput->Clear();
}

void MainWindow::OnAddContactClicked(wxCommandEvent& /*event*/)
{
    const wxString peerId = m_addContactInput->GetValue().Trim();
    if (peerId.IsEmpty()) {
        AppendChatLine(wxT("[system] Enter a peer ID to add."));
        return;
    }

    m_controller->addContact(std::string(peerId.utf8_str()));
    AddContactToList(peerId);
    m_peerIdInput->SetValue(peerId);
    m_addContactInput->Clear();
    AppendChatLine(wxT("[system] Looking up peer: ") + peerId);
}

void MainWindow::OnBootstrapClicked(wxCommandEvent& /*event*/)
{
    const wxString port = m_bootstrapPortInput->GetValue().Trim();
    if (port.IsEmpty() || port == wxT("0")) {
        AppendChatLine(wxT("[system] Enter a bootstrap port (not 0)."));
        return;
    }

    m_controller->bootstrap("127.0.0.1", std::string(port.utf8_str()));
    AppendChatLine(wxT("[system] Bootstrapping to 127.0.0.1:") + port);
}

void MainWindow::OnContactSelected(wxCommandEvent& /*event*/)
{
    const int selection = m_contactsList->GetSelection();
    if (selection == wxNOT_FOUND) {
        return;
    }

    m_peerIdInput->SetValue(m_contactsList->GetString(selection));
}

void MainWindow::OnClose(wxCloseEvent& event)
{
    m_controller->stop();
    event.Skip();
}

void MainWindow::OnP2PMessageReceived(wxThreadEvent& event)
{
    wxString fromPeerId;
    wxString text;
    ParseMessageReceivedEvent(event, fromPeerId, text);

    AddContactToList(fromPeerId);
    const wxString sender = fromPeerId.IsEmpty() ? wxT("unknown") : fromPeerId;
    AppendChatLine(wxT("[") + sender + wxT(" -> you] ") + text);
}

void MainWindow::OnP2PPeerFound(wxThreadEvent& event)
{
    const wxString peerId = event.GetString();
    AddContactToList(peerId);
    AppendChatLine(wxT("[system] Peer found on DHT: ") + peerId);
}

void MainWindow::OnP2PNetworkStatus(wxThreadEvent& event)
{
    UpdateNetworkStatus(event.GetString());
}

void MainWindow::AppendChatLine(const wxString& line)
{
    m_chatLog->Append(line);
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
