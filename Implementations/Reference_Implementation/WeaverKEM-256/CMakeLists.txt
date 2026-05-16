# CMakeLists.txt for WeaverKEM-256 (Reference Implementation)
# Subdirectory configuration - invoked from parent CMakeLists.txt

add_compile_definitions(WEAVER_MODE=3)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REF_SRC}
)

set(SOURCES
    drng.c
    auxfunc.c
    KAT_KEM.c
    KEM_WeaverKEM-256.c
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

add_executable(KAT_KEM_WeaverKEM-256 ${SOURCES})

if(UNIX)
    target_link_libraries(KAT_KEM_WeaverKEM-256 m)
endif()

add_custom_target(generate_kat_256
    COMMAND KAT_KEM_WeaverKEM-256
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating KAT_KEM_WeaverKEM-256.txt"
    DEPENDS KAT_KEM_WeaverKEM-256
)
