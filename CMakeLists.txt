cmake_minimum_required(VERSION 3.16)
project(Decentralized_P2P_Messenger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base richtext)
include(${wxWidgets_USE_FILE})

find_package(PkgConfig QUIET)

add_executable(Decentralized_P2P_Messenger
    src/main.cpp
    src/MainWindow.cpp
    src/AppController.cpp
    src/P2PNode.cpp
    src/CryptoService.cpp
    include/IP2PNode.h
    include/AppController.h
    include/P2PNode.h
    include/P2PWxEvents.h
    include/MainWindow.h
    include/Types.h
    include/CryptoService.h
)

target_include_directories(Decentralized_P2P_Messenger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Decentralized_P2P_Messenger PRIVATE ${wxWidgets_LIBRARIES})

# libsodium (CryptoService) — pkg-config first, then CMake find_path/find_library
set(SODIUM_FOUND FALSE)

if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM QUIET libsodium)
    if(SODIUM_FOUND)
        if(SODIUM_INCLUDE_DIRS)
            target_include_directories(Decentralized_P2P_Messenger PRIVATE ${SODIUM_INCLUDE_DIRS})
        endif()
        if(SODIUM_LIBRARY_DIRS)
            target_link_directories(Decentralized_P2P_Messenger PRIVATE ${SODIUM_LIBRARY_DIRS})
        endif()
        target_link_libraries(Decentralized_P2P_Messenger PRIVATE ${SODIUM_LIBRARIES})
    endif()
endif()

if(NOT SODIUM_FOUND)
    find_path(SODIUM_INCLUDE_DIR NAMES sodium.h PATH_SUFFIXES sodium)
    find_library(SODIUM_LIBRARY NAMES sodium libsodium)
    if(SODIUM_INCLUDE_DIR AND SODIUM_LIBRARY)
        set(SODIUM_FOUND TRUE)
        target_include_directories(Decentralized_P2P_Messenger PRIVATE ${SODIUM_INCLUDE_DIR})
        target_link_libraries(Decentralized_P2P_Messenger PRIVATE ${SODIUM_LIBRARY})
    endif()
endif()

if(SODIUM_FOUND)
    message(STATUS "libsodium found")
else()
    message(FATAL_ERROR
        "libsodium not found.\n"
        "  Debian/Ubuntu: sudo apt install libsodium-dev\n"
        "  Fedora:        sudo dnf install libsodium-devel\n"
        "  macOS:         brew install libsodium")
endif()

# OpenDHT — real P2P networking for Sprint 2
set(HAS_OPENDHT OFF)

find_package(opendht QUIET)

if(TARGET opendht-static)
    # Debian/Ubuntu libopendht-dev exports opendht-static with full transitive deps
    # (jsoncpp, ssl, crypto, gnutls, nettle, fmt, …) required for libopendht.a.
    set(HAS_OPENDHT ON)
    target_link_libraries(Decentralized_P2P_Messenger PRIVATE opendht-static)
elseif(TARGET opendht::opendht)
    set(HAS_OPENDHT ON)
    target_link_libraries(Decentralized_P2P_Messenger PRIVATE opendht::opendht)
elseif(PkgConfig_FOUND)
    pkg_check_modules(OPENDHT QUIET opendht)
    if(OPENDHT_FOUND)
        set(HAS_OPENDHT ON)
        target_include_directories(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_INCLUDE_DIRS})
        if(OPENDHT_LIBRARY_DIRS)
            target_link_directories(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_LIBRARY_DIRS})
        endif()
        # pkg-config Libs omit private deps needed when linking the static archive
        pkg_check_modules(JSONCPP QUIET jsoncpp)
        target_link_libraries(Decentralized_P2P_Messenger PRIVATE
            ${OPENDHT_LIBRARIES}
            ${JSONCPP_LIBRARIES}
            ssl
            crypto
            fmt
            pthread
        )
    endif()
endif()

if(HAS_OPENDHT)
    message(STATUS "OpenDHT found — building with real DHT networking")
    target_compile_definitions(Decentralized_P2P_Messenger PRIVATE HAS_OPENDHT=1)
else()
    message(WARNING
        "OpenDHT not found — building in mock mode.\n"
        "  Install for real P2P: sudo apt install libopendht-dev  (Debian/Ubuntu)\n"
        "                        brew install opendht               (macOS)")
endif()
