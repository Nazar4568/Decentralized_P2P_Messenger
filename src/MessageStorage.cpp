#include "MessageStorage.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <stdexcept>

using json = nlohmann::json;

// ─────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────

MessageStorage::MessageStorage(const std::string& filePath)
    : filePath_(filePath)
{
    // Create parent directory (e.g. data/) if it doesn't exist
    std::filesystem::create_directories(
        std::filesystem::path(filePath).parent_path()
    );

    // Create empty JSON array file on first run
    if (!std::filesystem::exists(filePath_)) {
        writeFile(json::array());
    }
}

// ─────────────────────────────────────────
// Save a message
// ─────────────────────────────────────────

void MessageStorage::saveMessage(const ChatMessage& message) {
    // Sprint 2: duplicate check — never store the same messageId twice
    if (messageExists(message.messageId)) {
        return;
    }

    json all = readFile();

    json entry;
    entry["messageId"]   = message.messageId;
    entry["senderId"]    = message.senderId;
    entry["receiverId"]  = message.receiverId;
    entry["plaintext"]   = message.plaintext;
    entry["timestamp"]   = message.timestamp;
    entry["incoming"]    = message.incoming;

    all.push_back(entry);
    writeFile(all);
}

// ─────────────────────────────────────────
// Load history for one contact
// ─────────────────────────────────────────

std::vector<ChatMessage> MessageStorage::loadHistory(const std::string& contactId) {
    json all = readFile();
    std::vector<ChatMessage> result;

    for (const auto& entry : all) {
        bool isFromContact = entry["senderId"]   == contactId;
        bool isToContact   = entry["receiverId"] == contactId;

        if (isFromContact || isToContact) {
            ChatMessage msg;
            msg.messageId  = entry["messageId"].get<std::string>();
            msg.senderId   = entry["senderId"].get<std::string>();
            msg.receiverId = entry["receiverId"].get<std::string>();
            msg.plaintext  = entry["plaintext"].get<std::string>();
            msg.timestamp  = entry["timestamp"].get<long long>();
            msg.incoming   = entry["incoming"].get<bool>();
            result.push_back(msg);
        }
    }

    return result;
}

// ─────────────────────────────────────────
// Load all messages
// ─────────────────────────────────────────

std::vector<ChatMessage> MessageStorage::loadAll() {
    json all = readFile();
    std::vector<ChatMessage> result;

    for (const auto& entry : all) {
        ChatMessage msg;
        msg.messageId  = entry["messageId"].get<std::string>();
        msg.senderId   = entry["senderId"].get<std::string>();
        msg.receiverId = entry["receiverId"].get<std::string>();
        msg.plaintext  = entry["plaintext"].get<std::string>();
        msg.timestamp  = entry["timestamp"].get<long long>();
        msg.incoming   = entry["incoming"].get<bool>();
        result.push_back(msg);
    }

    return result;
}

// ─────────────────────────────────────────
// Sprint 2: Duplicate detection
// ─────────────────────────────────────────

bool MessageStorage::messageExists(const std::string& messageId) {
    json all = readFile();
    for (const auto& entry : all) {
        if (entry["messageId"].get<std::string>() == messageId) {
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────
// Internal file helpers
// ─────────────────────────────────────────

json MessageStorage::readFile() {
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        return json::array();
    }

    json data;
    try {
        file >> data;
    } catch (const json::parse_error&) {
        // Corrupted file — return empty rather than crash
        return json::array();
    }

    return data;
}

void MessageStorage::writeFile(const json& data) {
    std::ofstream file(filePath_);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to: " + filePath_);
    }
    // Pretty print so data/messages.json is human readable
    file << data.dump(4);
}
