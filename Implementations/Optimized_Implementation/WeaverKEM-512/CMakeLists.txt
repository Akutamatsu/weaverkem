include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/weaver_avx2_iccs.cmake)

set(_avx512_src "")
if(WEAVER_USE_AVX_NTT7681)
  list(APPEND _avx512_src ${WEAVER_AVX2_SRC}/ntt7681_avx.c)
endif()
if(WEAVER_USE_AVX_COMPRESS7681)
  list(APPEND _avx512_src ${WEAVER_AVX2_SRC}/poly_compress_avx.c)
endif()

weaver_add_optimized_kat(
  KAT_KEM_WeaverKEM-512
  KEM_WeaverKEM-512.c
  5
  ${_avx512_src}
)
