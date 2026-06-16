#pragma once

#include <wx/dialog.h>

/**
 * Modal "About" dialog describing the Decentralized P2P Messenger MVP.
 * Pure GUI-thread widget; contains no network logic.
 */
class AboutDialog : public wxDialog {
public:
    explicit AboutDialog(wxWindow* parent);
};
