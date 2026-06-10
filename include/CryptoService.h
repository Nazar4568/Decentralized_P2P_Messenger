//
// Created by Roman Martakov on 09.05.26.
#pragma once
#include "Types.h"
#include <set>
#include <string>
#include <vector>

class CryptoService {
public:
    // Initializes libsodium — call once at startup
    CryptoService();

    // --- Sprint 1 ---
    KeyPair     generateKeyPair();
    bool        saveKeyPair(const KeyPair& keyPair,
                            const std::string& pubPath,
                            const std::string& privPath);
    KeyPair     loadKeyPair(const std::string& pubPath,
                            const std::string& privPath);

    // --- Sprint 2 ---
    bool        validatePacketStructure(const EncryptedPacket& packet);

    // --- Sprint 3 ---
    EncryptedPacket encrypt(const std::string& plaintext,
                            const std::vector<unsigned char>& receiverPublicKey,
                            const std::vector<unsigned char>& myPrivateKey,
                            const std::string& senderId,
                            const std::string& receiverId);

    std::string decrypt(const EncryptedPacket& packet,
                        const std::vector<unsigned char>& myPrivateKey,
                        const std::vector<unsigned char>& senderPublicKey);

    // --- Helpers (used internally and by other modules) ---
    std::string toBase64(const std::vector<unsigned char>& data);
    std::vector<unsigned char> fromBase64(const std::string& b64);
    std::string generateMessageId();

private:
    std::set<std::string> seenMessageIds_; // replay protection
};
//

#ifndef P2P_MESSENGER_CRYPTOSERVICE_H
#define P2P_MESSENGER_CRYPTOSERVICE_H

#endif //P2P_MESSENGER_CRYPTOSERVICE_H
