#include "../include/AboutDialog.h"

#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>

AboutDialog::AboutDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, wxT("About — Decentralized P2P Messenger"),
               wxDefaultPosition, wxSize(520, 440))
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY,
                                   wxT("Decentralized P2P Messenger"));
    wxFont titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 5);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(titleFont);
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 16);

    auto* version = new wxStaticText(this, wxID_ANY,
                                     wxT("MVP · Sprint 4 build"));
    root->Add(version, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);

    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

    const wxString description = wxT(
        "A serverless, end-to-end encrypted chat application.\n\n"
        "Peers find each other and exchange messages over a Distributed\n"
        "Hash Table (DHT) — there is no central server. Each message is\n"
        "encrypted for its recipient, so only the intended peer can read it.\n\n"
        "Highlights:\n"
        "  \u2022 Decentralized peer discovery via OpenDHT\n"
        "  \u2022 End-to-end encryption (Curve25519 + XSalsa20-Poly1305)\n"
        "  \u2022 Replay protection and message validation\n"
        "  \u2022 Responsive desktop UI — network runs off the GUI thread\n\n"
        "Technology stack:\n"
        "  \u2022 C++17\n"
        "  \u2022 wxWidgets (GUI)\n"
        "  \u2022 OpenDHT (peer-to-peer networking)\n"
        "  \u2022 libsodium (cryptography)");

    auto* body = new wxStaticText(this, wxID_ANY, description);
    root->Add(body, 1, wxEXPAND | wxALL, 16);

    auto* buttons = CreateButtonSizer(wxOK);
    if (buttons) {
        root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    SetSizer(root);
    Centre();
}
