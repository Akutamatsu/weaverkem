CC=/usr/bin/gcc
VPATH = ../src/:.
CFLAGS += -O3 -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto
SRC= cbd.c fips202.c indcpa.c kem.c ntt.c poly.c polyvec.c PQCgenKAT_kem.c reduce.c rng.c verify.c symmetric-shake.c
INC= api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h
BIN1= 512-PQCgenKAT_kem
BIN2= 768-PQCgenKAT_kem
BIN3= 1024-PQCgenKAT_kem
BIN= $(BIN1) $(BIN2) $(BIN3)

all:$(BIN) 

$(BIN1):$(SRC)
	$(CC) $(CFLAGS) -DKYBER_K=2 -o $@ $^ $(LDFLAGS) -I../src

$(BIN2):$(SRC)
	$(CC) $(CFLAGS) -DKYBER_K=3 -o $@ $^ $(LDFLAGS) -I../src

$(BIN3):$(SRC)
	$(CC) $(CFLAGS) -DKYBER_K=4 -o $@ $^ $(LDFLAGS) -I../src

.PHONY: clean

clean:
	-rm $(BIN)

