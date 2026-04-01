# CMakeLists.txt for WeaverKEM Reference Implementation (Root)
# Builds all three security levels: 128, 256, 512
# Build: mkdir build && cd build && cmake .. && cmake --build .

cmake_minimum_required(VERSION 3.10)
project(WeaverKEM-Reference C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set output directory for all executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Path to the shared reference source tree
set(REF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../ref/src")

# Include directories for all subdirectories
include_directories(
    ${REF_SRC}
)

# Add subdirectories for each security level
add_subdirectory(WeaverKEM-128)
add_subdirectory(WeaverKEM-256)
add_subdirectory(WeaverKEM-512)

# Custom target: generate test vectors
add_custom_target(generate_kat
    COMMAND ./KAT_KEM_WeaverKEM-128 && ./KAT_KEM_WeaverKEM-256 && ./KAT_KEM_WeaverKEM-512
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Generating KAT_KEM_WeaverKEM-xxx.txt"
)

add_dependencies(generate_kat KAT_KEM_WeaverKEM-128 KAT_KEM_WeaverKEM-256 KAT_KEM_WeaverKEM-512)
