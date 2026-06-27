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
USE_AVX_CBD ?= 1
ifeq ($(USE_AVX_CBD),1)
  CFLAGS += -DWEAVER_USE_AVX_CBD
  CBD_AVX_C = src/cbd_avx2.c
else
  CBD_AVX_C =
endif
ASMFLAGS = $(CFLAGS) -x assembler-with-cpp
INC     = -Isrc -Isrc/avx -Ibench
LDFLAGS = $(ARCH_FLAGS)
ifeq ($(ENABLE_LTO),1)
  LDFLAGS += -flto
  # Keccak x4 / rejsample / compress: avoid LTO miscompilation with gen_matrix (mode 1).
  NO_LTO_C = src/fips202x4.c src/rejsample.c src/indcpa_gen_matrix_avx.c \
             src/poly_compress_avx.c src/poly_compress9.c \
             src/keccak4x/KeccakP-1600-times4-SIMD256.c $(CBD_AVX_C)
  CFLAGS_NO_LTO = $(filter-out -flto,$(CFLAGS)) -fno-lto
endif
BD      = build

ICCS256 = ../Implementations/Reference_Implementation/WeaverKEM-256
ICCS128 = ../Implementations/Reference_Implementation/WeaverKEM-128
ICCS512 = ../Implementations/Reference_Implementation/WeaverKEM-512
REF_SRC = ../ref/src

WEAVER_C = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly.c src/ntt.c src/cbd.c src/reduce.c \
	src/verify.c src/fips202.c src/symmetric-shake.c src/randombytes_ctr.c \
	src/aes256ctr.c src/poly_invq.c $(CBD_AVX_C)

WEAVER_C_NORNG = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly.c src/ntt.c src/cbd.c src/reduce.c \
	src/verify.c src/fips202.c src/symmetric-shake.c \
	src/aes256ctr.c src/poly_invq.c $(CBD_AVX_C)

AVX_C  = src/poly_avx.c src/avx/consts.c
AVX_S  = src/avx/ntt.S src/avx/invntt.S src/avx/basemul.S src/avx/fq.S src/avx/shuffle.S
AVX_FQ_C = src/avx/consts.c
AVX_FQ_S = src/avx/fq.S
NTT_C  = src/ntt.c
BENCH  = bench/speed_test.c bench/cpucycles.c bench/speed_print.c
BENCH_COMPONENTS = bench/bench_components.c bench/cpucycles.c bench/speed_print.c
BENCH_NTESTS ?= 1000
BENCH_CSELFTESTS ?= 1000

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

# Kyber AVX2 NTT (n=256,q=3329) disabled by default; NGCC params need portable ref C NTT.
USE_AVX_NTT ?= 0
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

USE_AVX_COMPRESS ?= 0
ifeq ($(USE_AVX_COMPRESS),1)
MODE13_DEFS += -DWEAVER_USE_AVX_COMPRESS
endif

USE_AVX_FQ_512 ?= 0
ifeq ($(USE_AVX_FQ_512),1)
MODE5_DEFS = -DWEAVER_USE_AVX_FQ_512 -DWEAVER_EXPERIMENTAL_MSGENC_AVX
MODE5_C_FQ = $(AVX_FQ_C) src/poly_avx512.c
MODE5_S_FQ = $(AVX_FQ_S)
else
MODE5_DEFS =
MODE5_C_FQ =
MODE5_S_FQ =
endif

USE_AVX_GEN_MATRIX512 ?= 0
ifeq ($(USE_AVX_GEN_MATRIX512),1)
MODE5_DEFS += -DWEAVER_AVX_GEN_MATRIX512
MODE5_GEN_MATRIX_C = src/indcpa_gen_matrix_avx512.c src/fips202x4.c \
                     src/keccak4x/KeccakP-1600-times4-SIMD256.c
NO_LTO_C += src/indcpa_gen_matrix_avx512.c src/fips202x4.c \
            src/keccak4x/KeccakP-1600-times4-SIMD256.c
else
MODE5_GEN_MATRIX_C =
endif
MODE5_BUILD_C = $(NTT_C) $(MODE7681_C) $(MODE5_C_FQ) $(MODE5_GEN_MATRIX_C)

