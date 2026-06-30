# Shared sources/flags for Optimized_Implementation KAT executables.
# ICCS symmetric (SM3) + scalar gen_matrix; AVX2 NTT/CBD/compress kernels are local.

set(WEAVER_AVX2_CORE_SRC
  indcpa.c
  kem_cca.c
  msgenc.c
  bch_high.c
  bch_low.c
  polyvec.c
  poly.c
  ntt.c
  cbd.c
  reduce.c
  verify.c
  poly_invq.c
  cbd_avx2.c
  rejsample.c
)

set(WEAVER_ICCS_LOCAL_SRC
  drng.c
  auxfunc.c
  KAT_KEM.c
  symmetric-iccs.c
)

function(weaver_add_optimized_kat exe_name kem_c_file mode)
  set(extra_avx_src ${ARGN})
  set(sources
    ${WEAVER_ICCS_LOCAL_SRC}
    ${kem_c_file}
    ${WEAVER_AVX2_CORE_SRC}
    ${extra_avx_src}
  )
  add_executable(${exe_name} ${sources})
  target_include_directories(${exe_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_compile_definitions(${exe_name} PRIVATE
    WEAVER_MODE=${mode}
    WEAVER_USE_AVX_CBD=1
  )
  if(mode EQUAL 1)
    target_compile_definitions(${exe_name} PRIVATE
      WEAVER_USE_AVX_NTT128=1
      WEAVER_USE_AVX_COMPRESS=1
    )
  else()
    if(WEAVER_USE_AVX_NTT7681)
      target_compile_definitions(${exe_name} PRIVATE WEAVER_USE_AVX_NTT7681=1)
    endif()
    if(WEAVER_USE_AVX_COMPRESS7681)
      target_compile_definitions(${exe_name} PRIVATE WEAVER_USE_AVX_COMPRESS=1)
    endif()
  endif()
  if(UNIX)
    target_link_libraries(${exe_name} m)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${exe_name} OpenSSL::Crypto)
  endif()
  add_custom_target(generate_kat_${exe_name}
    COMMAND ${exe_name}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Generate ${exe_name}.txt"
    DEPENDS ${exe_name}
  )
endfunction()
