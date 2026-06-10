#pragma once

#include <string>
#include <vector>

struct KeyPair {
    std::vector<unsigned char> publicKey;
    std::vector<unsigned char> privateKey;
};

struct UserProfile {
    std::string userId;
    std::string displayName;
    std::string publicKey;
    std::string mailboxKey;
    std::string tcpEndpoint;
};

struct EncryptedPacket {
    int         version   = 1;
    std::string messageId;
    std::string senderId;
    std::string receiverId;
    long long   timestamp = 0;
    std::string nonce;
    std::string ciphertext;
};

struct ChatMessage {
    std::string messageId;
    std::string senderId;
    std::string receiverId;
    std::string plaintext;
    long long   timestamp = 0;
    bool        incoming  = false;
};
