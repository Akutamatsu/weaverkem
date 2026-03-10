CC=/usr/bin/gcc
VPATH = ../src/:../src/speed
CFLAGS += -O3 -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto
# PQCgenKAT_kem.c 
SRC= cpucycles.c speed_print.c test_speed.c cbd.c fips202.c indcpa.c kem.c ntt.c poly.c polyvec.c reduce.c rng.c verify.c symmetric-shake.c
INC= cpucycles.h speed_print.h api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h
BIN1= 512-speed_cca
BIN2= 768-speed_cca
BIN3= 1024-speed_cca
BIN= $(BIN1) $(BIN2) $(BIN3)

all:$(BIN) 

$(BIN1):$(SRC)
	$(CC) $(CFLAGS) -DKYBER_K=2 -o $@ $^ $(LDFLAGS) -I../src

$(BIN2):$(SRC)
	$(CC) $(CFLAGS) -DKYBER_K=3 -o $@ $^ $(LDFLAGS) -I../src

$(BIN3):$(SRC)
	$(CC) $(CFLAGS) -DKYBER_K=4 -o $@ $^ $(LDFLAGS) -I../src

$(SRC):$(INC)

.PHONY: clean

clean:
	-rm $(BIN)

