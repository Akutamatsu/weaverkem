include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/weaver_avx2_iccs.cmake)

weaver_add_optimized_kat(
  KAT_KEM_WeaverKEM-128
  KEM_WeaverKEM-128.c
  1
  ${WEAVER_AVX2_SRC}/ntt3329_avx128.c
  ${WEAVER_AVX2_SRC}/poly_compress_avx.c
  ${WEAVER_AVX2_SRC}/poly_compress9.c
)
