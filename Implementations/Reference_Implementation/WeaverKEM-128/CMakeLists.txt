# CMakeLists.txt for WeaverKEM-128 (Reference Implementation)
# Security level: 128-bit classical / 80-bit quantum
# Subdirectory configuration - invoked from parent CMakeLists.txt

# WEAVER_MODE=1 selects the 128-bit classical security parameter set
add_compile_definitions(WEAVER_MODE=1)

# Include local headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REF_SRC}
)

# Source files: ICCS-provided fixed files + WeaverKEM algorithm sources
set(SOURCES
    # ICCS fixed files (must not be modified)
    drng.c
    auxfunc.c
    # KAT driver (must not be modified)
    KAT_KEM.c
    # Interface implementation (WeaverKEM-128 specific)
    KEM_WeaverKEM-128.c
    # WeaverKEM reference implementation
    ${REF_SRC}/kem_cca.c
    ${REF_SRC}/indcpa.c
    ${REF_SRC}/polyvec.c
    ${REF_SRC}/poly.c
    ${REF_SRC}/ntt.c
    ${REF_SRC}/cbd.c
    ${REF_SRC}/reduce.c
    ${REF_SRC}/verify.c
    ${REF_SRC}/fips202.c
    ${REF_SRC}/symmetric-shake.c
    ${REF_SRC}/msgenc.c
    ${REF_SRC}/aes256ctr.c
    ${REF_SRC}/bch_high.c
    ${REF_SRC}/bch_low.c
    ${REF_SRC}/poly_invq.c
)

add_executable(KAT_KEM_WeaverKEM-128 ${SOURCES})

# Link math library on Linux
if(UNIX)
    target_link_libraries(KAT_KEM_WeaverKEM-128 m)
endif()
