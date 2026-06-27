CC=/usr/bin/gcc
VPATH = ../src/:../src/speed
CFLAGS += -O3 -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto
# NOTE: 'kem_cpa.c' used for debug. Change to 'kem_cca.c' for complete FO. 
SRC= cbd.c indcpa.c kem_cca.c ntt.c poly.c polyvec.c reduce.c verify.c bch_high.c bch_low.c msgenc.c poly_invq.c
INC= api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h bch.h invq.h
KECCAK_SOURCES = $(SRC) fips202.c symmetric-shake.c rng.c
ICCS_SOURCES = $(SRC) auxfunc.c symmetric-iccs.c drng.c

CORRECT_SOURCES = $(KECCAK_SOURCES) correct_test.c
KAT_SOURCES = $(ICCS_SOURCES) KEM_WeaverKEM.c KAT_KEM.c

.PHONY: all kat correct clean

all: correct kat

correct: \
  640-failure \
  1024-failure \
  2048-failure

kat: \
  640-kat \
  1024-kat \
  2048-kat

640-kat:$(KAT_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 -o $@ $^ -I../src

1024-kat:$(KAT_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 -o $@ $^ -I../src

2048-kat:$(KAT_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 -o $@ $^ -I../src

640-failure:$(CORRECT_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 -DWEAVER_USE_SHAKE -o $@ $^ $(LDFLAGS) -I../src

1024-failure:$(CORRECT_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 -DWEAVER_USE_SHAKE -o $@ $^ $(LDFLAGS) -I../src

2048-failure:$(CORRECT_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 -DWEAVER_USE_SHAKE -o $@ $^ $(LDFLAGS) -I../src

$(SRC):$(INC)

clean:
	-rm -f 640-failure
	-rm -f 1024-failure
	-rm -f 2048-failure
	-rm -f 640-kat
	-rm -f 1024-kat
	-rm -f 2048-kat
