#include "../include/AppController.h"
#include "../include/P2PNode.h"

#include <algorithm>
#include <cctype>

AppController::AppController(std::shared_ptr<P2PNode> node)
    : m_node(std::move(node))
{
}

void AppController::bindUiTarget(wxEvtHandler* target)
{
    m_node->bindUiTarget(target);
}

void AppController::configure(const std::string& peerId, uint16_t port)
{
    if (peerId.empty()) {
        m_peerId = "user1";
    } else {
        m_peerId = peerId;
    }
    m_port = port;
    m_node->setNodeConfig(port, m_peerId);
}

void AppController::start(const std::string& bootstrapHost, const std::string& bootstrapPort)
{
    if (m_running) {
        return;
    }

    m_node->startNode();

    UserProfile profile;
    profile.userId = m_peerId;
    profile.displayName = m_peerId;
    profile.tcpEndpoint = "127.0.0.1:" + std::to_string(m_port);
    m_node->publishProfile(profile);

    if (bootstrapPort != "0" && !bootstrapPort.empty()) {
        bootstrap(bootstrapHost, bootstrapPort);
        // DHT may not be reachable on the first publish attempt; retry after bootstrap.
        m_node->publishProfile(profile);
    }

    m_running = true;
}

void AppController::stop()
{
    if (!m_running) {
        return;
    }

    m_node->stopNode();
    m_running = false;
}

void AppController::bootstrap(const std::string& host, const std::string& port)
{
    m_node->bootstrap(host, port);
}

void AppController::sendMessage(const std::string& toPeerId, const std::string& text)
{
    std::string recipient = toPeerId;
    recipient.erase(recipient.begin(),
                    std::find_if(recipient.begin(), recipient.end(),
                                 [](unsigned char ch) { return !std::isspace(ch); }));
    recipient.erase(
        std::find_if(recipient.rbegin(), recipient.rend(),
                     [](unsigned char ch) { return !std::isspace(ch); })
            .base(),
        recipient.end());

    if (recipient.empty()) {
        return;
    }

    m_node->sendMessage(recipient, text);
}

void AppController::addContact(const std::string& peerId)
{
    if (peerId.empty()) {
        return;
    }

    recordContact(peerId);
    m_node->findPeer(peerId);
}

void AppController::recordContact(const std::string& peerId)
{
    std::lock_guard<std::mutex> lock(m_contactsMutex);
    const auto it = std::find(m_contacts.begin(), m_contacts.end(), peerId);
    if (it == m_contacts.end()) {
        m_contacts.push_back(peerId);
    }
}

std::string AppController::localPeerId() const
{
    return m_peerId;
}

uint16_t AppController::localPort() const
{
    return m_port;
}

bool AppController::isRunning() const
{
    return m_running;
}

std::string AppController::localPublicKeyBase64() const
{
    return m_node->localPublicKeyBase64();
}

std::vector<std::string> AppController::contacts() const
{
    std::lock_guard<std::mutex> lock(m_contactsMutex);
    return m_contacts;
}
