//
// Created by Roman Martakov on 09.05.26.
// Sprint 3 additions: message validation, per-sender replay protection.
//

#include "CryptoService.h"
#include <sodium.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <random>
#include <iomanip>
#include <iostream>
#include <regex>

// ─────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────

CryptoService::CryptoService() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }
}

// ─────────────────────────────────────────
// Sprint 1 — Key generation & persistence
// ─────────────────────────────────────────

KeyPair CryptoService::generateKeyPair() {
    KeyPair kp;
    kp.publicKey.resize(crypto_box_PUBLICKEYBYTES);
    kp.privateKey.resize(crypto_box_SECRETKEYBYTES);
    crypto_box_keypair(kp.publicKey.data(), kp.privateKey.data());
    return kp;
}

bool CryptoService::saveKeyPair(const KeyPair& keyPair,
                                const std::string& pubPath,
                                const std::string& privPath) {
    // Save public key as base64
    std::ofstream pubFile(pubPath);
    if (!pubFile) return false;
    pubFile << toBase64(keyPair.publicKey);
    pubFile.close();

    // Save private key as raw bytes (binary)
    std::ofstream privFile(privPath, std::ios::binary);
    if (!privFile) return false;
    privFile.write(
        reinterpret_cast<const char*>(keyPair.privateKey.data()),
        keyPair.privateKey.size()
    );
    privFile.close();

    return true;
}

KeyPair CryptoService::loadKeyPair(const std::string& pubPath,
                                   const std::string& privPath) {
    KeyPair kp;

    // Load public key from base64
    std::ifstream pubFile(pubPath);
    if (!pubFile) throw std::runtime_error("Cannot open public key file");
    std::string b64((std::istreambuf_iterator<char>(pubFile)),
                     std::istreambuf_iterator<char>());
    kp.publicKey = fromBase64(b64);

    // Load private key from raw binary
    std::ifstream privFile(privPath, std::ios::binary);
    if (!privFile) throw std::runtime_error("Cannot open private key file");
    kp.privateKey = std::vector<unsigned char>(
        std::istreambuf_iterator<char>(privFile),
        std::istreambuf_iterator<char>()
    );

    return kp;
}

// ─────────────────────────────────────────
// Sprint 2 — Structural packet validation
// Checks presence and non-emptiness of all
// required fields plus a 5-minute time window.
// ─────────────────────────────────────────

bool CryptoService::validatePacketStructure(const EncryptedPacket& packet) {
    if (packet.messageId.empty())   return false;
    if (packet.senderId.empty())    return false;
    if (packet.receiverId.empty())  return false;
    if (packet.nonce.empty())       return false;
    if (packet.ciphertext.empty())  return false;
    if (packet.timestamp <= 0)      return false;

    // Reject packets outside a 5-minute window (guards against stale replay)
    auto now = std::chrono::system_clock::now();
    auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    if (std::abs(nowSec - packet.timestamp) > 300) return false;

    return true;
}

// ─────────────────────────────────────────
// Sprint 3 — Full semantic validation
// Validates field formats, receiver identity,
// timestamp window, and replay protection.
// ─────────────────────────────────────────

bool CryptoService::validateMessage(const EncryptedPacket& packet,
                                    const std::string& myUserId) {
    // 1. Structural presence check (reuses Sprint 2 logic)
    if (!validatePacketStructure(packet)) {
        std::cerr << "[CryptoService] validateMessage: structural check failed"
                     " for messageId=" << packet.messageId << "\n";
        return false;
    }

    // 2. Receiver ID must match this node's identity
    if (packet.receiverId != myUserId) {
        std::cerr << "[CryptoService] validateMessage: receiverId mismatch"
                     " (expected=" << myUserId
                  << " got=" << packet.receiverId << ")\n";
        return false;
    }

    // 3. messageId must match our UUID-like format: 8-4-4-4-12 lowercase hex
    if (!isValidMessageId(packet.messageId)) {
        std::cerr << "[CryptoService] validateMessage: malformed messageId="
                  << packet.messageId << "\n";
        return false;
    }

    // 4. Nonce must decode to exactly crypto_box_NONCEBYTES (24 bytes)
    if (!isValidBase64Blob(packet.nonce, crypto_box_NONCEBYTES)) {
        std::cerr << "[CryptoService] validateMessage: invalid nonce length"
                     " for messageId=" << packet.messageId << "\n";
        return false;
    }

    // 5. Sender public key stored in senderId field is not validated here
    //    (it arrives separately as a raw key vector in decrypt()); we only
    //    check that senderId is a non-empty string — already done in step 1.

    // 6. Replay protection — per-sender deduplication
    if (!markSeen(packet.senderId, packet.messageId)) {
        std::cerr << "[CryptoService] REPLAY ATTACK detected:"
                     " senderId=" << packet.senderId
                  << " messageId=" << packet.messageId << "\n";
        return false;
    }

    return true;
}

// ─────────────────────────────────────────
// Sprint 3 — Encryption
// ─────────────────────────────────────────

