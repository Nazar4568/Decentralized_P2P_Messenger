#include "../include/MainWindow.h"
#include "../include/P2PWxEvents.h"

#include <wx/sizer.h>

// =============================================================================
// Main window constructor
// =============================================================================
// wxFrame — top-level window with a title bar and border.
// First argument (nullptr) — parent: the application main window has no parent.
// wxID_ANY — “assign an ID automatically” (alternative: explicit wxID_OK, etc.).
MainWindow::MainWindow(std::shared_ptr<IP2PNode> node)
    : wxFrame(nullptr, wxID_ANY, wxT("Decentralized P2P Messenger — Sprint 1"),
              wxDefaultPosition, wxSize(640, 480))
    , m_node(std::move(node))
{
    // Order matters: build widgets and layout first, then wire events,
    // then register this window as the wxQueueEvent target for P2PNode.
    BuildUi();
    WireP2PEvents();

    // MainWindow inherits wxFrame → wxWindow → wxEvtHandler.
    // P2PNode stores this pointer and calls wxQueueEvent(this, ...) from
    // background threads. The Bind() handlers below run on the main GUI thread —
    // the only thread where wx widgets may be modified safely.
    m_node->bindUiTarget(this);
}

// =============================================================================
// UI construction
// =============================================================================
// wxBoxSizer — layout manager: distributes space among child elements.
// wxVERTICAL — column top-to-bottom; wxHORIZONTAL — row left-to-right.
// wxEXPAND + proportion 1 on m_chatLog — the chat list takes all extra space
// when the window is resized (responsive layout).
void MainWindow::BuildUi()
{
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    // wxListBox — single-column list of strings; used as the conversation log.
    // Parent “this” is the frame: wxWidgets manages child lifetime and destroys
    // them when MainWindow is closed.
    m_chatLog = new wxListBox(this, wxID_ANY);
    rootSizer->Add(m_chatLog, 1, wxEXPAND | wxALL, 8);

    // Horizontal row: label + recipient peer ID field.
    auto* peerRow = new wxBoxSizer(wxHORIZONTAL);
    peerRow->Add(new wxStaticText(this, wxID_ANY, wxT("Peer ID:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_peerIdInput = new wxTextCtrl(this, wxID_ANY, wxT("mock-peer-alice"));
    // Proportion 1 — the input field stretches to fill the row width.
    peerRow->Add(m_peerIdInput, 1, wxEXPAND);
    rootSizer->Add(peerRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // wxTE_MULTILINE — multi-line message text entry.
    m_messageInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE);
    m_messageInput->SetMinSize(wxSize(-1, 80));
    rootSizer->Add(m_messageInput, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // Send button aligned to the right via AddStretchSpacer()
    auto* actionRow = new wxBoxSizer(wxHORIZONTAL);
    actionRow->AddStretchSpacer();
    m_sendButton = new wxButton(this, wxID_ANY, wxT("Send"));
    actionRow->Add(m_sendButton, 0, wxALIGN_CENTER_VERTICAL);
    rootSizer->Add(actionRow, 0, wxEXPAND | wxALL, 8);

    // Attach sizer to frame, recalculate geometry; Centre() — center on screen.
    SetSizer(rootSizer);
    Layout();
    Centre();
}

// =============================================================================
// Event wiring via Bind
// =============================================================================
// Bind connects an event source (button / window) to a handler method.
// Unlike EVT_* macros + DECLARE_EVENT_TABLE, Bind can be used at runtime
// and with lambdas; here we bind to MainWindow member functions.
void MainWindow::WireP2PEvents()
{
    // wxEVT_BUTTON — standard click event; handler always runs on the GUI thread.
    m_sendButton->Bind(wxEVT_BUTTON, &MainWindow::OnSendClicked, this);

    // Custom types are declared in P2PWxEvents.h (wxDECLARE_EVENT) and defined
    // in P2PNode.cpp (wxDEFINE_EVENT). P2PNode creates wxThreadEvent and enqueues
    // it on the main thread via wxQueueEvent — we only Bind here.
    //
    // Important: OnP2PMessageReceived / OnP2PPeerFound are NEVER invoked
    // directly from std::thread inside P2PNode — only from wx’s event loop.
    Bind(wxEVT_P2P_MESSAGE_RECEIVED, &MainWindow::OnP2PMessageReceived, this);
    Bind(wxEVT_P2P_PEER_FOUND, &MainWindow::OnP2PPeerFound, this);
}

// =============================================================================
// Outgoing message (user action → backend)
// =============================================================================
void MainWindow::OnSendClicked(wxCommandEvent& /*event*/)
{
    // wxString — wx’s internal string type (UTF-32/UTF-16 depending on build).
    // Trim() removes leading/trailing whitespace from the peer ID.
    const wxString peerId = m_peerIdInput->GetValue().Trim();
    const wxString text = m_messageInput->GetValue();

    if (peerId.IsEmpty() || text.IsEmpty()) {
        AppendChatLine(wxT("[system] Enter peer ID and message text."));
        return;
    }

    // IP2PNode uses std::string (UTF-8). utf8_str() returns a temporary buffer —
    // copy into std::string before leaving the handler, or the pointer becomes invalid.
    // sendMessage may continue on a network/mock thread; the UI only initiates here.
    m_node->sendMessage(std::string(peerId.utf8_str()), std::string(text.utf8_str()));

    AppendChatLine(wxT("[you -> ") + peerId + wxT("] ") + text);
    m_messageInput->Clear();
}

// =============================================================================
// Incoming P2P message (event from the GUI thread queue)
// =============================================================================
// wxThreadEvent& — event type chosen in wxDEFINE_EVENT for this ID.
// Payload is packed in P2PNode as "fromPeerId|text" (see ParseMessageReceivedEvent).
void MainWindow::OnP2PMessageReceived(wxThreadEvent& event)
{
    wxString fromPeerId;
    wxString text;
    ParseMessageReceivedEvent(event, fromPeerId, text);

    AppendChatLine(wxT("[") + fromPeerId + wxT("] ") + text);
}

// =============================================================================
// Peer discovered (mock DHT / OpenDHT in later sprints)
// =============================================================================
void MainWindow::OnP2PPeerFound(wxThreadEvent& event)
{
    const wxString peerId = event.GetString();
    AppendChatLine(wxT("[system] Peer found: ") + peerId);
}

// =============================================================================
// Single place to update the chat log
// =============================================================================
// Call only from the GUI thread (Bind handlers, OnSendClicked).
// wxListBox::Append is thread-safe only when invoked on the main thread.
void MainWindow::AppendChatLine(const wxString& line)
{
    m_chatLog->Append(line);
}
