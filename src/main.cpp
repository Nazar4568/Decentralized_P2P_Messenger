#include "../include/AppController.h"
#include "../include/MainWindow.h"
#include "../include/P2PNode.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <wx/wx.h>

namespace {

std::string EnvOrDefault(const char* name, const std::string& fallback)
{
    if (const char* value = std::getenv(name)) {
        if (*value != '\0') {
            return value;
        }
    }
    return fallback;
}

uint16_t ParsePort(const std::string& text, uint16_t fallback)
{
    try {
        const int port = std::stoi(text);
        if (port > 0 && port <= 65535) {
            return static_cast<uint16_t>(port);
        }
    } catch (...) {
    }
    return fallback;
}

} // namespace

class MyApp : public wxApp {
public:
    bool OnInit() override
    {
        const std::string peerId = EnvOrDefault("P2P_PEER_ID", "user1");
        const uint16_t port = ParsePort(EnvOrDefault("P2P_PORT", "4222"), 4222);
        const std::string bootstrapPort = EnvOrDefault("P2P_BOOTSTRAP_PORT", "0");

        auto node = std::make_shared<P2PNode>();
        auto controller = std::make_shared<AppController>(node);

        auto* mainWindow = new MainWindow(controller, peerId, port, bootstrapPort);
        SetTopWindow(mainWindow);
        mainWindow->Show(true);
        return true;
    }
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