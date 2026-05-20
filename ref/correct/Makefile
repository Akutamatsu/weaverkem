CC=/usr/bin/gcc
VPATH = ../src/:../src/speed
CFLAGS += -O3 -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto
# NOTE: 'kem_cpa.c' used for debug. Change to 'kem_cca.c' for complete FO. 
SRC= correct_test.c cbd.c fips202.c indcpa.c kem_cca.c ntt.c poly.c polyvec.c reduce.c rng.c verify.c symmetric-shake.c bch_high.c bch_low.c msgenc.c poly_invq.c
INC= api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h bch.h invq.h

# 输出文件名改为适配 WEAVER 论文的三个实例 
BIN1= 512-failure
BIN2= 768-failure
BIN3= 1024-failure
BIN= $(BIN1) $(BIN2) $(BIN3)

all:$(BIN) 

# Level 1 (WEAVER-512): 对应 WEAVER_MODE=1 (k=2)
$(BIN1):$(SRC)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 -o $@ $^ $(LDFLAGS) -I../src

# Level 3 (WEAVER-768): 对应 WEAVER_MODE=3 (k=3)
$(BIN2):$(SRC)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 -o $@ $^ $(LDFLAGS) -I../src

# Level 5 (WEAVER-1024): 对应 WEAVER_MODE=5 (k=4)
$(BIN3):$(SRC)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 -o $@ $^ $(LDFLAGS) -I../src

$(SRC):$(INC)

.PHONY: clean

clean:
	-rm $(BIN) *.rsp
