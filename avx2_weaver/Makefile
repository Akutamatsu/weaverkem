# WeaverKEM AVX2 — four parameter-set benchmarks

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
ASMFLAGS = $(CFLAGS) -x assembler-with-cpp
INC     = -Isrc -Isrc/avx -Ibench
LDFLAGS = $(ARCH_FLAGS)
ifeq ($(ENABLE_LTO),1)
  LDFLAGS += -flto
  # Keccak x4 / rejsample / compress: avoid LTO miscompilation with gen_matrix (mode 1).
  NO_LTO_C = src/fips202x4.c src/rejsample.c src/indcpa_gen_matrix_avx.c \
             src/poly_compress_avx.c src/poly_compress512.c \
             src/keccak4x/KeccakP-1600-times4-SIMD256.c
endif
# Keep the selected compression and XOF files outside LTO.
CFLAGS_NO_LTO = $(filter-out -flto,$(CFLAGS)) -fno-lto
BD      = build

# Windows system RNG uses CryptoAPI; Unix needs no extra RNG library.
ifeq ($(OS),Windows_NT)
LDLIBS += -ladvapi32
endif

WEAVER_C = \
	src/indcpa.c src/kem_cca.c src/msgenc.c src/bch_high.c src/bch_low.c \
	src/polyvec.c src/poly_weaver.c src/cbd.c src/reduce.c \
	src/verify.c src/fips202.c src/symmetric-shake.c src/randombytes.c \
	src/poly_invq.c

AVX_C  = src/poly_avx.c src/avx/consts.c
AVX_S  = src/avx/ntt.S src/avx/invntt.S src/avx/basemul.S src/avx/fq.S src/avx/shuffle.S
NTT_C  = src/ntt.c
BENCH  = bench/speed_test.c bench/cpucycles.c bench/speed_print.c

SAN ?= 0
ifeq ($(SAN),1)
CFLAGS_SAN = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
CFLAGS := $(CFLAGS_SAN) -mavx2 -mbmi2 -mpopcnt
LDFLAGS += -fsanitize=address,undefined
ENABLE_LTO := 0
endif

# AVX2 is mandatory; select the implementation by polynomial dimension.
KECCAK4X_C = src/fips202x4.c src/rejsample.c src/indcpa_gen_matrix_avx.c \
 src/poly_compress_avx.c src/keccak4x/KeccakP-1600-times4-SIMD256.c
MODE13_C_NTT = $(AVX_C) $(KECCAK4X_C)
MODE13_S_NTT = $(AVX_S)
MODE7_C_NTT = $(NTT_C) src/poly_avx512.c src/poly_compress512.c \
 src/indcpa_gen_matrix_avx512.c src/fips202x4.c \
 src/keccak4x/KeccakP-1600-times4-SIMD256.c
NO_LTO_C += src/indcpa_gen_matrix_avx512.c

BINS = bin/weaver_avx_512 bin/weaver_avx_768 bin/weaver_avx_1024 bin/weaver_avx_2048
HEADERS = $(wildcard src/*.h src/avx/*.h src/avx/*.inc src/keccak4x/*.h src/keccak4x/*.macros)
SOURCES = $(sort $(WEAVER_C) $(MODE13_C_NTT) $(MODE7_C_NTT) $(MODE13_S_NTT) $(BENCH))
.PHONY: all clean
all: $(BINS)

define compile_objs
	@rm -rf $(BD)/$(1) && mkdir -p $(BD)/$(1)
	@for f in $(2); do \
	  obj=$$(echo $$f | sed 's#^\./##; s#^\.\./##; s#/#_#g; s/\.[^.]*$$/.o/'); \
	  cflags="$(CFLAGS) -DWEAVER_MODE=$(3) $(INC)"; \
	  if echo " $(NO_LTO_C) " | grep -q " $$f "; then \
	    cflags="$(CFLAGS_NO_LTO) -DWEAVER_MODE=$(3) $(INC)"; \
	  fi; \
	  $(CC) $$cflags -c $$f -o $(BD)/$(1)/$$obj || exit $$?; \
	done
	@for f in $(4); do \
	  obj=$$(echo $$f | sed 's#^\./##; s#^\.\./##; s#/#_#g; s/\.[^.]*$$/.o/'); \
	  $(CC) $(ASMFLAGS) -DWEAVER_MODE=$(3) $(INC) -c $$f \
	    -o $(BD)/$(1)/$$obj || exit $$?; \
	done
endef

define link_bin
	@mkdir -p bin
	$(CC) $(LDFLAGS) -o $(1) $(BD)/$(2)/*.o $(LDLIBS)
endef

# --- benchmarks ---
# Modes 1/3/5 use the n=256 AVX2 backend; mode 7 uses n=512 AVX2.
bin/weaver_avx_512:
	$(call compile_objs,w512,$(WEAVER_C) $(MODE13_C_NTT),1,$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_test.c -o $(BD)/w512/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/cpucycles.c -o $(BD)/w512/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=1 $(INC) -c bench/speed_print.c -o $(BD)/w512/speed_print.o
	$(call link_bin,$@,w512)

bin/weaver_avx_768:
	$(call compile_objs,w768,$(WEAVER_C) $(MODE13_C_NTT),3,$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_test.c -o $(BD)/w768/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/cpucycles.c -o $(BD)/w768/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=3 $(INC) -c bench/speed_print.c -o $(BD)/w768/speed_print.o
	$(call link_bin,$@,w768)

bin/weaver_avx_1024:
	$(call compile_objs,w1024,$(WEAVER_C) $(MODE13_C_NTT),5,$(MODE13_S_NTT))
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_test.c -o $(BD)/w1024/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/cpucycles.c -o $(BD)/w1024/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=5 $(INC) -c bench/speed_print.c -o $(BD)/w1024/speed_print.o
	$(call link_bin,$@,w1024)

bin/weaver_avx_2048:
	$(call compile_objs,w2048,$(WEAVER_C) $(MODE7_C_NTT),7,)
	@$(CC) $(CFLAGS) -DWEAVER_MODE=7 $(INC) -c bench/speed_test.c -o $(BD)/w2048/speed_test.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=7 $(INC) -c bench/cpucycles.c -o $(BD)/w2048/cpucycles.o
	@$(CC) $(CFLAGS) -DWEAVER_MODE=7 $(INC) -c bench/speed_print.c -o $(BD)/w2048/speed_print.o
	$(call link_bin,$@,w2048)

$(BINS): $(SOURCES) $(HEADERS) Makefile

clean:
	rm -f $(BINS)
	rm -rf $(BD)/w512 $(BD)/w768 $(BD)/w1024 $(BD)/w2048
