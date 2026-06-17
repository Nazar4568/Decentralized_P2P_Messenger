#pragma once

#include "IP2PNode.h"
#include "Types.h"

#ifdef HAS_OPENDHT
#include <opendht.h>
#endif
#include "CryptoService.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "../include/Types.h"
class wxEvtHandler;

/**
 * Concrete P2P node — OpenDHT when HAS_OPENDHT is defined, mock worker otherwise.
 * Network callbacks run on background threads; UI updates are queued via wxQueueEvent.
 */
class P2PNode : public IP2PNode {
public:
    P2PNode();
    ~P2PNode() override;

    void sendMessage(const std::string& toPeerId, const std::string& text) override;
    void startNode() override;
    void stopNode() override;

    void bindUiTarget(wxEvtHandler* target) override;
    void setMessageReceivedHandler(MessageReceivedHandler handler) override;
    void setPeerFoundHandler(PeerFoundHandler handler) override;

    void setNodeConfig(uint16_t port, const std::string& id);
    void setBootstrapHost(const std::string& host);
    void bootstrap(const std::string& ip, const std::string& port);
    void publishProfile(const UserProfile& profile);
    void findPeer(const std::string& userId);
    void sendPacket(const std::string& receiverId, const EncryptedPacket& packet);

    std::string localPeerId() const { return m_myId; }
    uint16_t localPort() const { return m_port; }
    std::string bootstrapHost() const { return m_bootstrapHost; }
    bool isRunning() const { return m_running; }

    /// Base64-encoded local public key. Empty until the node has finished
    /// generating/loading its identity (safe to call from the GUI thread).
    std::string localPublicKeyBase64() const;

private:
    std::shared_ptr<CryptoService> m_crypto;
    KeyPair m_myKeys;

    // Identity is created on the network worker thread but read from the GUI
    // thread (export feature), so guard the cached base64 public key.
    mutable std::mutex m_identityMutex;
    std::string m_publicKeyB64;
    void notifyMessageReceived(const std::string& fromPeerId, const std::string& text);
    void notifyPeerFound(const std::string& peerId);
    void notifyNetworkStatus(const std::string& status);
    void notifyError(const std::string& code, const std::string& message);
    void queueMessageEventToUi(const std::string& fromPeerId, const std::string& text);
    void queuePeerFoundEventToUi(const std::string& peerId);
    void queueNetworkStatusEventToUi(const std::string& status);
    void queueErrorEventToUi(const std::string& code, const std::string& message);

#ifdef HAS_OPENDHT
    dht::DhtRunner m_dht;
    mutable std::mutex m_peerKeysMutex;
    std::unordered_map<std::string, std::vector<unsigned char>> m_peerPublicKeys;
    mutable std::mutex m_deliveredMutex;
    std::unordered_set<std::string> m_deliveredInboxKeys;

    void cachePeerPublicKey(const std::string& userId, const std::vector<unsigned char>& publicKey);
    bool tryGetCachedPeerPublicKey(const std::string& userId,
                                   std::vector<unsigned char>& outPublicKey) const;
    bool wasInboxMessageDelivered(const std::string& senderId, const std::string& messageId) const;
    void markInboxMessageDelivered(const std::string& senderId, const std::string& messageId);
    void deliverSecureInboxMessage(const std::string& senderId, const EncryptedPacket& packet);
#else
    void mockWorkerLoop();
    std::unique_ptr<std::thread> m_mockThread;
#endif

    wxEvtHandler* m_uiTarget{nullptr};

    MessageReceivedHandler m_messageHandler;
    PeerFoundHandler m_peerFoundHandler;
    std::mutex m_handlerMutex;

    uint16_t m_port{4222};
    std::string m_myId{"default_node"};
    // Host used to reach the DHT bootstrap node. Default is loopback for
    // single-machine testing; updated when bootstrap() is called with a host.
    std::string m_bootstrapHost{"127.0.0.1"};

    //MADE ATOMIC: Threads can now safely read and change this flag
    std::atomic<bool> m_running{false};

    // sprint 4:
    UserProfile m_currentProfile;                 // Remember our profile
    std::unique_ptr<std::thread> m_publishThread; // Our background "worker"
    void backgroundPublishLoop();                 // The code that the worker will execute
};
