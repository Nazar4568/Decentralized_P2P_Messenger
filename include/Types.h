//
// Created by Roman Martakov 
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
    std::string publicKey;   // base64
    std::string mailboxKey;  // DHT key
    std::string tcpEndpoint; // ip:port
};

struct EncryptedPacket {
    int         version    = 1;
    std::string messageId;
    std::string senderId;
    std::string receiverId;
    long long   timestamp  = 0;
    std::string nonce;       // base64
    std::string ciphertext;  // base64
};

struct ChatMessage {
    std::string messageId;
    std::string senderId;
    std::string receiverId;
    std::string plaintext;
    long long   timestamp = 0;
    bool        incoming  = false;
};
//

#ifndef P2P_MESSENGER_TYPES_H
#define P2P_MESSENGER_TYPES_H

#endif //P2P_MESSENGER_TYPES_H
