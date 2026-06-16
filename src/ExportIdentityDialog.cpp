#include "../include/ExportIdentityDialog.h"

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <fstream>
#include <string>

namespace {
enum {
    ID_COPY_USER_ID = wxID_HIGHEST + 200,
    ID_COPY_PUBLIC_KEY,
    ID_SAVE_TO_FILE,
};
} // namespace

ExportIdentityDialog::ExportIdentityDialog(wxWindow* parent,
                                           const wxString& userId,
                                           const wxString& publicKeyBase64)
    : wxDialog(parent, wxID_ANY, wxT("My Identity — Export / Share"),
               wxDefaultPosition, wxSize(560, 420))
    , m_userId(userId)
    , m_publicKeyBase64(publicKeyBase64)
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* intro = new wxStaticText(this, wxID_ANY,
        wxT("Share your User ID so others can add you as a contact.\n"
            "Your public key lets peers encrypt messages to you."));
    root->Add(intro, 0, wxALL, 12);

    root->Add(new wxStaticText(this, wxID_ANY, wxT("User ID:")),
              0, wxLEFT | wxRIGHT | wxTOP, 12);
    auto* userIdField = new wxTextCtrl(this, wxID_ANY, m_userId,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_READONLY);
    root->Add(userIdField, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    root->Add(new wxStaticText(this, wxID_ANY, wxT("Public key (base64):")),
              0, wxLEFT | wxRIGHT, 12);
    const wxString keyValue = m_publicKeyBase64.IsEmpty()
        ? wxT("(identity is still initializing — try again in a moment)")
        : m_publicKeyBase64;
    auto* keyField = new wxTextCtrl(this, wxID_ANY, keyValue,
                                    wxDefaultPosition, wxSize(-1, 110),
                                    wxTE_READONLY | wxTE_MULTILINE | wxTE_BESTWRAP);
    root->Add(keyField, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* actionRow = new wxBoxSizer(wxHORIZONTAL);
    auto* copyIdBtn = new wxButton(this, ID_COPY_USER_ID, wxT("Copy User ID"));
    auto* copyKeyBtn = new wxButton(this, ID_COPY_PUBLIC_KEY, wxT("Copy Public Key"));
    auto* saveBtn = new wxButton(this, ID_SAVE_TO_FILE, wxT("Save to File…"));
    actionRow->Add(copyIdBtn, 0, wxRIGHT, 8);
    actionRow->Add(copyKeyBtn, 0, wxRIGHT, 8);
    actionRow->Add(saveBtn, 0);
    root->Add(actionRow, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* closeRow = CreateButtonSizer(wxCLOSE);
    if (closeRow) {
        root->Add(closeRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    const bool hasKey = !m_publicKeyBase64.IsEmpty();
    copyKeyBtn->Enable(hasKey);
    saveBtn->Enable(hasKey);

    copyIdBtn->Bind(wxEVT_BUTTON, &ExportIdentityDialog::OnCopyUserId, this);
    copyKeyBtn->Bind(wxEVT_BUTTON, &ExportIdentityDialog::OnCopyPublicKey, this);
    saveBtn->Bind(wxEVT_BUTTON, &ExportIdentityDialog::OnSaveToFile, this);

    SetSizer(root);
    Centre();
}

void ExportIdentityDialog::CopyToClipboard(const wxString& value, const wxString& label)
{
    if (value.IsEmpty()) {
        return;
    }
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(value));
        wxTheClipboard->Close();
        wxMessageBox(label + wxT(" copied to clipboard."),
                     wxT("Copied"), wxOK | wxICON_INFORMATION, this);
    } else {
        wxMessageBox(wxT("Could not access the clipboard."),
                     wxT("Clipboard Error"), wxOK | wxICON_ERROR, this);
    }
}

void ExportIdentityDialog::OnCopyUserId(wxCommandEvent& /*event*/)
{
    CopyToClipboard(m_userId, wxT("User ID"));
}

void ExportIdentityDialog::OnCopyPublicKey(wxCommandEvent& /*event*/)
{
    CopyToClipboard(m_publicKeyBase64, wxT("Public key"));
}

void ExportIdentityDialog::OnSaveToFile(wxCommandEvent& /*event*/)
{
    const wxString defaultName = m_userId.IsEmpty()
        ? wxT("identity.txt")
        : m_userId + wxT("_identity.txt");

    wxFileDialog saveDialog(this, wxT("Export identity"), wxEmptyString, defaultName,
                            wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    const wxString path = saveDialog.GetPath();
    std::ofstream out(std::string(path.utf8_str()), std::ios::out | std::ios::trunc);
    if (!out) {
        wxMessageBox(wxT("Could not write to the selected file."),
                     wxT("Save Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    out << "Decentralized P2P Messenger — Identity\n";
    out << "User ID: " << std::string(m_userId.utf8_str()) << "\n";
    out << "Public Key (base64): " << std::string(m_publicKeyBase64.utf8_str()) << "\n";
    out.close();

    wxMessageBox(wxT("Identity saved to:\n") + path,
                 wxT("Saved"), wxOK | wxICON_INFORMATION, this);
}
