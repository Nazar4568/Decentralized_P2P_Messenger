#pragma once
#include "Types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class MessageStorage {
public:
    // Pass path to storage file e.g. "data/messages.json"
    explicit MessageStorage(const std::string& filePath);

    // Save a single message — silently skips duplicates
    void saveMessage(const ChatMessage& message);

    // Load all messages exchanged with a specific contact
    std::vector<ChatMessage> loadHistory(const std::string& contactId);

    // Load every stored message
    std::vector<ChatMessage> loadAll();

    // Sprint 2: duplicate detection by messageId
    bool messageExists(const std::string& messageId);

private:
    std::string filePath_;

    nlohmann::json readFile();
    void           writeFile(const nlohmann::json& data);
};
