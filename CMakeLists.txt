cmake_minimum_required(VERSION 3.15)
project(VaultArchiver VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(vault
    src/main.cpp
    src/vault.cpp
    src/vault_crypto.cpp
)

target_include_directories(vault PRIVATE src)

add_executable(vault_gui WIN32
    src/vault_gui.cpp
    src/vault.cpp
    src/vault_crypto.cpp
)

target_include_directories(vault_gui PRIVATE src)