# q=7681, n=256 (Weaver-1024 / mode 3): 16-bit AVX2 NTT + shake128x4 gen_matrix.
USE_AVX_NTT7681 ?= 1
USE_AVX_GEN_MATRIX7681 ?= 1
USE_AVX_NTT7681_ASM ?= 0
MODE7681_DEFS =
MODE7681_C =
MODE7681_S =
ifeq ($(USE_AVX_NTT7681),1)
MODE7681_DEFS += -DWEAVER_USE_AVX_NTT7681
MODE7681_C += src/ntt7681_avx.c
ifeq ($(USE_AVX_NTT7681_ASM),1)
MODE7681_DEFS += -DWEAVER_USE_AVX_NTT7681_ASM
MODE7681_S += src/avx/ntt7681_asm256.S
MODE7681_C += src/avx/consts7681.c
endif
endif
ifeq ($(USE_AVX_GEN_MATRIX7681),1)
MODE7681_DEFS += -DWEAVER_AVX_GEN_MATRIX7681
MODE7681_C += src/indcpa_gen_matrix_avx7681.c src/fips202x4.c \
               src/keccak4x/KeccakP-1600-times4-SIMD256.c
NO_LTO_C += src/indcpa_gen_matrix_avx7681.c src/fips202x4.c \
            src/keccak4x/KeccakP-1600-times4-SIMD256.c
endif

USE_AVX_COMPRESS7681 ?= 1
ifeq ($(USE_AVX_COMPRESS7681),1)
MODE7681_DEFS += -DWEAVER_USE_AVX_COMPRESS
MODE7681_C += src/poly_compress_avx.c src/poly_compress9.c
endif

MODE5_DEFS += $(MODE7681_DEFS)

# q=3329, n=128 (WEAVER-640 / mode 1): AVX2 NTT + shake128x4 gen_matrix.
USE_AVX_NTT128 ?= 1
USE_AVX_GEN_MATRIX128 ?= 1
MODE1_DEFS =
MODE1_C =
ifeq ($(USE_AVX_NTT128),1)
MODE1_DEFS += -DWEAVER_USE_AVX_NTT128
MODE1_C += src/ntt3329_avx128.c
endif
ifeq ($(USE_AVX_GEN_MATRIX128),1)
MODE1_DEFS += -DWEAVER_AVX_GEN_MATRIX128
MODE1_C += src/indcpa_gen_matrix_avx128.c src/fips202x4.c src/rejsample.c \
           src/keccak4x/KeccakP-1600-times4-SIMD256.c
NO_LTO_C += src/indcpa_gen_matrix_avx128.c src/fips202x4.c src/rejsample.c \
            src/keccak4x/KeccakP-1600-times4-SIMD256.c
endif
USE_AVX_COMPRESS128 ?= 1
ifeq ($(USE_AVX_COMPRESS128),1)
MODE1_DEFS += -DWEAVER_USE_AVX_COMPRESS
MODE1_C += src/poly_compress_avx.c src/poly_compress9.c
endif

.PHONY: all kat test bench bench-components bench-q12289-saber bench-q7681 bench-q7681-i16 gen_vectors ref_vectors clean test-san quick-check

quick-check: test test-extended bin/test_enc_derand_coins_2048 bin/test_compress9_512 \
             bin/test_compress7681_1024 bin/test_compress7681_2048
	@chmod +x tools/quick_check.sh
	@bin/test_compress9_512
	@bin/test_compress7681_1024
	@bin/test_compress7681_2048

