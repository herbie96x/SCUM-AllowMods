# scum_allow_mods — UE4SS C++ Mod Build Configuration
#
# Adapted from Narknon's AllowModsMod (https://github.com/narknon/AllowModsMod).
# A local UE4SS repository is built as a subdirectory — same convention as
# the AllowModsMod upstream. The first-time configure pulls UE4SS's
# third-party dependencies via FetchContent (internet connection required).
#
# Build command (Visual Studio multi-config):
#   cmake -B build -S .
#   cmake --build build --config Game__Shipping__Win64 --target scum_allow_mods
#
# Output:  build/Game__Shipping__Win64/scum_allow_mods.dll
# Deploy:  <UE4SS_MODS_DIR>/scum_allow_mods/dlls/main.dll  (POST_BUILD copy)

cmake_minimum_required(VERSION 3.22)
project(scum_allow_mods_build
    DESCRIPTION "Wrapper build: RE-UE4SS + scum_allow_mods mod"
    LANGUAGES CXX
)

# ── UE4SS repository path ─────────────────────────────────────────────────
# Default: ../../UE4SS/RE-UE4SS-main (relative to this CMakeLists).
# Override with -DUE4SS_REPO_DIR=<path>.
#
# The UE4SS repo MUST be cloned with `git clone --recurse-submodules`,
# not downloaded as a ZIP — submodule `deps/first/Unreal` (UEPseudo) is
# Epic-Games-protected and only accessible via a linked Epic-GitHub account.
if(NOT DEFINED UE4SS_REPO_DIR)
    set(_CANDIDATE_GIT "${CMAKE_SOURCE_DIR}/../../UE4SS/RE-UE4SS-main")
    set(_CANDIDATE_ZIP "${CMAKE_SOURCE_DIR}/../../UE4SS/RE-UE4SS-main/RE-UE4SS-main")
    if(EXISTS "${_CANDIDATE_GIT}/CMakeLists.txt"
       AND EXISTS "${_CANDIDATE_GIT}/deps/first/Unreal/CMakeLists.txt")
        set(UE4SS_REPO_DIR "${_CANDIDATE_GIT}" CACHE PATH "Path to RE-UE4SS monorepo")
    elseif(EXISTS "${_CANDIDATE_ZIP}/CMakeLists.txt")
        set(UE4SS_REPO_DIR "${_CANDIDATE_ZIP}" CACHE PATH "Path to RE-UE4SS monorepo")
    else()
        set(UE4SS_REPO_DIR "${_CANDIDATE_GIT}" CACHE PATH "Path to RE-UE4SS monorepo")
    endif()
endif()

if(NOT EXISTS "${UE4SS_REPO_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "RE-UE4SS monorepo NOT found:\n"
        "  Expected: ${UE4SS_REPO_DIR}/CMakeLists.txt\n"
        "  Pass -DUE4SS_REPO_DIR=<path> or place the repo at the default location.")
endif()

if(NOT EXISTS "${UE4SS_REPO_DIR}/UE4SS/include/Mod/CppUserModBase.hpp")
    message(FATAL_ERROR "UE4SS SDK headers not found:\n"
        "  Expected: ${UE4SS_REPO_DIR}/UE4SS/include/Mod/CppUserModBase.hpp")
endif()

message(STATUS "")
message(STATUS "=== scum_allow_mods build ===")
message(STATUS "UE4SS_REPO_DIR: ${UE4SS_REPO_DIR}")
message(STATUS "")

# Pull in UE4SS as a subdirectory — builds UE4SS target + all deps.
add_subdirectory(${UE4SS_REPO_DIR} ${CMAKE_BINARY_DIR}/ue4ss_build)

# ── Our mod target ────────────────────────────────────────────────────────
set(MOD_NAME scum_allow_mods)

add_library(${MOD_NAME} SHARED
    src/dllmain.cpp
)

target_include_directories(${MOD_NAME} PRIVATE src/)

# Link PUBLIC against UE4SS — pulls all transit deps (DynamicOutput, SigScanner, ...).
target_link_libraries(${MOD_NAME} PUBLIC UE4SS)

# Output DLL is named scum_allow_mods.dll (no lib- prefix).
set_target_properties(${MOD_NAME} PROPERTIES
    OUTPUT_NAME "${MOD_NAME}"
    PREFIX ""
)

if(WIN32)
    target_compile_definitions(${MOD_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# ── Deploy target ─────────────────────────────────────────────────────────
# Default deploy location is ../../UE4SS/SKRYPT-UE4SS-Mods/ (project-internal
# convention from the SKRYPT repo). For end users this should be overridden:
#
#   cmake -B build -S . -DUE4SS_MODS_DIR="C:/Steam/.../SCUM/Binaries/Win64/ue4ss/Mods"
if(NOT DEFINED UE4SS_MODS_DIR)
    set(UE4SS_MODS_DIR "${CMAKE_SOURCE_DIR}/../../UE4SS/SKRYPT-UE4SS-Mods"
        CACHE PATH "Target mods dir for deploy (POST_BUILD copy)")
endif()

# UE4SS C++ mod convention:
#   - DLL must be named "main.dll"
#   - Path: <UE4SS_MODS_DIR>/<modname>/dlls/main.dll
#   - Activation via empty enabled.txt next to dlls/
add_custom_command(TARGET ${MOD_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UE4SS_MODS_DIR}/${MOD_NAME}/dlls"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${MOD_NAME}>"
            "${UE4SS_MODS_DIR}/${MOD_NAME}/dlls/main.dll"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${UE4SS_MODS_DIR}/${MOD_NAME}/enabled.txt"
    COMMENT "Deploying ${MOD_NAME}.dll -> ${UE4SS_MODS_DIR}/${MOD_NAME}/dlls/main.dll"
)

message(STATUS "${MOD_NAME} mod target configured")
message(STATUS "  UE4SS_MODS_DIR: ${UE4SS_MODS_DIR}")
