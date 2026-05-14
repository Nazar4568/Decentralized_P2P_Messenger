cmake_minimum_required(VERSION 4.1)
project(Decentralized_P2P_Messenger)

set(CMAKE_CXX_STANDARD 17)

add_executable(Decentralized_P2P_Messenger
        src/main.cpp
        include/P2PNode.h
        src/P2PNode.cpp
)

if (APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OPENDHT REQUIRED opendht)

    target_include_directories(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_INCLUDE_DIRS} "/opt/homebrew/include")
    target_link_directories(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_LIBRARY_DIRS} "/opt/homebrew/lib")
    target_link_libraries(Decentralized_P2P_Messenger PRIVATE ${OPENDHT_LIBRARIES})

elseif (WIN32)
    find_package(opendht REQUIRED)

    target_link_libraries(Decentralized_P2P_Messenger PRIVATE opendht::opendht)
endif()

