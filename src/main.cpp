#include "../include/MainWindow.h"
#include "../include/P2PNode.h"
#include <iostream>
#include <string>
#include <memory>

#include <wx/wx.h>
/*
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
*/


int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: ./main <my_id> <my_port> <target_id> <boot_port>\n";
        return 1;
    }

    std::string myId = argv[1];
    uint16_t myPort = std::stoi(argv[2]);
    std::string targetId = argv[3];
    std::string bootPort = argv[4];

    P2PNode node;
    node.setNodeConfig(myPort, myId);

    node.startNode();

    if (bootPort != "0") {
        node.bootstrap("127.0.0.1", bootPort);
    }

    std::cout << "Type a message and press Enter to send to " << targetId << ":\n";
    std::string text;
    while (std::getline(std::cin, text)) {
        if (text == "exit") break;

        EncryptedPacket dummyPacket;
        dummyPacket.senderId = myId;
        dummyPacket.ciphertext = text;

        node.sendPacket(targetId, dummyPacket);
    }

    return 0;
}