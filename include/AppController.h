#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class wxEvtHandler;
class P2PNode;

/**
 * Business-logic facade between the wxWidgets UI and the P2P network layer.
 * The UI must talk only to AppController — never to P2PNode directly.
 */
class AppController {
public:
    explicit AppController(std::shared_ptr<P2PNode> node);

    void bindUiTarget(wxEvtHandler* target);

    void configure(const std::string& peerId, uint16_t port);
    void start(const std::string& bootstrapHost = "127.0.0.1",
               const std::string& bootstrapPort = "0");
    void stop();

    void setBootstrapHost(const std::string& host);
    void bootstrap(const std::string& host, const std::string& port);
    void sendMessage(const std::string& toPeerId, const std::string& text);
    void addContact(const std::string& peerId);

    /// Restart the node under a new Peer ID (new identity + inbox + profile).
    /// Blocking (stops then starts the DHT); call from a worker thread.
    void changePeerId(const std::string& newPeerId,
                      const std::string& bootstrapHost,
                      const std::string& bootstrapPort);

    std::string localPeerId() const;
    uint16_t localPort() const;
    std::string bootstrapHost() const;
    bool isRunning() const;

    /// Base64 local public key for the export feature (empty until ready).
    std::string localPublicKeyBase64() const;

    std::vector<std::string> contacts() const;

private:
    void recordContact(const std::string& peerId);

    std::shared_ptr<P2PNode> m_node;

    mutable std::mutex m_contactsMutex;
    std::vector<std::string> m_contacts;

    std::string m_peerId;
    uint16_t m_port{4222};
    bool m_running{false};
};
