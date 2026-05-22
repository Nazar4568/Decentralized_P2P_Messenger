#include "../include/MainWindow.h"
#include "../include/P2PNode.h"

#include <memory>

#include <wx/wx.h>

class MyApp : public wxApp {
public:
    bool OnInit() override
    {
        m_node = std::make_shared<P2PNode>();

        auto* mainWindow = new MainWindow(m_node);
        SetTopWindow(mainWindow);
        m_node->startNode();
        mainWindow->Show(true);
        return true;
    }

private:
    std::shared_ptr<IP2PNode> m_node;
};

wxIMPLEMENT_APP(MyApp);
