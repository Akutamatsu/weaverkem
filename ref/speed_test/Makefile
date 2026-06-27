CC=/usr/bin/gcc
VPATH = ../src/:../src/speed
CFLAGS += -O3 -march=native -fomit-frame-pointer 
#-DALL_TESTS
LDFLAGS=-lcrypto
# PQCgenKAT_kem.c 
SRC= cpucycles.c speed_print.c test_speed.c cbd.c indcpa.c kem_cca.c ntt.c poly.c polyvec.c reduce.c verify.c bch_high.c bch_low.c msgenc.c poly_invq.c
INC= cpucycles.h speed_print.h api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h bch.h invq.h
KECCAK_SOURCES = $(SRC) fips202.c symmetric-shake.c rng.c
ICCS_SOURCES = $(SRC) auxfunc.c symmetric-iccs.c drng.c

.PHONY: all speed shared clean

all: speed

speed: \
  640-speed \
  1024-speed \
  2048-speed \
  640-speedshake \
  1024-speedshake \
  2048-speedshake

640-speed:$(ICCS_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 -o $@ $^ $(LDFLAGS) -I../src

1024-speed:$(ICCS_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 -o $@ $^ $(LDFLAGS) -I../src

2048-speed:$(ICCS_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 -o $@ $^ $(LDFLAGS) -I../src

640-speedshake:$(KECCAK_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 -DWEAVER_USE_SHAKE -o $@ $^ $(LDFLAGS) -I../src

1024-speedshake:$(KECCAK_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 -DWEAVER_USE_SHAKE -o $@ $^ $(LDFLAGS) -I../src

2048-speedshake:$(KECCAK_SOURCES)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 -DWEAVER_USE_SHAKE -o $@ $^ $(LDFLAGS) -I../src

$(SRC):$(INC)

clean:
	-rm -f 640-speed
	-rm -f 1024-speed
	-rm -f 2048-speed
	-rm -f 640-speedshake
	-rm -f 1024-speedshake
	-rm -f 2048-speedshake

