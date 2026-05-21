# WeaverKEM AVX2 — make all | test | bench | clean

# Prefer clang when available; override with make CC=...
ifeq ($(origin CC),command line)
  # user passed CC= on the command line
else
  CC := $(shell command -v clang >/dev/null 2>&1 && echo clang || echo gcc)
endif
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  ARCH_FLAGS = -arch x86_64
else
  ARCH_FLAGS =
endif

CFLAGS_COMMON = -std=c99 -Wall -O3 -fomit-frame-pointer $(ARCH_FLAGS)
CFLAGS  = $(CFLAGS_COMMON) -mavx2 -mbmi2 -mpopcnt
PERF_NATIVE ?= 1
ENABLE_LTO  ?= 1
ifeq ($(PERF_NATIVE),1)
ifneq ($(UNAME_M),arm64)
  CFLAGS += -march=native -mtune=native
endif
endif
ifeq ($(ENABLE_LTO),1)
  CFLAGS += -flto
endif
# Set NO_INV_Q=1 to match submissions that omit poly_invq (canonical PK decompress)
ifeq ($(NO_INV_Q),1)
  CFLAGS += -DNO_INV_Q_LIFTING
endif
# Linux + glibc: fast OS RNG for randombytes() (KAT still uses AES-DRBG after randombytes_init).
USE_GETRANDOM ?= 0
ifeq ($(USE_GETRANDOM),1)
  CFLAGS += -DUSE_GETRANDOM
endif
ASMFLAGS = $(CFLAGS) -x assembler-with-cpp
INC     = -Isrc -Isrc/avx -Ibench
LDFLAGS = $(ARCH_FLAGS)
ifeq ($(ENABLE_LTO),1)
  LDFLAGS += -flto
  # Keccak x4 / rejsample / compress: avoid LTO miscompilation with gen_matrix (mode 1).
  NO_LTO_C = src/fips202x4.c src/rejsample.c src/indcpa_gen_matrix_avx.c \
             src/poly_compress_avx.c src/poly_compress9.c \
             src/keccak4x/KeccakP-1600-times4-SIMD256.c
  CFLAGS_NO_LTO = $(filter-out -flto,$(CFLAGS)) -fno-lto
endif
BD      = build

ICCS256 = ../Implementations/Reference_Implementation/WeaverKEM-256
ICCS128 = ../Implementations/Reference_Implementation/WeaverKEM-128
ICCS512 = ../Implementations/Reference_Implementation/WeaverKEM-512
REF_SRC = ../ref/src

# Binary suffixes match parameter set names (WEAVER-512 / 768 / 1024), not ICCS folder IDs.
# WEAVER_MODE: 1 -> *_512 + WeaverKEM-128 KAT; 3 -> *_768 + WeaverKEM-256; 5 -> *_1024 + WeaverKEM-512.

WEAVER_C = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly_weaver.c src/poly_compress9.c src/cbd.c src/reduce.c \
	src/verify.c src/fips202.c src/symmetric-shake.c src/randombytes_ctr.c \
	src/aes256ctr.c src/poly_invq.c

WEAVER_C_NORNG = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly_weaver.c src/poly_compress9.c src/cbd.c src/reduce.c \
	src/verify.c src/fips202.c src/symmetric-shake.c src/poly_invq.c

AVX_C  = src/poly_avx.c src/avx/consts.c
AVX_S  = src/avx/ntt.S src/avx/invntt.S src/avx/basemul.S src/avx/fq.S src/avx/shuffle.S
AVX_FQ_C = src/avx/consts.c
AVX_FQ_S = src/avx/fq.S
NTT_C  = src/ntt.c
BENCH  = bench/speed_test.c bench/cpucycles.c bench/speed_print.c
BENCH_COMPONENTS = bench/bench_components.c bench/cpucycles.c bench/speed_print.c

REF_LIB_C = \
	indcpa.c kem_cca.c msgenc.c bch_high.c bch_low.c polyvec.c poly.c \
	ntt.c cbd.c reduce.c verify.c fips202.c symmetric-shake.c poly_invq.c

REF_INCPA_C = \
	indcpa.c msgenc.c bch_high.c bch_low.c polyvec.c poly.c \
	ntt.c cbd.c reduce.c verify.c fips202.c symmetric-shake.c poly_invq.c