define link_bench_components
	@mkdir -p bin
	$(call compile_objs,bc$(1),$(WEAVER_C) $(3),$(1),$(4),$(5))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) $(4) -c bench/bench_components.c \
	  -o $(BD)/bc$(1)/bench_components.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c bench/cpucycles.c -o $(BD)/bc$(1)/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c bench/speed_print.c -o $(BD)/bc$(1)/speed_print.o
	$(CC) $(LDFLAGS) -o bin/bench_components_$(2) $(BD)/bc$(1)/*.o
endef

define link_profile_encap
	@mkdir -p bin
	$(call compile_objs,pe$(1),$(WEAVER_C) $(3),$(1),$(4),$(5))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) $(4) -DWEAVER_PROFILE_KEYPAIR_DERAND \
	  -c src/indcpa.c -o $(BD)/pe$(1)/src_indcpa.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) $(4) -DWEAVER_PROFILE_KEYPAIR_DERAND \
	  -c tools/profile_encap_decaps.c -o $(BD)/pe$(1)/profile_encap_decaps.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c bench/cpucycles.c \
	  -o $(BD)/pe$(1)/cpucycles.o
	$(CC) $(LDFLAGS) -o bin/profile_encap_$(2) $(BD)/pe$(1)/*.o
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

all: bin/weaver_avx_512 bin/weaver_avx_1024 bin/weaver_avx_2048 \
     bin/kat_iccs_128 bin/kat_iccs_256 bin/kat_iccs_512 \
     bin/gen_vectors_1024 bin/verify_vectors_1024 \
     bin/compare_ntt_avx_512 bin/compare_ntt_avx_1024 bin/compare_ntt_avx_2048

# Replay ../Test_Vectors/ (generated from ref via Reference_Implementation)
kat: bin/kat_iccs_128 bin/kat_iccs_256 bin/kat_iccs_512
	@echo "=== ICCS KAT 128 (WEAVER-512) ===" \
	  && bin/kat_iccs_128 ../Test_Vectors/KAT_KEM_WeaverKEM-128.txt
	@echo "=== ICCS KAT 256 (WEAVER-1024) ===" \
	  && bin/kat_iccs_256 ../Test_Vectors/KAT_KEM_WeaverKEM-256.txt
	@echo "=== ICCS KAT 512 (WEAVER-2048) ===" \
	  && bin/kat_iccs_512 ../Test_Vectors/KAT_KEM_WeaverKEM-512.txt

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
	$(call compile_objs,w512,$(WEAVER_C) $(MODE1_C),1,$(MODE1_DEFS),)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_test.c -o $(BD)/w512/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/cpucycles.c -o $(BD)/w512/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_print.c -o $(BD)/w512/speed_print.o
	$(call link_bin,$@,w512)

bin/weaver_avx_1024:
	$(call compile_objs,w1024,$(WEAVER_C) $(MODE7681_C),3,$(MODE7681_DEFS),$(MODE7681_S))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_test.c -o $(BD)/w1024/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/cpucycles.c -o $(BD)/w1024/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_print.c -o $(BD)/w1024/speed_print.o
	$(call link_bin,$@,w1024)

bin/weaver_avx_2048:
	$(call compile_objs,w2048,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_test.c -o $(BD)/w2048/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/cpucycles.c -o $(BD)/w2048/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_print.c -o $(BD)/w2048/speed_print.o
	$(call link_bin,$@,w2048)

# --- ICCS KAT verify ---
bin/kat_iccs_256: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat256,$(WEAVER_C_NORNG) $(MODE13_C_NTT) $(ICCS256)/drng.c $(ICCS256)/auxfunc.c,3,-I$(ICCS256) $(MODE13_DEFS),$(MODE13_S_NTT))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -I$(ICCS256) -c tools/kat_iccs.c -o $(BD)/kat256/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat256/*.o

bin/kat_iccs_128: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat128,$(WEAVER_C_NORNG) $(MODE1_C) $(ICCS128)/drng.c $(ICCS128)/auxfunc.c,1,-I$(ICCS128) $(MODE1_DEFS),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -I$(ICCS128) -c tools/kat_iccs.c -o $(BD)/kat128/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat128/*.o

bin/kat_iccs_512: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat512,$(WEAVER_C_NORNG) $(MODE5_BUILD_C) $(ICCS512)/drng.c $(ICCS512)/auxfunc.c,5,-I$(ICCS512) $(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -I$(ICCS512) -c tools/kat_iccs.c -o $(BD)/kat512/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat512/*.o

# --- extended vectors ---
bin/gen_vectors_512:
	@mkdir -p bin
	$(call compile_objs,gv512,$(WEAVER_C) $(MODE1_C),1,$(MODE1_DEFS),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/gen_vectors.c -o $(BD)/gv512/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv512/*.o

bin/gen_vectors_1024:
	@mkdir -p bin
	$(call compile_objs,gv1024,$(WEAVER_C) $(MODE7681_C),3,$(MODE7681_DEFS),$(MODE7681_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/gen_vectors.c -o $(BD)/gv1024/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv1024/*.o

bin/gen_vectors_2048:
	@mkdir -p bin
	$(call compile_objs,gv2048,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/gen_vectors.c -o $(BD)/gv2048/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv2048/*.o

bin/verify_vectors_1024: tools/verify_vectors.c
	@mkdir -p bin
	$(call compile_objs,vv1024,$(WEAVER_C) $(MODE7681_C),3,$(MODE7681_DEFS),$(MODE7681_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/verify_vectors.c -o $(BD)/vv1024/verify_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/vv1024/*.o

bin/compare_ref_avx_128: tools/compare_ref_avx.c
	@mkdir -p bin
	$(call compile_objs,cmp128,$(WEAVER_C) $(MODE1_C),1,$(MODE1_DEFS),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp128/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp128/*.o

bin/compare_ref_avx_1024: tools/compare_ref_avx.c
	@mkdir -p bin
	$(call compile_objs,cmp1024,$(WEAVER_C) $(MODE7681_C),3,$(MODE7681_DEFS),$(MODE7681_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp1024/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp1024/*.o

bin/compare_ref_avx_512: tools/compare_ref_avx.c
	@mkdir -p bin
	$(call compile_objs,cmp512,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp512/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp512/*.o

bin/kem_replay_ref_512:
	$(call link_kem_replay_ref,1,512)

bin/kem_replay_ref_1024:
	$(call link_kem_replay_ref,3,1024)

bin/kem_replay_ref_2048:
	$(call link_kem_replay_ref,5,2048)

bin/kem_replay_avx_512:
	$(call link_kem_replay_avx,1,512,$(MODE1_C),$(MODE1_DEFS),)

bin/kem_replay_avx_1024:
	$(call link_kem_replay_avx,3,1024,$(MODE7681_C),$(MODE7681_DEFS),$(MODE7681_S))

bin/kem_replay_avx_2048:
	$(call link_kem_replay_avx,5,2048,$(MODE5_BUILD_C),$(MODE5_DEFS),$(MODE5_S_FQ))

COMPARE_RANDOM_SH = tools/compare_ref_random.sh

bin/compare_ref_random_512: bin/kem_replay_ref_512 bin/kem_replay_avx_512 $(COMPARE_RANDOM_SH)
	@chmod +x $(COMPARE_RANDOM_SH)
	@echo "compare_ref_random: WEAVER-512 (N=256)"
	@$(COMPARE_RANDOM_SH) bin/kem_replay_ref_512 bin/kem_replay_avx_512 10000

bin/compare_ref_random_1024: bin/kem_replay_ref_1024 bin/kem_replay_avx_1024 $(COMPARE_RANDOM_SH)
	@chmod +x $(COMPARE_RANDOM_SH)
	@echo "compare_ref_random: WEAVER-1024 (N=512)"
	@$(COMPARE_RANDOM_SH) bin/kem_replay_ref_1024 bin/kem_replay_avx_1024 10000

bin/compare_ref_random_2048: bin/kem_replay_ref_2048 bin/kem_replay_avx_2048 $(COMPARE_RANDOM_SH)
	@chmod +x $(COMPARE_RANDOM_SH)
	@echo "compare_ref_random: WEAVER-2048 (N=1024)"
	@$(COMPARE_RANDOM_SH) bin/kem_replay_ref_2048 bin/kem_replay_avx_2048 5000

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
	$(CC) $(ARCH_FLAGS) -o bin/trace_encap_ref_2048 $(BD)/ter$(1)/*.o
endef

define link_trace_encap_avx
	@mkdir -p bin
	$(call compile_objs,tea$(1),$(WEAVER_C_NORNG) src/kem_cca.c src/randombytes_ctr.c src/aes256ctr.c $(MODE5_BUILD_C),$(1),$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=$(1) $(INC) -c tools/trace_encap_mode5.c \
	  -o $(BD)/tea$(1)/trace_encap.o
	$(CC) $(LDFLAGS) -o bin/trace_encap_avx_2048 $(BD)/tea$(1)/*.o
endef

TRACE_ENCAP_SH = tools/compare_encap_trace.sh

bin/trace_encap_ref_2048: tools/trace_encap_mode5.c
	$(call link_trace_encap_ref,5)

bin/trace_encap_avx_2048: tools/trace_encap_mode5.c
	$(call link_trace_encap_avx,5)

trace-encap-mode5: bin/trace_encap_ref_2048 bin/trace_encap_avx_2048 $(TRACE_ENCAP_SH)
	@chmod +x $(TRACE_ENCAP_SH)
	@$(TRACE_ENCAP_SH) bin/trace_encap_ref_2048 bin/trace_encap_avx_2048

bin/bench_components_512:
	$(call link_bench_components,1,512,$(MODE1_C),$(MODE1_DEFS),)

bin/bench_components_1024:
	$(call link_bench_components,3,1024,$(MODE7681_C),$(MODE7681_DEFS),$(MODE7681_S))

bin/bench_components_2048:
	$(call link_bench_components,5,2048,$(MODE5_BUILD_C),$(MODE5_DEFS),$(MODE5_S_FQ))

bin/compare_ntt_avx_512: tools/compare_ntt_avx.c
	@mkdir -p bin
	$(call compile_objs,nttcmp128,src/ntt.c src/reduce.c src/ntt3329_avx128.c,1,$(MODE1_DEFS),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) $(MODE1_DEFS) -c tools/compare_ntt_avx.c -o $(BD)/nttcmp128/compare_ntt_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/nttcmp128/*.o

bin/compare_ntt_avx_1024: tools/compare_ntt_avx.c
	@mkdir -p bin
	$(call compile_objs,nttcmp1024,src/ntt.c src/reduce.c src/ntt7681_avx.c $(filter src/avx/consts7681.c,$(MODE7681_C)),3,$(MODE7681_DEFS),$(MODE7681_S))
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) $(MODE7681_DEFS) -c tools/compare_ntt_avx.c -o $(BD)/nttcmp1024/compare_ntt_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/nttcmp1024/*.o

bin/compare_ntt_avx_2048: tools/compare_ntt_avx.c
	@mkdir -p bin
	$(call compile_objs,nttcmp2048,src/ntt.c src/reduce.c src/ntt7681_avx.c,5,$(MODE7681_DEFS),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) $(MODE7681_DEFS) -c tools/compare_ntt_avx.c -o $(BD)/nttcmp2048/compare_ntt_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/nttcmp2048/*.o

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

bin/gen_vectors_ref_1024: src/randombytes_ctr.c src/aes256ctr.c tools/gen_vectors.c
	$(call link_gen_vectors_ref,3,1024)

bin/gen_vectors_ref_2048: src/randombytes_ctr.c src/aes256ctr.c tools/gen_vectors.c
	$(call link_gen_vectors_ref,5,2048)

gen_vectors: bin/gen_vectors_512 bin/gen_vectors_1024 bin/gen_vectors_2048
	@mkdir -p test_vectors
	bin/gen_vectors_512 test_vectors/weaver_avx_mode1.vec
	bin/gen_vectors_1024 test_vectors/weaver_avx_mode3.vec
	bin/gen_vectors_2048 test_vectors/weaver_avx_mode5.vec

ref_vectors: bin/gen_vectors_ref_512 bin/gen_vectors_ref_1024 bin/gen_vectors_ref_2048
	@mkdir -p test_vectors
	bin/gen_vectors_ref_512 test_vectors/weaver_ref_mode1.vec
	bin/gen_vectors_ref_1024 test_vectors/weaver_ref_mode3.vec
	bin/gen_vectors_ref_2048 test_vectors/weaver_ref_mode5.vec

test: kat ref_vectors gen_vectors \
      bin/compare_ref_avx_128 bin/compare_ref_avx_1024 bin/compare_ref_avx_512 \
      bin/compare_ref_random_512 bin/compare_ref_random_1024 \
      bin/verify_vectors_1024 bin/compare_ntt_avx_2048
	@echo "=== Ref vs AVX vectors (mode 1) ===" \
	  && bin/compare_ref_avx_128 test_vectors/weaver_ref_mode1.vec
	@echo "=== Ref vs AVX vectors (mode 3) ===" \
	  && bin/compare_ref_avx_1024 test_vectors/weaver_ref_mode3.vec
	@echo "=== Random ref vs AVX (mode 1, 10000) ===" \
	  && $(MAKE) bin/compare_ref_random_512
	@echo "=== Random ref vs AVX (mode 3, 10000) ===" \
	  && $(MAKE) bin/compare_ref_random_1024
	@echo "=== AVX self-check (mode 3) ===" \
	  && bin/verify_vectors_1024 test_vectors/weaver_avx_mode3.vec
	@echo "=== NTT512 AVX primitives (mode 5) ===" \
	  && bin/compare_ntt_avx_2048

test-extended: test test-gen-matrix-mode5 \
      bin/compare_ref_avx_512 bin/compare_ref_random_2048
	@echo "=== Ref vs AVX vectors (mode 5) ===" \
	  && bin/compare_ref_avx_512 test_vectors/weaver_ref_mode5.vec
	@echo "=== Random ref vs AVX (mode 5, 5000) ===" \
	  && $(MAKE) bin/compare_ref_random_2048

bin/test_enc_derand_coins_2048:
	@mkdir -p bin
	$(call compile_objs,tedc5,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/test_enc_derand_coins.c \
	  -o $(BD)/tedc5/test_enc_derand_coins.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tedc5/*.o

test-san: SAN=1
test-san: clean kat bin/compare_ref_random_512 bin/compare_ref_random_1024 \
          bin/test_enc_derand_coins_2048 bin/compare_ref_random_2048
	@bin/test_enc_derand_coins_2048
	@echo "=== ASan/UBSan KAT + random diff + mode-5 enc_derand coins done ==="

bench: all
	@echo "========== AVX2 Weaver (median cycles, 10000 iters) =========="
	@for x in 512 1024 2048; do echo "--- WEAVER-$$x ---"; bin/weaver_avx_$$x; done

bin/test_compress9_512:
	@mkdir -p bin
	$(call compile_objs,tc9,$(WEAVER_C) $(MODE1_C),1,$(MODE1_DEFS),)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) $(MODE1_DEFS) -c tools/test_compress9.c \
	  -o $(BD)/tc9/test_compress9.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tc9/*.o

bin/test_compress7681_1024:
	@mkdir -p bin
	$(call compile_objs,tc7681,$(WEAVER_C) $(MODE7681_C),3,$(MODE7681_DEFS),$(MODE7681_S))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) $(MODE7681_DEFS) -c tools/test_compress7681.c \
	  -o $(BD)/tc7681/test_compress7681.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tc7681/*.o

bin/test_compress7681_2048:
	@mkdir -p bin
	$(call compile_objs,tc7681_5,$(WEAVER_C) $(MODE5_BUILD_C),5,$(MODE5_DEFS),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) $(MODE5_DEFS) -c tools/test_compress7681.c \
	  -o $(BD)/tc7681_5/test_compress7681.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tc7681_5/*.o

bin/test_gen_matrix5_scalar:
	@mkdir -p bin
	$(call compile_objs,tgm5s,$(WEAVER_C) $(NTT_C) $(MODE5_C_FQ),5,\
	  $(filter-out -DWEAVER_AVX_GEN_MATRIX512,$(MODE5_DEFS)),$(MODE5_S_FQ))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) \
	  $(filter-out -DWEAVER_AVX_GEN_MATRIX512 -DWEAVER_AVX_GEN_MATRIX7681,$(MODE5_DEFS)) \
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
	@USE_AVX_GEN_MATRIX7681=0 $(MAKE) -s bin/test_gen_matrix5_scalar
	@USE_AVX_GEN_MATRIX7681=1 $(MAKE) -s bin/test_gen_matrix5_avx
	@tools/compare_gen_matrix_mode5.sh 50

bin/test_gen_matrix1_scalar: tools/test_gen_matrix_mode1.c
	@mkdir -p bin
	$(call compile_objs,tgm1s,$(WEAVER_C),1,,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/test_gen_matrix_mode1.c \
	  -o $(BD)/tgm1s/test_gen_matrix_mode1.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tgm1s/*.o

bin/test_gen_matrix1_avx: tools/test_gen_matrix_mode1.c
	@mkdir -p bin
	$(call compile_objs,tgm1a,$(WEAVER_C) $(MODE1_C),1,$(MODE1_DEFS),)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) $(MODE1_DEFS) -c tools/test_gen_matrix_mode1.c \
	  -o $(BD)/tgm1a/test_gen_matrix_mode1.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tgm1a/*.o

test-gen-matrix-mode1: bin/test_gen_matrix1_scalar bin/test_gen_matrix1_avx
	@chmod +x tools/compare_gen_matrix_mode1.sh
	@USE_AVX_NTT128=0 USE_AVX_GEN_MATRIX128=0 $(MAKE) -s bin/test_gen_matrix1_scalar
	@USE_AVX_NTT128=1 USE_AVX_GEN_MATRIX128=1 $(MAKE) -s bin/test_gen_matrix1_avx
	@tools/compare_gen_matrix_mode1.sh 50

bin/test_gen_matrix3_scalar: tools/test_gen_matrix_mode3.c
	@mkdir -p bin
	$(call compile_objs,tgm3s,$(WEAVER_C) $(filter-out src/indcpa_gen_matrix_avx7681.c,$(MODE7681_C)),3,,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/test_gen_matrix_mode3.c \
	  -o $(BD)/tgm3s/test_gen_matrix_mode3.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tgm3s/*.o

bin/test_gen_matrix3_avx: tools/test_gen_matrix_mode3.c
	@mkdir -p bin
	$(call compile_objs,tgm3a,$(WEAVER_C) $(MODE7681_C),3,$(MODE7681_DEFS),$(MODE7681_S))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) $(MODE7681_DEFS) -c tools/test_gen_matrix_mode3.c \
	  -o $(BD)/tgm3a/test_gen_matrix_mode3.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/tgm3a/*.o

test-gen-matrix-mode3: bin/test_gen_matrix3_scalar bin/test_gen_matrix3_avx
	@chmod +x tools/compare_gen_matrix_mode3.sh
	@USE_AVX_GEN_MATRIX7681=0 $(MAKE) -s bin/test_gen_matrix3_scalar
	@USE_AVX_GEN_MATRIX7681=1 $(MAKE) -s bin/test_gen_matrix3_avx
	@tools/compare_gen_matrix_mode3.sh 50

bench-components: bin/bench_components_512 bin/bench_components_1024 bin/bench_components_2048
	@echo "========== component benchmarks (modes 1, 3, 5) =========="
	@bin/bench_components_512
	@bin/bench_components_1024
	@bin/bench_components_2048

bin/bench_q12289_saber: bench/bench_q12289_saber.c bench/cpucycles.c bench/speed_print.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -DNTESTS=$(BENCH_NTESTS) $(INC) -o $@ $^

bench-q12289-saber: bin/bench_q12289_saber
	@bin/bench_q12289_saber

bin/bench_q7681: bench/bench_q7681.c bench/cpucycles.c bench/speed_print.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -DNTESTS=$(BENCH_NTESTS) $(INC) -o $@ $^

bench-q7681: bin/bench_q7681
	@bin/bench_q7681

bin/bench_q7681_i16: bench/bench_q7681_i16.c bench/cpucycles.c bench/speed_print.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -DNTESTS=$(BENCH_NTESTS) -DCSELFTESTS=$(BENCH_CSELFTESTS) $(INC) -o $@ $^

bench-q7681-i16: bin/bench_q7681_i16
	@bin/bench_q7681_i16

bin/profile_encap_512:
	$(call link_profile_encap,1,512,$(MODE1_C),$(MODE1_DEFS),)

bin/profile_encap_1024:
	$(call link_profile_encap,3,1024,$(MODE7681_C),$(MODE7681_DEFS),$(MODE7681_S))

profile-encap: bin/profile_encap_512 bin/profile_encap_1024
	@echo "========== encaps/decaps profile (modes 1, 3) =========="
	@bin/profile_encap_512
	@bin/profile_encap_1024

clean:
	rm -rf bin build test_vectors/*.vec
