# WeaverKEM AVX2 — CMake build (mirrors avx2_weaver/Makefile defaults)
#
#   cmake -B build -S .
#   cmake --build build
#   ctest --test-dir build          # optional
#
# Common options:
#   -DWEAVER_PERF_NATIVE=OFF        # portable -march (no -march=native)
#   -DWEAVER_ENABLE_LTO=OFF         # disable link-time optimization
#   -DWEAVER_USE_AVX_CBD=OFF        # scalar CBD only
#   -DWEAVER_USE_AVX_COMPRESS7681=OFF

cmake_minimum_required(VERSION 3.14)
project(WeaverKEM-AVX2 C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --- options (Makefile equivalents) ---
option(WEAVER_PERF_NATIVE "Use -march=native -mtune=native" ON)
option(WEAVER_ENABLE_LTO "Enable -flto (with per-file -fno-lto exceptions)" ON)
option(WEAVER_NO_INV_Q "Define NO_INV_Q_LIFTING" OFF)
option(WEAVER_USE_GETRANDOM "Use getrandom() for randombytes()" OFF)
option(WEAVER_USE_AVX_CBD "AVX2 CBD (eta=7/9)" ON)
option(WEAVER_USE_AVX_NTT128 "Mode 1: ntt3329_avx128" ON)
option(WEAVER_USE_AVX_GEN_MATRIX128 "Mode 1: shake128x4 gen_matrix" ON)
option(WEAVER_USE_AVX_COMPRESS128 "Mode 1: AVX2 9-bit compress" ON)
option(WEAVER_USE_AVX_NTT7681 "Modes 3/5: ntt7681_avx.c" ON)
option(WEAVER_USE_AVX_GEN_MATRIX7681 "Modes 3/5: shake128x4 gen_matrix7681" ON)
option(WEAVER_USE_AVX_NTT7681_ASM "Modes 3/5: ntt7681 assembly (experimental)" OFF)
option(WEAVER_USE_AVX_COMPRESS7681 "Modes 3/5: AVX2 10/11-bit compress" ON)
option(WEAVER_BUILD_TESTS "Build compare/test executables" ON)
option(WEAVER_BUILD_KAT "Build ICCS KAT replay tools" ON)
option(WEAVER_BUILD_BENCH "Build speed_test benchmarks" ON)

set(WEAVER_ICCS128 "${CMAKE_CURRENT_SOURCE_DIR}/../Implementations/Reference_Implementation/WeaverKEM-128")
set(WEAVER_ICCS256 "${CMAKE_CURRENT_SOURCE_DIR}/../Implementations/Reference_Implementation/WeaverKEM-256")
set(WEAVER_ICCS512 "${CMAKE_CURRENT_SOURCE_DIR}/../Implementations/Reference_Implementation/WeaverKEM-512")
set(WEAVER_REF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../ref/src")

set(WEAVER_CORE_SRC
  src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c
  src/polyvec.c src/poly.c src/ntt.c src/cbd.c src/reduce.c
  src/verify.c src/fips202.c src/symmetric-shake.c src/randombytes_ctr.c
  src/aes256ctr.c src/poly_invq.c
)

set(WEAVER_CORE_SRC_NORNG
  src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c
  src/polyvec.c src/poly.c src/ntt.c src/cbd.c src/reduce.c
  src/verify.c src/fips202.c src/symmetric-shake.c
  src/aes256ctr.c src/poly_invq.c
)

set(WEAVER_NO_LTO_SRC
  src/fips202x4.c
  src/rejsample.c
  src/poly_compress_avx.c
  src/poly_compress9.c
  src/keccak4x/KeccakP-1600-times4-SIMD256.c
)

set(WEAVER_INC src src/avx bench)

# --- global compile flags ---
add_compile_options(
  -Wall -O3 -fomit-frame-pointer
  -mavx2 -mbmi2 -mpopcnt
)

if(WEAVER_PERF_NATIVE)
  add_compile_options(-march=native -mtune=native)
endif()

if(WEAVER_NO_INV_Q)
  add_compile_definitions(NO_INV_Q_LIFTING)
endif()

if(WEAVER_USE_GETRANDOM)
  add_compile_definitions(USE_GETRANDOM)
endif()

if(WEAVER_USE_AVX_CBD)
  list(APPEND WEAVER_CORE_SRC src/cbd_avx2.c)
  list(APPEND WEAVER_CORE_SRC_NORNG src/cbd_avx2.c)
  list(APPEND WEAVER_NO_LTO_SRC src/cbd_avx2.c)
  add_compile_definitions(WEAVER_USE_AVX_CBD)
endif()

if(WEAVER_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok)
  if(ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "LTO not supported by this toolchain; continuing without -flto")
  endif()
endif()

# --- mode-specific source/def lists ---
set(MODE1_EXTRA_SRC "")
set(MODE1_DEFS "")
set(MODE7681_EXTRA_SRC "")
set(MODE7681_DEFS "")
set(MODE7681_EXTRA_ASM "")
set(MODE5_EXTRA_SRC "")
set(MODE5_DEFS "")
set(MODE5_EXTRA_ASM "")

if(WEAVER_USE_AVX_NTT128)
  list(APPEND MODE1_EXTRA_SRC src/ntt3329_avx128.c)
  list(APPEND MODE1_DEFS WEAVER_USE_AVX_NTT128)
endif()

if(WEAVER_USE_AVX_GEN_MATRIX128)
  list(APPEND MODE1_EXTRA_SRC
    src/indcpa_gen_matrix_avx128.c src/fips202x4.c src/rejsample.c
    src/keccak4x/KeccakP-1600-times4-SIMD256.c)
  list(APPEND MODE1_DEFS WEAVER_AVX_GEN_MATRIX128)
  list(APPEND WEAVER_NO_LTO_SRC
    src/indcpa_gen_matrix_avx128.c src/fips202x4.c src/rejsample.c
    src/keccak4x/KeccakP-1600-times4-SIMD256.c)
endif()

if(WEAVER_USE_AVX_COMPRESS128)
  list(APPEND MODE1_EXTRA_SRC src/poly_compress_avx.c src/poly_compress9.c)
  list(APPEND MODE1_DEFS WEAVER_USE_AVX_COMPRESS)
endif()

if(WEAVER_USE_AVX_NTT7681)
  list(APPEND MODE7681_EXTRA_SRC src/ntt7681_avx.c)
  list(APPEND MODE7681_DEFS WEAVER_USE_AVX_NTT7681)
  list(APPEND MODE5_EXTRA_SRC src/ntt7681_avx.c)
  list(APPEND MODE5_DEFS WEAVER_USE_AVX_NTT7681)
endif()

if(WEAVER_USE_AVX_NTT7681_ASM)
  list(APPEND MODE7681_EXTRA_ASM src/avx/ntt7681_asm256.S)
  list(APPEND MODE7681_EXTRA_SRC src/avx/consts7681.c)
  list(APPEND MODE7681_DEFS WEAVER_USE_AVX_NTT7681_ASM)
endif()

if(WEAVER_USE_AVX_GEN_MATRIX7681)
  list(APPEND MODE7681_EXTRA_SRC
    src/indcpa_gen_matrix_avx7681.c src/fips202x4.c
    src/keccak4x/KeccakP-1600-times4-SIMD256.c)
  list(APPEND MODE7681_DEFS WEAVER_AVX_GEN_MATRIX7681)
  list(APPEND WEAVER_NO_LTO_SRC
    src/indcpa_gen_matrix_avx7681.c src/fips202x4.c
    src/keccak4x/KeccakP-1600-times4-SIMD256.c)
endif()

if(WEAVER_USE_AVX_COMPRESS7681)
  list(APPEND MODE7681_EXTRA_SRC src/poly_compress_avx.c)
  list(APPEND MODE7681_DEFS WEAVER_USE_AVX_COMPRESS)
endif()

list(APPEND MODE5_DEFS ${MODE7681_DEFS})
list(APPEND MODE5_EXTRA_SRC ${MODE7681_EXTRA_SRC})
list(APPEND MODE5_EXTRA_ASM ${MODE7681_EXTRA_ASM})

if(WEAVER_ENABLE_LTO)
  list(REMOVE_DUPLICATES WEAVER_NO_LTO_SRC)
  foreach(f ${WEAVER_NO_LTO_SRC})
    set_source_files_properties(${f} PROPERTIES COMPILE_OPTIONS "-fno-lto")
  endforeach()
endif()

enable_language(ASM)
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp -mavx2 -mbmi2 -mpopcnt")

# --- helpers ---
function(weaver_add_mode_lib out_var mode mode_defs extra_src extra_asm use_rng)
  if(use_rng)
    set(sources ${WEAVER_CORE_SRC} ${extra_src} ${extra_asm})
  else()
    set(sources ${WEAVER_CORE_SRC_NORNG} ${extra_src} ${extra_asm})
  endif()
  add_library(${out_var} OBJECT ${sources})
  target_include_directories(${out_var} PRIVATE ${WEAVER_INC})
  target_compile_definitions(${out_var} PRIVATE WEAVER_MODE=${mode})
  if(mode_defs)
    target_compile_definitions(${out_var} PRIVATE ${mode_defs})
  endif()
  if(extra_asm)
    set_source_files_properties(${extra_asm} PROPERTIES LANGUAGE ASM)
  endif()
  set(${out_var} ${out_var} PARENT_SCOPE)
endfunction()

function(weaver_link_exe exe_name lib_name)
  add_executable(${exe_name} $<TARGET_OBJECTS:${lib_name}>)
  target_include_directories(${exe_name} PRIVATE ${WEAVER_INC})
endfunction()

function(weaver_add_tool exe_name lib_name tool_src mode mode_defs)
  add_executable(${exe_name} ${tool_src} $<TARGET_OBJECTS:${lib_name}>)
  target_include_directories(${exe_name} PRIVATE ${WEAVER_INC})
  target_compile_definitions(${exe_name} PRIVATE WEAVER_MODE=${mode})
  if(mode_defs)
    target_compile_definitions(${exe_name} PRIVATE ${mode_defs})
  endif()
endfunction()

# --- per-mode object libraries ---
weaver_add_mode_lib(weaver_mode1_obj 1 "${MODE1_DEFS}" "${MODE1_EXTRA_SRC}" "" ON)
weaver_add_mode_lib(weaver_mode3_obj 3 "${MODE7681_DEFS}" "${MODE7681_EXTRA_SRC}" "${MODE7681_EXTRA_ASM}" ON)
weaver_add_mode_lib(weaver_mode5_obj 5 "${MODE5_DEFS}" "${MODE5_EXTRA_SRC}" "${MODE5_EXTRA_ASM}" ON)

weaver_add_mode_lib(weaver_mode1_norng_obj 1 "${MODE1_DEFS}" "${MODE1_EXTRA_SRC}" "" OFF)
weaver_add_mode_lib(weaver_mode3_norng_obj 3 "${MODE7681_DEFS}" "${MODE7681_EXTRA_SRC}" "${MODE7681_EXTRA_ASM}" OFF)
weaver_add_mode_lib(weaver_mode5_norng_obj 5 "${MODE5_DEFS}" "${MODE5_EXTRA_SRC}" "${MODE5_EXTRA_ASM}" OFF)

# --- benchmarks ---
if(WEAVER_BUILD_BENCH)
  add_executable(weaver_avx_512
    bench/speed_test.c bench/cpucycles.c bench/speed_print.c
    $<TARGET_OBJECTS:weaver_mode1_obj>)
  target_include_directories(weaver_avx_512 PRIVATE ${WEAVER_INC})
  target_compile_definitions(weaver_avx_512 PRIVATE WEAVER_MODE=1)

  add_executable(weaver_avx_1024
    bench/speed_test.c bench/cpucycles.c bench/speed_print.c
    $<TARGET_OBJECTS:weaver_mode3_obj>)
  target_include_directories(weaver_avx_1024 PRIVATE ${WEAVER_INC})
  target_compile_definitions(weaver_avx_1024 PRIVATE WEAVER_MODE=3)

  add_executable(weaver_avx_2048
    bench/speed_test.c bench/cpucycles.c bench/speed_print.c
    $<TARGET_OBJECTS:weaver_mode5_obj>)
  target_include_directories(weaver_avx_2048 PRIVATE ${WEAVER_INC})
  target_compile_definitions(weaver_avx_2048 PRIVATE WEAVER_MODE=5)
endif()

# --- ICCS KAT replay ---
if(WEAVER_BUILD_KAT)
  weaver_add_tool(kat_iccs_128 weaver_mode1_norng_obj tools/kat_iccs.c 1 "${MODE1_DEFS}")
  target_sources(kat_iccs_128 PRIVATE
    "${WEAVER_ICCS128}/drng.c" "${WEAVER_ICCS128}/auxfunc.c")
  target_include_directories(kat_iccs_128 PRIVATE "${WEAVER_ICCS128}")

  weaver_add_tool(kat_iccs_256 weaver_mode3_norng_obj tools/kat_iccs.c 3 "${MODE7681_DEFS}")
  target_sources(kat_iccs_256 PRIVATE
    "${WEAVER_ICCS256}/drng.c" "${WEAVER_ICCS256}/auxfunc.c")
  target_include_directories(kat_iccs_256 PRIVATE "${WEAVER_ICCS256}")

  weaver_add_tool(kat_iccs_512 weaver_mode5_norng_obj tools/kat_iccs.c 5 "${MODE5_DEFS}")
  target_sources(kat_iccs_512 PRIVATE
    "${WEAVER_ICCS512}/drng.c" "${WEAVER_ICCS512}/auxfunc.c")
  target_include_directories(kat_iccs_512 PRIVATE "${WEAVER_ICCS512}")
endif()

# --- tests / tools ---
if(WEAVER_BUILD_TESTS)
  weaver_add_tool(compare_ref_avx_128 weaver_mode1_obj tools/compare_ref_avx.c 1 "${MODE1_DEFS}")
  weaver_add_tool(compare_ref_avx_1024 weaver_mode3_obj tools/compare_ref_avx.c 3 "${MODE7681_DEFS}")
  weaver_add_tool(compare_ref_avx_512 weaver_mode5_obj tools/compare_ref_avx.c 5 "${MODE5_DEFS}")

  weaver_add_tool(test_compress9_512 weaver_mode1_obj tools/test_compress9.c 1 "${MODE1_DEFS}")
  weaver_add_tool(test_compress7681_1024 weaver_mode3_obj tools/test_compress7681.c 3 "${MODE7681_DEFS}")
  weaver_add_tool(test_compress7681_2048 weaver_mode5_obj tools/test_compress7681.c 5 "${MODE5_DEFS}")

  add_executable(compare_ntt_avx_2048
    tools/compare_ntt_avx.c
    src/ntt.c src/reduce.c src/ntt7681_avx.c)
  target_include_directories(compare_ntt_avx_2048 PRIVATE ${WEAVER_INC})
  target_compile_definitions(compare_ntt_avx_2048 PRIVATE
    WEAVER_MODE=5 ${MODE7681_DEFS})

  enable_testing()
  add_test(NAME test_compress7681_1024 COMMAND test_compress7681_1024)
  add_test(NAME test_compress7681_2048 COMMAND test_compress7681_2048)
  add_test(NAME test_compress9_512 COMMAND test_compress9_512)
endif()

# --- convenience targets ---
if(WEAVER_BUILD_BENCH)
  add_custom_target(weaver_bench
    COMMAND ${CMAKE_COMMAND} -E echo "========== AVX2 Weaver benchmarks =========="
    COMMAND weaver_avx_512
    COMMAND weaver_avx_1024
    COMMAND weaver_avx_2048
    DEPENDS weaver_avx_512 weaver_avx_1024 weaver_avx_2048
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    COMMENT "Run speed_test for all three parameter sets"
  )
endif()

if(WEAVER_BUILD_KAT)
  add_custom_target(weaver_kat
    COMMAND ${CMAKE_COMMAND} -E echo "=== ICCS KAT 128 ==="
    COMMAND kat_iccs_128 "${CMAKE_CURRENT_SOURCE_DIR}/../Test_Vectors/KAT_KEM_WeaverKEM-128.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=== ICCS KAT 256 ==="
    COMMAND kat_iccs_256 "${CMAKE_CURRENT_SOURCE_DIR}/../Test_Vectors/KAT_KEM_WeaverKEM-256.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=== ICCS KAT 512 ==="
    COMMAND kat_iccs_512 "${CMAKE_CURRENT_SOURCE_DIR}/../Test_Vectors/KAT_KEM_WeaverKEM-512.txt"
    DEPENDS kat_iccs_128 kat_iccs_256 kat_iccs_512
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
  )
endif()

if(WEAVER_BUILD_TESTS)
  add_custom_target(weaver_quick_check
    COMMAND test_compress9_512
    COMMAND test_compress7681_1024
    COMMAND test_compress7681_2048
    DEPENDS test_compress9_512 test_compress7681_1024 test_compress7681_2048
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
  )
endif()