SAN ?= 0
ifeq ($(SAN),1)
CFLAGS_SAN = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
CFLAGS := $(CFLAGS_SAN) -mavx2 -mbmi2 -mpopcnt
LDFLAGS += -fsanitize=address,undefined
ENABLE_LTO := 0
endif

# n=256 (modes 1/3/5): AVX2 NTT + gen_matrix x4 + compress AVX
USE_AVX_NTT ?= 1
KECCAK4X_C = src/fips202x4.c src/rejsample.c src/indcpa_gen_matrix_avx.c \
             src/poly_compress_avx.c src/keccak4x/KeccakP-1600-times4-SIMD256.c
ifeq ($(USE_AVX_NTT),1)
MODE13_DEFS  = -DWEAVER_USE_AVX_NTT -DWEAVER_AVX_POST_NTT_REDUCE=1 \
               -DWEAVER_EXPERIMENTAL_MSGENC_AVX
MODE13_C_NTT = $(NTT_C) $(AVX_C) $(KECCAK4X_C)
MODE13_S_NTT = $(AVX_S)
else
MODE13_DEFS  =
MODE13_C_NTT = $(NTT_C)
MODE13_S_NTT =
endif

USE_AVX_COMPRESS ?= 1
ifeq ($(USE_AVX_COMPRESS),1)
MODE13_DEFS += -DWEAVER_USE_AVX_COMPRESS
endif

# Mode 5 uses n=256 (same AVX2 NTT path as modes 1/3).
MODE5_DEFS    = $(MODE13_DEFS)
MODE5_S_FQ    = $(MODE13_S_NTT)

USE_AVX_GEN_MATRIX512 ?= 1
ifeq ($(USE_AVX_GEN_MATRIX512),1)
MODE5_DEFS += -DWEAVER_AVX_GEN_MATRIX512
MODE5_GEN_MATRIX_C = src/indcpa_gen_matrix_avx512.c src/fips202x4.c \
                     src/keccak4x/KeccakP-1600-times4-SIMD256.c
NO_LTO_C += src/indcpa_gen_matrix_avx512.c src/fips202x4.c \
            src/keccak4x/KeccakP-1600-times4-SIMD256.c
else
MODE5_GEN_MATRIX_C =
endif
MODE5_BUILD_C = $(MODE13_C_NTT) $(MODE5_GEN_MATRIX_C)

.PHONY: all kat test bench bench-components gen_vectors ref_vectors clean test-san quick-check

quick-check: test test-extended bin/test_enc_derand_coins_1024 bin/test_compress9_512
	@chmod +x tools/quick_check.sh
	@bin/test_compress9_512

