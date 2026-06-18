# Decentralized P2P Messenger

A secure, fully decentralized peer-to-peer (P2P) instant messaging application built with Modern C++. By eliminating the need for a central server, users communicate directly with one another, maximizing privacy, preventing censorship, and guaranteeing absolute ownership of data.

## Features

- **True P2P Architecture:** Direct client-to-client network topology using socket programming; no central coordination server or cloud infrastructure.
- **End-to-End Encryption (E2EE):** All communications are cryptographically secured using hybrid encryption (Asymmetric RSA/ECC for handshakes and key exchange; Symmetric AES for high-speed message encryption).
- **Local SQLite Persistence:** Complete chat logs, contact profiles, and trusted public keys are securely cached locally inside an embedded SQLite database.
- **Cryptographic Identities:** Users are entirely identified by their public cryptographic keys rather than phone numbers or email addresses.

## Core Architecture

- **Network Layer:** Multithreaded TCP/UDP socket processing allowing asynchronous message relays and connection management.
- **Crypto Engine:** Interfaces with robust cryptographic backends (OpenSSL/Crypto++) to execute secure keypair generation, signature verification, and payload encryption.
- **Storage Layer:** Utilizes a lightweight, embedded SQLite data model to ensure local message indexing and ACID-compliant transactional safety.

## Requirements

- C++17 (or higher) compatible compiler
- CMake (v3.16+)
- SQLite3 Development Libraries
- OpenSSL Development Libraries
