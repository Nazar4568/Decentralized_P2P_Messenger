#include "../include/P2PNode.h"
#include <iostream>

P2PNode::P2PNode() {
}

P2PNode::~P2PNode() {
    stop();
}
void P2PNode::publishProfile(const UserProfile& profile) {
}

void P2PNode::findPeer(const std::string& userId) {
}

void P2PNode::sendPacket(const std::string& receiverId, const EncryptedPacket& packet) {
}

void P2PNode::setMessageCallback(std::function<void(EncryptedPacket)> callback) {
    m_messageCallback = std::move(callback);
}

bool P2PNode::start(uint16_t port) {
    try {
        dht::crypto::Identity id = dht::crypto::generateIdentity();

        m_dht.run(port, id, true);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error running DHT " << e.what() << std::endl;
        return false;
    }
}

void P2PNode::stop() {
    m_dht.join();
}