define link_bench_components
	@mkdir -p bin
	$(call compile_objs,bc$(1),$(WEAVER_C) $(3),$(1),$(4),$(5))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) $(4) -c bench/bench_components.c \
	  -o $(BD)/bc$(1)/bench_components.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c bench/cpucycles.c -o $(BD)/bc$(1)/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c bench/speed_print.c -o $(BD)/bc$(1)/speed_print.o
	$(CC) $(LDFLAGS) -o bin/bench_components_$(2) $(BD)/bc$(1)/*.o
endef

define link_kem_replay_ref
	@rm -rf $(BD)/kr$(1) && mkdir -p $(BD)/kr$(1) bin
	@for f in $(REF_LIB_C); do \
	  $(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -I$(REF_SRC) -c $(REF_SRC)/$$f \
	    -o $(BD)/kr$(1)/ref_$$(basename $$f .c).o; \
	done
	@$(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -DWEAVER_REF_SIDE=1 -I$(REF_SRC) \
	  -c tools/kem_derand_replay.c -o $(BD)/kr$(1)/kem_derand_replay.o
	$(CC) $(ARCH_FLAGS) -o bin/kem_replay_ref_$(2) $(BD)/kr$(1)/*.o
endef

define link_kem_replay_avx
	@mkdir -p bin
	$(call compile_objs,ka$(1),$(WEAVER_C_NORNG) $(3),$(1),$(4),$(5))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c tools/kem_derand_replay.c \
	  -o $(BD)/ka$(1)/kem_derand_replay.o
	$(CC) $(LDFLAGS) -o bin/kem_replay_avx_$(2) $(BD)/ka$(1)/*.o
endef

all: bin/weaver_avx_512 bin/weaver_avx_768 bin/weaver_avx_1024 \
     bin/kat_iccs_512 bin/kat_iccs_768 bin/kat_iccs_1024 \
     bin/gen_vectors_768 bin/verify_vectors_768 \
     bin/compare_ntt_avx_512 bin/compare_ntt_avx_768 bin/compare_ntt_avx_1024

# Replay ../Test_Vectors/ (generated from ref via Reference_Implementation)
kat: bin/kat_iccs_512 bin/kat_iccs_768 bin/kat_iccs_1024
	@echo "=== ICCS KAT 128 (WEAVER-512) ===" \
	  && bin/kat_iccs_512 ../Test_Vectors/KAT_KEM_WeaverKEM-128.txt
	@echo "=== ICCS KAT 256 (WEAVER-768) ===" \
	  && bin/kat_iccs_768 ../Test_Vectors/KAT_KEM_WeaverKEM-256.txt
	@echo "=== ICCS KAT 512 (WEAVER-1024) ===" \
	  && bin/kat_iccs_1024 ../Test_Vectors/KAT_KEM_WeaverKEM-512.txt

# --- helpers ---
define compile_objs
	@rm -rf $(BD)/$(1) && mkdir -p $(BD)/$(1)
	@for f in $(2); do \
	  obj=$$(echo $$f | sed 's#^\./##; s#^\.\./##; s#/#_#g; s/\.[^.]*$$/.o/'); \
	  cflags="$(CFLAGS) -DWEAVER_MODE=$(3) $(INC) $(4)"; \
	  if echo " $(NO_LTO_C) " | grep -q " $$f "; then \
	    cflags="$(CFLAGS_NO_LTO) -DWEAVER_MODE=$(3) $(INC) $(4)"; \
	  fi; \
	  $(CC) $$cflags -c $$f -o $(BD)/$(1)/$$obj; \
	done
	@for f in $(5); do \
	  obj=$$(echo $$f | sed 's#^\./##; s#^\.\./##; s#/#_#g; s/\.[^.]*$$/.o/'); \
	  $(CC) $(ASMFLAGS) -DWEAVER_MODE=$(3) $(INC) $(4) -c $$f \
	    -o $(BD)/$(1)/$$obj; \
	done
endef

define link_bin
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $(1) $(BD)/$(2)/*.o
endef

LINK = $(CC)

# --- benchmarks ---
# Modes 1 & 3: AVX2 NTT path when USE_AVX_NTT=1, otherwise portable C NTT.
bin/weaver_avx_512:
	$(call compile_objs,w512,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_DEFS),$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_test.c -o $(BD)/w512/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/cpucycles.c -o $(BD)/w512/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_print.c -o $(BD)/w512/speed_print.o
	$(call link_bin,$@,w512)

bin/weaver_avx_768:
	$(call compile_objs,w768,$(WEAVER_C) $(MODE13_C_NTT),3,$(MODE13_DEFS),$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_test.c -o $(BD)/w768/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/cpucycles.c -o $(BD)/w768/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_print.c -o $(BD)/w768/speed_print.o
	$(call link_bin,$@,w768)

bin/weaver_avx_1024:
	$(call compile_objs,w1024,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_test.c -o $(BD)/w1024/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/cpucycles.c -o $(BD)/w1024/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_print.c -o $(BD)/w1024/speed_print.o
	$(call link_bin,$@,w1024)

# --- ICCS KAT verify ---
bin/kat_iccs_768: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat768,$(WEAVER_C_NORNG) $(MODE13_C_NTT) $(ICCS256)/drng.c $(ICCS256)/auxfunc.c,3,-I$(ICCS256) $(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -I$(ICCS256) -c tools/kat_iccs.c -o $(BD)/kat768/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat768/*.o

bin/kat_iccs_512: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat512,$(WEAVER_C_NORNG) $(MODE13_C_NTT) $(ICCS128)/drng.c $(ICCS128)/auxfunc.c,1,-I$(ICCS128) $(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -I$(ICCS128) -c tools/kat_iccs.c -o $(BD)/kat512/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat512/*.o

bin/kat_iccs_1024: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat1024,$(WEAVER_C_NORNG) $(MODE5_BUILD_C) $(ICCS512)/drng.c $(ICCS512)/auxfunc.c,5,-I$(ICCS512) $(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -I$(ICCS512) -c tools/kat_iccs.c -o $(BD)/kat1024/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat1024/*.o

# --- extended vectors ---
bin/gen_vectors_512:
	@mkdir -p bin
	$(call compile_objs,gv512,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/gen_vectors.c -o $(BD)/gv512/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv512/*.o

bin/gen_vectors_768:
	@mkdir -p bin
	$(call compile_objs,gv768,$(WEAVER_C) $(MODE13_C_NTT),3,$(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/gen_vectors.c -o $(BD)/gv768/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv768/*.o

bin/gen_vectors_1024:
	@mkdir -p bin
	$(call compile_objs,gv1024,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/gen_vectors.c -o $(BD)/gv1024/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv1024/*.o

bin/verify_vectors_512: tools/verify_vectors.c
	@mkdir -p bin
	$(call compile_objs,vv512,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/verify_vectors.c -o $(BD)/vv512/verify_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/vv512/*.o

bin/verify_vectors_768: tools/verify_vectors.c
	@mkdir -p bin
	$(call compile_objs,vv768,$(WEAVER_C) $(MODE13_C_NTT),3,$(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/verify_vectors.c -o $(BD)/vv768/verify_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/vv768/*.o

bin/verify_vectors_1024: tools/verify_vectors.c
	@mkdir -p bin
	$(call compile_objs,vv1024,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/verify_vectors.c -o $(BD)/vv1024/verify_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/vv1024/*.o

bin/compare_ref_avx_512: tools/compare_ref_avx.c
	@mkdir -p bin
	$(call compile_objs,cmp512,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp512/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp512/*.o

bin/compare_ref_avx_768: tools/compare_ref_avx.c
	@mkdir -p bin
	$(call compile_objs,cmp768,$(WEAVER_C) $(MODE13_C_NTT),3,$(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp768/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp768/*.o

bin/compare_ref_avx_1024: tools/compare_ref_avx.c
	@mkdir -p bin
	$(call compile_objs,cmp1024,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp1024/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp1024/*.o

bin/kem_replay_ref_512:
	$(call link_kem_replay_ref,1,512)

bin/kem_replay_ref_768:
	$(call link_kem_replay_ref,3,768)

bin/kem_replay_ref_1024:
	$(call link_kem_replay_ref,5,1024)

bin/kem_replay_avx_512:
	$(call link_kem_replay_avx,1,512,$(MODE13_C_NTT),$(MODE13_DEFS),$(MODE13_S_NTT))

bin/kem_replay_avx_768:
	$(call link_kem_replay_avx,3,768,$(MODE13_C_NTT),$(MODE13_DEFS),$(MODE13_S_NTT))

bin/kem_replay_avx_1024:
	$(call link_kem_replay_avx,5,1024,$(MODE5_BUILD_C),$(MODE5_DEFS),$(MODE5_S_FQ))

COMPARE_RANDOM_SH = tools/compare_ref_random.sh

bin/compare_ref_random_512: bin/kem_replay_ref_512 bin/kem_replay_avx_512 $(COMPARE_RANDOM_SH)
	@chmod +x $(COMPARE_RANDOM_SH)
	@echo "compare_ref_random: WEAVER-512 (N=256)"
	@$(COMPARE_RANDOM_SH) bin/kem_replay_ref_512 bin/kem_replay_avx_512 10000

bin/compare_ref_random_768: bin/kem_replay_ref_768 bin/kem_replay_avx_768 $(COMPARE_RANDOM_SH)
	@chmod +x $(COMPARE_RANDOM_SH)
	@echo "compare_ref_random: WEAVER-768 (N=256)"
	@$(COMPARE_RANDOM_SH) bin/kem_replay_ref_768 bin/kem_replay_avx_768 10000

bin/compare_ref_random_1024: bin/kem_replay_ref_1024 bin/kem_replay_avx_1024 $(COMPARE_RANDOM_SH)
	@chmod +x $(COMPARE_RANDOM_SH)
	@echo "compare_ref_random: WEAVER-1024 (N=256)"
	@$(COMPARE_RANDOM_SH) bin/kem_replay_ref_1024 bin/kem_replay_avx_1024 10000

define link_trace_encap_ref
	@rm -rf $(BD)/ter$(1) && mkdir -p $(BD)/ter$(1) bin
	@for f in $(REF_INCPA_C) kem_cca.c; do \
	  $(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -I$(REF_SRC) -c $(REF_SRC)/$$f \
	    -o $(BD)/ter$(1)/ref_$$(basename $$f .c).o; \
	done
	@$(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -DWEAVER_REF_SIDE=1 -I$(REF_SRC) -Isrc \
	  -c src/randombytes_ctr.c -o $(BD)/ter$(1)/randombytes_ctr.o
	@$(CC) $(CFLAGS_COMMON) -I$(REF_SRC) -Isrc -c src/aes256ctr.c \
	  -o $(BD)/ter$(1)/aes256ctr.o
	@$(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -DWEAVER_REF_SIDE=1 -I$(REF_SRC) \
	  -c tools/trace_encap_mode5.c -o $(BD)/ter$(1)/trace_encap.o
	$(CC) $(ARCH_FLAGS) -o bin/trace_encap_ref_1024 $(BD)/ter$(1)/*.o
endef

define link_trace_encap_avx
	@mkdir -p bin
	$(call compile_objs,tea$(1),$(WEAVER_C_NORNG) src/kem_cca.c src/randombytes_ctr.c src/aes256ctr.c $(MODE5_BUILD_C),$(1),$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c tools/trace_encap_mode5.c \
	  -o $(BD)/tea$(1)/trace_encap.o
	$(CC) $(LDFLAGS) -o bin/trace_encap_avx_1024 $(BD)/tea$(1)/*.o
endef

TRACE_ENCAP_SH = tools/compare_encap_trace.sh

bin/trace_encap_ref_1024: tools/trace_encap_mode5.c
	$(call link_trace_encap_ref,5)

bin/trace_encap_avx_1024: tools/trace_encap_mode5.c
	$(call link_trace_encap_avx,5)

trace-encap-mode5: bin/trace_encap_ref_1024 bin/trace_encap_avx_1024 $(TRACE_ENCAP_SH)
	@chmod +x $(TRACE_ENCAP_SH)
	@$(TRACE_ENCAP_SH) bin/trace_encap_ref_1024 bin/trace_encap_avx_1024

bin/bench_components_512:
	$(call link_bench_components,1,512,$(MODE13_C_NTT),$(MODE13_DEFS),$(MODE13_S_NTT))

bin/bench_components_768:
	$(call link_bench_components,3,768,$(MODE13_C_NTT),$(MODE13_DEFS),$(MODE13_S_NTT))

bin/bench_components_1024:
	$(call link_bench_components,5,1024,$(MODE5_BUILD_C),$(MODE5_DEFS),$(MODE5_S_FQ))

bin/compare_ntt_avx_512: tools/compare_ntt_avx.c
	@mkdir -p bin
	$(call compile_objs,nttcmp512,src/avx/consts.c,1,, $(AVX_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/compare_ntt_avx.c -o $(BD)/nttcmp512/compare_ntt_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/nttcmp512/*.o

bin/compare_ntt_avx_768: tools/compare_ntt_avx.c
	@mkdir -p bin
	$(call compile_objs,nttcmp768,src/avx/consts.c,3,, $(AVX_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/compare_ntt_avx.c -o $(BD)/nttcmp768/compare_ntt_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/nttcmp768/*.o

bin/compare_ntt_avx_1024: tools/compare_ntt_avx.c
	@mkdir -p bin
	$(call compile_objs,nttcmp1024,src/avx/consts.c,5,, $(AVX_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/compare_ntt_avx.c -o $(BD)/nttcmp1024/compare_ntt_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/nttcmp1024/*.o

define link_gen_vectors_ref
	@mkdir -p bin
	@rm -rf $(BD)/refgv$(1) && mkdir -p $(BD)/refgv$(1)
	@for f in $(REF_LIB_C); do \
	  $(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -I$(REF_SRC) -c $(REF_SRC)/$$f \
	    -o $(BD)/refgv$(1)/$$(basename $$f .c).o; \
	done
	$(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=$(1) -I$(REF_SRC) -Isrc -c tools/gen_vectors.c \
	  -o $(BD)/refgv$(1)/gen_vectors_tool.o
	@$(CC) $(CFLAGS_COMMON) -I$(REF_SRC) -Isrc -c src/randombytes_ctr.c \
	  -o $(BD)/refgv$(1)/randombytes_ctr.o
	@$(CC) $(CFLAGS_COMMON) -I$(REF_SRC) -Isrc -c src/aes256ctr.c \
	  -o $(BD)/refgv$(1)/aes256ctr.o
	$(CC) $(ARCH_FLAGS) -o bin/gen_vectors_ref_$(2) $(BD)/refgv$(1)/*.o
endef

bin/gen_vectors_ref_512: src/randombytes_ctr.c src/aes256ctr.c tools/gen_vectors.c
	$(call link_gen_vectors_ref,1,512)

bin/gen_vectors_ref_768: src/randombytes_ctr.c src/aes256ctr.c tools/gen_vectors.c
	$(call link_gen_vectors_ref,3,768)

bin/gen_vectors_ref_1024: src/randombytes_ctr.c src/aes256ctr.c tools/gen_vectors.c
	$(call link_gen_vectors_ref,5,1024)

gen_vectors: bin/gen_vectors_512 bin/gen_vectors_768 bin/gen_vectors_1024
	@mkdir -p test_vectors
	bin/gen_vectors_512 test_vectors/weaver_avx_mode1.vec
	bin/gen_vectors_768 test_vectors/weaver_avx_mode3.vec
	bin/gen_vectors_1024 test_vectors/weaver_avx_mode5.vec

ref_vectors: bin/gen_vectors_ref_512 bin/gen_vectors_ref_768 bin/gen_vectors_ref_1024
	@mkdir -p test_vectors
	bin/gen_vectors_ref_512 test_vectors/weaver_ref_mode1.vec
	bin/gen_vectors_ref_768 test_vectors/weaver_ref_mode3.vec
	bin/gen_vectors_ref_1024 test_vectors/weaver_ref_mode5.vec

# Full correctness for WEAVER-512 / 768 / 1024: KAT, 100-vector ref cross-check,
# 10000 derand replays, AVX self-check, and NTT primitive checks per level.
test: kat ref_vectors gen_vectors \
      bin/compare_ref_avx_512 bin/compare_ref_avx_768 bin/compare_ref_avx_1024 \
      bin/compare_ref_random_512 bin/compare_ref_random_768 bin/compare_ref_random_1024 \
      bin/verify_vectors_512 bin/verify_vectors_768 bin/verify_vectors_1024 \
      bin/compare_ntt_avx_512 bin/compare_ntt_avx_768 bin/compare_ntt_avx_1024
	@echo "=== Ref vs AVX vectors (WEAVER-512) ===" \
	  && bin/compare_ref_avx_512 test_vectors/weaver_ref_mode1.vec
	@echo "=== Ref vs AVX vectors (WEAVER-768) ===" \
	  && bin/compare_ref_avx_768 test_vectors/weaver_ref_mode3.vec
	@echo "=== Ref vs AVX vectors (WEAVER-1024) ===" \
	  && bin/compare_ref_avx_1024 test_vectors/weaver_ref_mode5.vec
	@echo "=== AVX self-check (WEAVER-512) ===" \
	  && bin/verify_vectors_512 test_vectors/weaver_avx_mode1.vec
	@echo "=== AVX self-check (WEAVER-768) ===" \
	  && bin/verify_vectors_768 test_vectors/weaver_avx_mode3.vec
	@echo "=== AVX self-check (WEAVER-1024) ===" \
	  && bin/verify_vectors_1024 test_vectors/weaver_avx_mode5.vec
	@echo "=== NTT AVX primitives (WEAVER-512) ===" \
	  && bin/compare_ntt_avx_512
	@echo "=== NTT AVX primitives (WEAVER-768) ===" \
	  && bin/compare_ntt_avx_768
	@echo "=== NTT AVX primitives (WEAVER-1024) ===" \
	  && bin/compare_ntt_avx_1024
	@echo "PASS: make test completed for WEAVER-512, WEAVER-768, and WEAVER-1024."

test-extended: test test-gen-matrix-mode5 bin/test_enc_derand_coins_1024
	@bin/test_enc_derand_coins_1024
	@echo "PASS: extended checks (gen_matrix mode 5, enc_derand coins) done."

bin/test_enc_derand_coins_1024:
	@mkdir -p bin
	$(call compile_objs,tedc5,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/test_enc_derand_coins.c \
	  -o $(BD)/tedc5/test_enc_derand_coins.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tedc5/*.o

test-san: SAN=1
test-san: clean kat bin/compare_ref_random_512 bin/compare_ref_random_768 \
          bin/test_enc_derand_coins_1024 bin/compare_ref_random_1024
	@bin/test_enc_derand_coins_1024
	@echo "=== ASan/UBSan KAT + random diff + mode-5 enc_derand coins done ==="

bench: all
	@echo "========== AVX2 Weaver (median cycles, 10000 iters) =========="
	@for x in 512 768 1024; do echo "--- WEAVER-$$x ---"; bin/weaver_avx_$$x; done

bin/test_compress9_512:
	@mkdir -p bin
	$(call compile_objs,tc9,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_DEFS),$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) $(MODE13_DEFS) -c tools/test_compress9.c \
	  -o $(BD)/tc9/test_compress9.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tc9/*.o

bin/test_gen_matrix5_scalar:
	@mkdir -p bin
	$(call compile_objs,tgm5s,$(WEAVER_C) $(NTT_C),5,,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) \
	  -c tools/test_gen_matrix_mode5.c -o $(BD)/tgm5s/test_gen_matrix_mode5.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tgm5s/*.o

bin/test_gen_matrix5_avx:
	@mkdir -p bin
	$(call compile_objs,tgm5a,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) $(MODE5_DEFS) \
	  -c tools/test_gen_matrix_mode5.c -o $(BD)/tgm5a/test_gen_matrix_mode5.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tgm5a/*.o

test-gen-matrix-mode5: bin/test_gen_matrix5_scalar bin/test_gen_matrix5_avx
	@chmod +x tools/compare_gen_matrix_mode5.sh
	@tools/compare_gen_matrix_mode5.sh 50

bench-components: bin/bench_components_512 bin/bench_components_768 bin/bench_components_1024
	@echo "========== component benchmarks (modes 1, 3, 5) =========="
	@bin/bench_components_512
	@bin/bench_components_768
	@bin/bench_components_1024

bin/profile_encap_512:
	@mkdir -p bin
	$(call compile_objs,pe1,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_DEFS),$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) $(MODE13_DEFS) -DWEAVER_PROFILE_KEYPAIR_DERAND -c src/indcpa.c \
	  -o $(BD)/pe1/src_indcpa.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) $(MODE13_DEFS) -DWEAVER_PROFILE_KEYPAIR_DERAND -c tools/profile_encap_decaps.c \
	  -o $(BD)/pe1/profile_encap_decaps.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/cpucycles.c -o $(BD)/pe1/cpucycles.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/pe1/*.o

bin/profile_encap_768:
	@mkdir -p bin
	$(call compile_objs,pe3,$(WEAVER_C) $(MODE13_C_NTT),3,$(MODE13_DEFS),$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) $(MODE13_DEFS) -DWEAVER_PROFILE_KEYPAIR_DERAND -c src/indcpa.c \
	  -o $(BD)/pe3/src_indcpa.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) $(MODE13_DEFS) -DWEAVER_PROFILE_KEYPAIR_DERAND -c tools/profile_encap_decaps.c \
	  -o $(BD)/pe3/profile_encap_decaps.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/cpucycles.c -o $(BD)/pe3/cpucycles.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/pe3/*.o

profile-encap: bin/profile_encap_512 bin/profile_encap_768
	@echo "========== encaps/decaps profile (modes 1, 3) =========="
	@bin/profile_encap_512
	@bin/profile_encap_768

clean:
	rm -rf bin build test_vectors/*.vec
