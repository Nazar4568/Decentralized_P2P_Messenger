//
// Created by Roman Martakov on 09.05.26.
//

#include "CryptoService.h"
#include <sodium.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <CryptoService.h>
#include <random>
#include <iomanip>

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
// Sprint 2 — Packet validation
// ─────────────────────────────────────────

bool CryptoService::validatePacketStructure(const EncryptedPacket& packet) {
    if (packet.messageId.empty())   return false;
    if (packet.senderId.empty())    return false;
    if (packet.receiverId.empty())  return false;
    if (packet.nonce.empty())       return false;
    if (packet.ciphertext.empty())  return false;
    if (packet.timestamp <= 0)      return false;

    // Reject packets older than 5 minutes (replay guard)
    auto now = std::chrono::system_clock::now();
    auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    if (std::abs(nowSec - packet.timestamp) > 300) return false;

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
    // Generate random nonce
    std::vector<unsigned char> nonce(crypto_box_NONCEBYTES);
    randombytes_buf(nonce.data(), nonce.size());

    // Encrypt
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

    // Build packet
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
// ─────────────────────────────────────────

std::string CryptoService::decrypt(
    const EncryptedPacket& packet,
    const std::vector<unsigned char>& myPrivateKey,
    const std::vector<unsigned char>& senderPublicKey)
{
    // Replay protection
    if (seenMessageIds_.count(packet.messageId)) {
        throw std::runtime_error("Duplicate messageId — possible replay attack");
    }

    if (!validatePacketStructure(packet)) {
        throw std::runtime_error("Invalid packet structure");
    }

    auto nonce      = fromBase64(packet.nonce);
    auto ciphertext = fromBase64(packet.ciphertext);

    if (ciphertext.size() < crypto_box_MACBYTES) {
        throw std::runtime_error("Ciphertext too short");
    }

    std::vector<unsigned char> plaintext(
        ciphertext.size() - crypto_box_MACBYTES
    );

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

    // Mark messageId as seen
    seenMessageIds_.insert(packet.messageId);

    return std::string(plaintext.begin(), plaintext.end());
}

// ─────────────────────────────────────────
// Helpers
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
    // Remove null terminator sodium appends
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

std::string CryptoService::generateMessageId() {
    // Simple UUID-like ID using random bytes
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
