CC=/usr/bin/gcc
VPATH = ../src/:../src/speed
CFLAGS += -g -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto
# NOTE: 'kem_cpa.c' used for debug. Change to 'kem.c' for complete FO. 
SRC= correct_test.c cbd.c fips202.c indcpa.c kem_cpa.c ntt.c poly.c polyvec.c reduce.c rng.c verify.c symmetric-shake.c
INC= api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h
BIN1= 512-failure
BIN2= 768-failure
BIN3= 1024-failure
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

