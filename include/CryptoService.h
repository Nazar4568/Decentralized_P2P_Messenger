#pragma once

#include "Types.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class CryptoService {
public:
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
                        const std::vector<unsigned char>& senderPublicKey,
                        const std::string& myUserId);

    // Sprint 3: full semantic validation (receiver match, field formats, replay check)
    bool        validateMessage(const EncryptedPacket& packet,
                                const std::string& myUserId);

    // --- Helpers ---
    std::string                toBase64(const std::vector<unsigned char>& data);
    std::vector<unsigned char> fromBase64(const std::string& b64);
    std::string                generateMessageId();

private:
    // Replay protection: tracks the latest processed messageId per sender.
    // Key = senderId, Value = set of already-processed messageIds from that sender.
    // A std::set per sender keeps memory bounded per conversation and gives O(log n) lookup.
    std::map<std::string, std::set<std::string>> seenMessageIds_;

    // Returns true and marks the messageId as seen; false if it was already seen (replay).
    bool markSeen(const std::string& senderId, const std::string& messageId);

    // Validate UUID-like messageId format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    static bool isValidMessageId(const std::string& id);

    // Validate base64-encoded key/nonce length decodes to expectedBytes
    bool isValidBase64Blob(const std::string& b64, size_t expectedBytes);
};
