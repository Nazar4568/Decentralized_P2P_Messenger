#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <opendht.h>

struct UserProfile;
struct EncryptedPacket;

class IP2PNode {
public:
    virtual ~IP2PNode() = default;

    virtual bool start(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual void publishProfile(const UserProfile& profile) = 0;
    virtual void findPeer(const std::string& userId) = 0;
    virtual void sendPacket(const std::string& receiverId, const EncryptedPacket& packet) = 0;

    virtual void setMessageCallback(std::function<void(EncryptedPacket)> callback) = 0;
};

class P2PNode : public IP2PNode {
public:
    P2PNode();
    ~P2PNode() override;

    bool start(uint16_t port) override;
    void stop() override;
    void publishProfile(const UserProfile& profile) override;
    void findPeer(const std::string& userId) override;
    void sendPacket(const std::string& receiverId, const EncryptedPacket& packet) override;
    void setMessageCallback(std::function<void(EncryptedPacket)> callback) override;

private:
    std::function<void(EncryptedPacket)> m_messageCallback;
    dht::DhtRunner m_dht;
};