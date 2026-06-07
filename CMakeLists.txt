cmake_minimum_required(VERSION 3.16)
project(Decentralized_P2P_Messenger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# wxWidgets GUI (Sprint 1)
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(Decentralized_P2P_Messenger
    src/main.cpp
    src/MainWindow.cpp
    src/P2PNode.cpp
    src/CryptoService.cpp
    include/IP2PNode.h
    include/P2PNode.h
    include/P2PWxEvents.h
    include/MainWindow.h
    include/Types.h
    include/CryptoService.h
)

target_include_directories(Decentralized_P2P_Messenger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /opt/homebrew/opt/libsodium/include
)

target_link_libraries(Decentralized_P2P_Messenger PRIVATE 
    ${wxWidgets_LIBRARIES}
    /opt/homebrew/opt/libsodium/lib/libsodium.dylib
)

# OpenDHT — optional platform-specific link for Sprint 2+ (not required for Sprint 1 mock)
if(APPLE)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OPENDHT opendht)

    if(OPENDHT_FOUND)
        target_include_directories(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_INCLUDE_DIRS} "/opt/homebrew/include")
        target_link_directories(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_LIBRARY_DIRS} "/opt/homebrew/lib")
        target_link_libraries(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_LIBRARIES})
        target_compile_definitions(Decentralized_P2P_Messenger PRIVATE HAS_OPENDHT=1)
    endif()
elseif(WIN32)
    find_package(opendht QUIET)
    if(opendht_FOUND)
        target_link_libraries(Decentralized_P2P_Messenger PRIVATE opendht::opendht)
        target_compile_definitions(Decentralized_P2P_Messenger PRIVATE HAS_OPENDHT=1)
    endif()
endif()
