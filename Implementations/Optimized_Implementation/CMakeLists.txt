cmake_minimum_required(VERSION 3.14)
project(WeaverKEM-Optimized C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(WEAVER_AVX2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../avx2_weaver")
set(WEAVER_AVX2_SRC  "${WEAVER_AVX2_ROOT}/src")

set(WEAVER_USE_AVX_NTT7681 ON CACHE BOOL "Link ntt7681_avx.c for modes 3/5")
set(WEAVER_USE_AVX_COMPRESS7681 ON CACHE BOOL "Link poly_compress_avx.c for modes 3/5")

add_compile_options(
  -Wall -O3 -fomit-frame-pointer
  -mavx2 -mbmi2 -mpopcnt
)

add_subdirectory(WeaverKEM-128)
add_subdirectory(WeaverKEM-256)
add_subdirectory(WeaverKEM-512)

add_custom_target(generate_kat
  COMMAND ./KAT_KEM_WeaverKEM-128
          && ./KAT_KEM_WeaverKEM-256
          && ./KAT_KEM_WeaverKEM-512
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMENT "Generate NGCC KAT files (optimized AVX2)"
)

add_dependencies(generate_kat
  KAT_KEM_WeaverKEM-128
  KAT_KEM_WeaverKEM-256
  KAT_KEM_WeaverKEM-512
)

add_custom_target(verify_kat
  COMMAND ${CMAKE_COMMAND} -E compare_files
          ${CMAKE_CURRENT_SOURCE_DIR}/../../Test_Vectors/KAT_KEM_WeaverKEM-128.txt
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/output/KAT_KEM_WeaverKEM-128.txt
  COMMAND ${CMAKE_COMMAND} -E compare_files
          ${CMAKE_CURRENT_SOURCE_DIR}/../../Test_Vectors/KAT_KEM_WeaverKEM-256.txt
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/output/KAT_KEM_WeaverKEM-256.txt
  COMMAND ${CMAKE_COMMAND} -E compare_files
          ${CMAKE_CURRENT_SOURCE_DIR}/../../Test_Vectors/KAT_KEM_WeaverKEM-512.txt
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/output/KAT_KEM_WeaverKEM-512.txt
  COMMENT "Compare optimized KAT output to reference Test_Vectors"
  DEPENDS generate_kat
)
