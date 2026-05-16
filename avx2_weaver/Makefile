# WeaverKEM AVX2 — make all | test | bench | clean

CC := clang
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  ARCH_FLAGS = -arch x86_64
else
  ARCH_FLAGS =
endif

CFLAGS_COMMON = -std=c99 -Wall -O3 -fomit-frame-pointer $(ARCH_FLAGS)
CFLAGS  = $(CFLAGS_COMMON) -mavx2 -mbmi2 -mpopcnt
# Set NO_INV_Q=1 to match submissions that omit poly_invq (canonical PK decompress)
ifeq ($(NO_INV_Q),1)
  CFLAGS += -DNO_INV_Q_LIFTING
endif
ASMFLAGS = $(CFLAGS) -x assembler-with-cpp
INC     = -Isrc -Isrc/avx -Ibench
LDFLAGS = $(ARCH_FLAGS)
BD      = build

ICCS256 = ../Implementations/Reference_Implementation/WeaverKEM-256
ICCS128 = ../Implementations/Reference_Implementation/WeaverKEM-128
REF_SRC = ../ref/src

WEAVER_C = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly_weaver.c src/cbd.c src/reduce.c src/verify.c \
	src/fips202.c src/symmetric-shake.c src/randombytes_ctr.c src/aes256ctr.c src/poly_invq.c

WEAVER_C_NORNG = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly_weaver.c src/cbd.c src/reduce.c src/verify.c \
	src/fips202.c src/symmetric-shake.c src/poly_invq.c

AVX_C  = src/poly_avx.c src/avx/consts.c
AVX_S  = src/avx/ntt.S src/avx/invntt.S src/avx/basemul.S src/avx/fq.S src/avx/shuffle.S
NTT_C  = src/ntt.c
BENCH  = bench/speed_test.c bench/cpucycles.c bench/speed_print.c

.PHONY: all test bench gen_vectors ref_vectors clean

all: bin/weaver_avx_512 bin/weaver_avx_1024 bin/weaver_avx_2048 \
     bin/kat_iccs_256 bin/kat_iccs_128 \
     bin/gen_vectors_1024 bin/verify_vectors_1024

# --- helpers ---
define compile_objs
	@rm -rf $(BD)/$(1) && mkdir -p $(BD)/$(1)
	@for f in $(2); do \
	  $(CC) $(CFLAGS) -DWEAVER_MODE=$(3) $(INC) $(4) -c $$f \
	    -o $(BD)/$(1)/$$(basename $$f | sed 's/\.[^.]*$$/.o/'); \
	done
	@for f in $(5); do \
	  $(CC) $(ASMFLAGS) -DWEAVER_MODE=$(3) $(INC) $(4) -c $$f \
	    -o $(BD)/$(1)/$$(basename $$f .S).o; \
	done
endef

