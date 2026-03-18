CC=/usr/bin/gcc
VPATH = ../src/:../src/speed
CFLAGS += -g -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto
# NOTE: 'kem_cpa.c' used for debug. Change to 'kem.c' for complete FO. 
SRC= correct_test.c cbd.c fips202.c indcpa.c kem_cpa.c ntt.c poly.c polyvec.c reduce.c rng.c verify.c symmetric-shake.c bch_high.c bch_low.c
INC= api.h cbd.h fips202.h indcpa.h ntt.h params.h poly.h polyvec.h reduce.h rng.h verify.h symmetric.h bch.h

# 输出文件名改为适配 WEAVER 论文的三个实例 (512, 1024, 2048)
BIN1= 512-failure
BIN2= 1024-failure
BIN3= 2048-failure
BIN= $(BIN1) $(BIN2) $(BIN3)

all:$(BIN) 

# Level 1 (WEAVER-512): 对应 WEAVER_MODE=1
$(BIN1):$(SRC)
	$(CC) $(CFLAGS) -DWEAVER_MODE=1 -o $@ $^ $(LDFLAGS) -I../src

# Level 3 (WEAVER-1024): 对应 WEAVER_MODE=3
$(BIN2):$(SRC)
	$(CC) $(CFLAGS) -DWEAVER_MODE=3 -o $@ $^ $(LDFLAGS) -I../src

# Level 5 (WEAVER-2048): 对应 WEAVER_MODE=5
$(BIN3):$(SRC)
	$(CC) $(CFLAGS) -DWEAVER_MODE=5 -o $@ $^ $(LDFLAGS) -I../src

$(SRC):$(INC)

.PHONY: clean

clean:
	-rm $(BIN) *.rsp
