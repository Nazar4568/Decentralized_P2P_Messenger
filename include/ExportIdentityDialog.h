#pragma once

#include <wx/dialog.h>
#include <wx/string.h>

class wxTextCtrl;

/**
 * Modal dialog that lets the user export/share their identity:
 *   - User ID (the handle other peers add as a contact)
 *   - Public key (base64) used to encrypt messages to this user
 *
 * Provides copy-to-clipboard and save-to-file actions. GUI-thread only.
 */
class ExportIdentityDialog : public wxDialog {
public:
    ExportIdentityDialog(wxWindow* parent,
                         const wxString& userId,
                         const wxString& publicKeyBase64);

private:
    void CopyToClipboard(const wxString& value, const wxString& label);
    void OnCopyUserId(wxCommandEvent& event);
    void OnCopyPublicKey(wxCommandEvent& event);
    void OnSaveToFile(wxCommandEvent& event);

    wxString m_userId;
    wxString m_publicKeyBase64;
};