EncryptedPacket CryptoService::encrypt(
    const std::string& plaintext,
    const std::vector<unsigned char>& receiverPublicKey,
    const std::vector<unsigned char>& myPrivateKey,
    const std::string& senderId,
    const std::string& receiverId)
{
    // Generate a cryptographically random nonce for each message.
    // Never reuse a nonce with the same key pair.
    std::vector<unsigned char> nonce(crypto_box_NONCEBYTES);
    randombytes_buf(nonce.data(), nonce.size());

    // crypto_box_easy: Curve25519 DH + XSalsa20 + Poly1305 MAC
    std::vector<unsigned char> ciphertext(
        crypto_box_MACBYTES + plaintext.size()
    );

    int result = crypto_box_easy(
        ciphertext.data(),
        reinterpret_cast<const unsigned char*>(plaintext.data()),
        plaintext.size(),
        nonce.data(),
        receiverPublicKey.data(),
        myPrivateKey.data()
    );

    if (result != 0) {
        throw std::runtime_error("Encryption failed");
    }

    // Pack binary fields as Base64 for text-safe transmission
    EncryptedPacket packet;
    packet.version    = 1;
    packet.messageId  = generateMessageId();
    packet.senderId   = senderId;
    packet.receiverId = receiverId;
    packet.nonce      = toBase64(nonce);
    packet.ciphertext = toBase64(ciphertext);

    auto now = std::chrono::system_clock::now();
    packet.timestamp  = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    return packet;
}

// ─────────────────────────────────────────
// Sprint 3 — Decryption
// Validates the packet fully before attempting
// cryptographic decryption. A failed MAC check
// (wrong key or tampered ciphertext) is reported
// via exception without leaking plaintext.
// ─────────────────────────────────────────

std::string CryptoService::decrypt(
    const EncryptedPacket& packet,
    const std::vector<unsigned char>& myPrivateKey,
    const std::vector<unsigned char>& senderPublicKey,
    const std::string& myUserId)
{
    // Validate sender public key length (must be exactly 32 bytes for Curve25519)
    if (senderPublicKey.size() != crypto_box_PUBLICKEYBYTES) {
        throw std::runtime_error("Invalid sender public key length");
    }

    // Full semantic validation including replay check
    if (!validateMessage(packet, myUserId)) {
        throw std::runtime_error("Message validation failed — rejected");
    }

    auto nonce      = fromBase64(packet.nonce);
    auto ciphertext = fromBase64(packet.ciphertext);

    if (ciphertext.size() < crypto_box_MACBYTES) {
        throw std::runtime_error("Ciphertext too short");
    }

    std::vector<unsigned char> plaintext(
        ciphertext.size() - crypto_box_MACBYTES
    );

    // crypto_box_open_easy authenticates the MAC before decrypting.
    // A non-zero return means the message is invalid or was tampered with.
    int result = crypto_box_open_easy(
        plaintext.data(),
        ciphertext.data(),
        ciphertext.size(),
        nonce.data(),
        senderPublicKey.data(),
        myPrivateKey.data()
    );

    if (result != 0) {
        throw std::runtime_error("Decryption failed — wrong key or corrupted data");
    }

    return std::string(plaintext.begin(), plaintext.end());
}

// ─────────────────────────────────────────
// Helpers — Base64
// ─────────────────────────────────────────

std::string CryptoService::toBase64(const std::vector<unsigned char>& data) {
    size_t b64Len = sodium_base64_encoded_len(
        data.size(), sodium_base64_VARIANT_ORIGINAL
    );
    std::string b64(b64Len, '\0');
    sodium_bin2base64(
        b64.data(), b64Len,
        data.data(), data.size(),
        sodium_base64_VARIANT_ORIGINAL
    );
    // sodium_bin2base64 null-terminates; strip the trailing '\0'
    if (!b64.empty() && b64.back() == '\0') b64.pop_back();
    return b64;
}

std::vector<unsigned char> CryptoService::fromBase64(const std::string& b64) {
    std::vector<unsigned char> bin(b64.size());
    size_t binLen = 0;
    int result = sodium_base642bin(
        bin.data(), bin.size(),
        b64.data(), b64.size(),
        nullptr, &binLen, nullptr,
        sodium_base64_VARIANT_ORIGINAL
    );
    if (result != 0) {
        throw std::runtime_error("Invalid base64 string");
    }
    bin.resize(binLen);
    return bin;
}

// ─────────────────────────────────────────
// Helpers — Message ID
// Generates a UUID-like random identifier:
// xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (lowercase hex)
// ─────────────────────────────────────────

std::string CryptoService::generateMessageId() {
    std::vector<unsigned char> buf(16);
    randombytes_buf(buf.data(), buf.size());

    std::ostringstream oss;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) oss << '-';
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(buf[i]);
    }
    return oss.str();
}

// ─────────────────────────────────────────
// Private — Replay protection
// Per-sender set of seen messageIds.
// Returns false if the messageId was already
// recorded for this sender (= replay attempt).
// ─────────────────────────────────────────

bool CryptoService::markSeen(const std::string& senderId,
                             const std::string& messageId) {
    auto& senderSet = seenMessageIds_[senderId];
    // insert() returns {iterator, bool}; false means already present
    return senderSet.insert(messageId).second;
}

// ─────────────────────────────────────────
// Private — Validation helpers
// ─────────────────────────────────────────

// Expected UUID format: 8-4-4-4-12 lowercase hex digits
bool CryptoService::isValidMessageId(const std::string& id) {
    static const std::regex uuidRe(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
    );
    return std::regex_match(id, uuidRe);
}

bool CryptoService::isValidBase64Blob(const std::string& b64,
                                      size_t expectedBytes) {
    try {
        auto decoded = fromBase64(b64);
        return decoded.size() == expectedBytes;
    } catch (...) {
        return false;
    }
}