define link_bin
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $(1) $(BD)/$(2)/*.o
endef

# Ensure link uses clang not plain cc
LINK = $(CC)

# --- benchmarks ---
# Modes 1 & 3: portable C NTT (matches ICCS KAT). AVX2 assembly port is WIP — see README.
bin/weaver_avx_512:
	$(call compile_objs,w512,$(WEAVER_C) $(NTT_C),1,,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_test.c -o $(BD)/w512/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/cpucycles.c -o $(BD)/w512/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_print.c -o $(BD)/w512/speed_print.o
	$(call link_bin,$@,w512)

bin/weaver_avx_1024:
	$(call compile_objs,w1024,$(WEAVER_C) $(NTT_C),3,,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_test.c -o $(BD)/w1024/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/cpucycles.c -o $(BD)/w1024/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_print.c -o $(BD)/w1024/speed_print.o
	$(call link_bin,$@,w1024)

bin/weaver_avx_2048:
	$(call compile_objs,w2048,$(WEAVER_C) $(NTT_C),5,,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_test.c -o $(BD)/w2048/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/cpucycles.c -o $(BD)/w2048/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_print.c -o $(BD)/w2048/speed_print.o
	$(call link_bin,$@,w2048)

# --- ICCS KAT verify ---
bin/kat_iccs_256: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat256,$(WEAVER_C_NORNG) $(NTT_C) $(ICCS256)/drng.c $(ICCS256)/auxfunc.c,3,-I$(ICCS256),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -I$(ICCS256) -c tools/kat_iccs.c -o $(BD)/kat256/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat256/*.o

bin/kat_iccs_128: tools/kat_iccs.c
	@mkdir -p bin
	$(call compile_objs,kat128,$(WEAVER_C_NORNG) $(NTT_C) $(ICCS128)/drng.c $(ICCS128)/auxfunc.c,1,-I$(ICCS128),)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -I$(ICCS128) -c tools/kat_iccs.c -o $(BD)/kat128/kat_iccs.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/kat128/*.o

# --- extended vectors ---
bin/gen_vectors_512:
	$(call compile_objs,gv512,$(WEAVER_C) $(NTT_C),1,,)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c tools/gen_vectors.c -o $(BD)/gv512/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv512/*.o

bin/gen_vectors_1024:
	$(call compile_objs,gv1024,$(WEAVER_C) $(NTT_C),3,,)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/gen_vectors.c -o $(BD)/gv1024/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv1024/*.o

bin/gen_vectors_2048:
	$(call compile_objs,gv2048,$(WEAVER_C) $(NTT_C),5,,)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c tools/gen_vectors.c -o $(BD)/gv2048/gen_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/gv2048/*.o

bin/verify_vectors_1024: tools/verify_vectors.c
	$(call compile_objs,vv1024,$(WEAVER_C) $(NTT_C),3,,)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/verify_vectors.c -o $(BD)/vv1024/verify_vectors.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/vv1024/*.o

bin/compare_ref_avx_1024: tools/compare_ref_avx.c
	$(call compile_objs,cmp1024,$(WEAVER_C) $(NTT_C),3,,)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c tools/compare_ref_avx.c -o $(BD)/cmp1024/compare_ref_avx.o
	$(CC) $(LDFLAGS) -o $@ $(BD)/cmp1024/*.o

bin/gen_vectors_ref_1024: tools/gen_vectors.c
	@mkdir -p bin
	@rm -rf $(BD)/refgv && mkdir -p $(BD)/refgv
	@for f in $(REF_SRC)/indcpa.c $(REF_SRC)/kem_cca.c $(REF_SRC)/msgenc.c \
	  $(REF_SRC)/bch_high.c $(REF_SRC)/bch_low.c $(REF_SRC)/polyvec.c $(REF_SRC)/poly.c \
	  $(REF_SRC)/ntt.c $(REF_SRC)/cbd.c $(REF_SRC)/reduce.c $(REF_SRC)/verify.c \
	  $(REF_SRC)/fips202.c $(REF_SRC)/symmetric-shake.c src/randombytes_ctr.c src/aes256ctr.c \
	  $(REF_SRC)/poly_invq.c; do \
	  $(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=3 -I$(REF_SRC) -c $$f -o $(BD)/refgv/$$(basename $$f .c).o; \
	done
	$(CC) $(CFLAGS_COMMON) -DWEAVER_MODE=3 -I$(REF_SRC) -c tools/gen_vectors.c -o $(BD)/refgv/gen_vectors_tool.o
	$(CC) $(ARCH_FLAGS) -o $@ $(BD)/refgv/*.o

gen_vectors: bin/gen_vectors_512 bin/gen_vectors_1024 bin/gen_vectors_2048
	@mkdir -p test_vectors
	bin/gen_vectors_512 test_vectors/weaver_avx_mode1.vec
	bin/gen_vectors_1024 test_vectors/weaver_avx_mode3.vec
	bin/gen_vectors_2048 test_vectors/weaver_avx_mode5.vec

ref_vectors: bin/gen_vectors_ref_1024
	@mkdir -p test_vectors
	bin/gen_vectors_ref_1024 test_vectors/weaver_ref_mode3.vec

test: all gen_vectors ref_vectors bin/verify_vectors_1024 bin/compare_ref_avx_1024
	@echo "=== ICCS KAT 256 ===" && bin/kat_iccs_256 ../Test_Vectors/KAT_KEM_WeaverKEM-256.txt
	@echo "=== ICCS KAT 128 ===" && bin/kat_iccs_128 ../Test_Vectors/KAT_KEM_WeaverKEM-128.txt
	@echo "=== Ref vs AVX ===" && bin/compare_ref_avx_1024 test_vectors/weaver_ref_mode3.vec
	@echo "=== AVX self-check ===" && bin/verify_vectors_1024 test_vectors/weaver_avx_mode3.vec

bench: all
	@echo "========== AVX2 Weaver (median cycles, 10000 iters) =========="
	@for x in 512 1024 2048; do echo "--- WEAVER-$$x ---"; bin/weaver_avx_$$x; done

clean:
	rm -rf bin build test_vectors/*.vec
