#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "api.h"
#include "ecc.h"
#include "rand.h"
#include <stdlib.h>

#define CTESTS 10000
uint64_t loop=1;

static void print_uint64(unsigned long long num)
{
	if(num>=10)
		print_uint64(num/10);
	printf("%u",(unsigned int)(num%10));
}

int error_bit_num(unsigned char *k1, unsigned char *k2, int num)
{
	int i,sum=0;
	unsigned char temp;
	for(i=0;i<num;i++)
	{
		temp=k1[i]^k2[i];
		if(temp>0)
		{
			sum+=(temp&0x1);
			sum+=((temp>>1)&0x1);
			sum+=((temp>>2)&0x1);
			sum+=((temp>>3)&0x1);
			sum+=((temp>>4)&0x1);
			sum+=((temp>>5)&0x1);
			sum+=((temp>>6)&0x1);
			sum+=((temp>>7)&0x1);
		}
	}
	
	return sum;
}
int print_error_bit(unsigned char *k1, unsigned char *k2, int num)
{
	int i,sum=0;
	unsigned char temp;
	printf("\nerror bit:\n");
	for(i=0;i<num;i++)
	{
		temp=k1[i]^k2[i];
		printf("%d%d%d%d%d%d%d%d",temp&0x1,((temp>>1)&0x1),((temp>>2)&0x1),((temp>>3)&0x1),((temp>>4)&0x1),((temp>>5)&0x1),((temp>>6)&0x1),((temp>>7)&0x1));
	}
	printf("\n");
	
	return sum;
}

//print bytes
int print_bytes(unsigned char *buf, int len)
{
	int i;
	for(i=0;i<len;i++)
	{
		printf("%d ",buf[i]);
	}
	printf("\n");
	
	return 0;
}

//test correctness of pke
int test_pke_correctness()
{

	unsigned char pk[CRYPTO_PUBLICKEYBYTES];
	unsigned char sk[CRYPTO_SECRETKEYBYTES];
	unsigned char k1[CRYPTO_BYTES],k2[CRYPTO_BYTES],c[CRYPTO_CIPHERTEXTBYTES];
	int i,j;
	long long int  error_bit,sum=0;
	long long int  error_num=0,sum_bits;
	unsigned long long mlen=MESSAGE_LEN,clen=CRYPTO_CIPHERTEXTBYTES;
	
	printf("correctness test of pke:\n");
	for(j=0;j<loop;j++)
	{
		crypto_encrypt_keypair(pk,sk);
		random_bytes(k1,CRYPTO_BYTES);
		for(i=0;i<CTESTS;i++)
		{
			crypto_encrypt(c,&clen,k1,mlen,pk);
			crypto_encrypt_open(k2,&mlen,c,clen,sk);
			//printf("i=%d\n",i);
			if(memcmp(k1,k2,mlen)!=0)
			{
				error_num++;
				error_bit=error_bit_num(k1,k2,mlen);
				sum+=error_bit;
				if(error_bit>0)
				{
					printf("error bit num:");
					print_uint64(error_bit);
					printf("\n");
					print_error_bit(k1,k2,mlen);
				}
			}
		}
		printf("test %d error block:",j+1);
		print_uint64(error_num);
		printf(" error bit:");
		print_uint64(sum);
		printf("\n");

	}
    sum_bits=CTESTS*loop*CRYPTO_BYTES*8;
	printf("total error bit:");
	print_uint64(sum);
	printf("/");
	print_uint64(sum_bits);
	printf("\n\n");
	
	return error_num;
}

//test kem fo correctness
int test_kem_fo_correctness()
{
	unsigned char pk[CRYPTO_PUBLICKEYBYTES];
	unsigned char sk[CRYPTO_SECRETKEYBYTES];
	unsigned char k1[CRYPTO_BYTES],k2[CRYPTO_BYTES],c[CRYPTO_CIPHERTEXTBYTES];
	int i,j;
	long long int  error_num=0;
	
	printf("correctness test of kem_fo:\n");
	for(j=0;j<loop;j++)
	{
		crypto_kem_keypair(pk,sk);
		random_bytes(k1,CRYPTO_BYTES);
		for(i=0;i<CTESTS;i++)
		{
			crypto_kem_enc(c,k1,pk);
			crypto_kem_dec(k2,c,sk);
			
			if(memcmp(k1,k2,CRYPTO_BYTES)!=0)
			{
				error_num++;
			}
			
		}
		printf("test %d error block:",j+1);
		print_uint64(error_num);
		printf("\n");
	}
	printf("\n");

	return error_num;
}

//test  ke correctness
int test_ke_correctness()
{
	unsigned char pk[CRYPTO_PUBLICKEYBYTES];
	unsigned char sk[CRYPTO_SECRETKEYBYTES];
	unsigned char k1[CRYPTO_BYTES],k2[CRYPTO_BYTES],c[CRYPTO_CIPHERTEXTBYTES];
	int i,j;
	long long int  error_num=0;
	
	printf("correctness test of ke:\n");
	for(j=0;j<loop;j++)
	{
		for(i=0;i<CTESTS;i++)
		{
			crypto_ke_alice_send(pk,sk);
			crypto_ke_bob_receive(pk,c,k1);
			crypto_ke_alice_receive(pk,sk,c,k2);
			if(memcmp(k1,k2,CRYPTO_BYTES)!=0)
			{
				error_num++;
			}
		}
		printf("test %d error block:",j+1);
		print_uint64(error_num);
		printf("\n");
	}
	printf("\n");
	
	return error_num;
}

//test  ake correctness
int test_ake_correctness()
{
	unsigned char pk_a[CRYPTO_PUBLICKEYBYTES],pk_b[CRYPTO_PUBLICKEYBYTES],pk[CRYPTO_PUBLICKEYBYTES];
	unsigned char sk[CRYPTO_SECRETKEYBYTES],sk_a[CRYPTO_SECRETKEYBYTES],sk_b[CRYPTO_SECRETKEYBYTES];
	unsigned char k_a[CRYPTO_BYTES],k_b[CRYPTO_BYTES],c_a[CRYPTO_CIPHERTEXTBYTES],c_b[2*CRYPTO_CIPHERTEXTBYTES],k1[CRYPTO_BYTES];
	int i,j;
	long long int  error_num=0;
	
	// generate public parameter  a and the key pair of alice and bob
	crypto_encrypt_keypair(pk_a,sk_a);
	crypto_encrypt_keypair(pk_b,sk_b);
	printf("correctness test of ake:\n");
	for(j=0;j<loop;j++)
	{
		for(i=0;i<CTESTS;i++)
		{
			crypto_ake_alice_send(pk,sk,pk_b,sk_a,c_a,k1);
			
			crypto_ake_bob_receive(pk_b,sk_b,pk_a,pk,c_a,c_b,k_b);
			
			crypto_ake_alice_receive(pk_a,sk_a,pk_b,pk,sk,c_a,c_b,k1,k_a);
			
			if(memcmp(k_b,k_a,CRYPTO_BYTES)!=0)
			{
				error_num++;
			}
		}
		printf("test %d error block:",j+1);
		print_uint64(error_num);
		printf("\n");
	}
	printf("\n");

	return error_num;
}

#include <string.h> // 需要用到 memcmp 和 memcpy
#include <stdio.h>

int test_bch_correctness()
{
    int i, err_count,BCH_T=4;
    unsigned char data[DATA_LEN];
    unsigned char orig_data[DATA_LEN]; // 存储原始数据的副本
    unsigned char code[CODE_LEN];
    
    // 假设当前 BCH_T 为 4
    printf("Testing BCH Correctness (Max Errors: %d)\n", BCH_T);
    printf("------------------------------------------------------\n");

    // 我们可以测试 0 到 BCH_T+1 个错误（故意越界测试一下）
    for(err_count = 0; err_count <= BCH_T + 1; err_count++)
    {
        int success_count = 0;
        int fail_count = 0;
        int miscorrect_count = 0;
        
        for(i = 0; i < 10000; i++)
        {
            // 1. 生成随机数据并备份
            random_bytes(data, DATA_LEN);
            memcpy(orig_data, data, DATA_LEN); // 备份绝对正确的明文
            
            // 2. 编码
            ecc_enc(data, code);
            
            // 3. 注入特定数量的比特反转错误
            int e = 0;
            int flipped[CODE_LEN * 8] = {0};
            while(e < err_count) 
            {
                int bit_pos = rand() % (CODE_LEN * 8);
                if (!flipped[bit_pos]) 
                {
                    flipped[bit_pos] = 1;
                    code[bit_pos / 8] ^= (1 << (bit_pos % 8)); // 翻转
                    e++;
                }
            }
            
            // 4. 解码并获取返回值
            int decoded_err_num = ecc_dec(data, code);
            
            // 5. 验证正确性
            // 5. 验证正确性：只关心明文（DATA）是否完美恢复
            int is_data_match = (memcmp(orig_data, data, DATA_LEN) == 0);
            
            if (is_data_match) {
                // 只要明文没坏，或者被完美修好了，就是绝对的成功
                success_count++;
            } else if (decoded_err_num < 0) {
                // 算法明确报告失败（在 LAC 中极少出现，因为是恒时的）
                fail_count++;
            } else {
                // 明文数据没对上，发生了误纠（超过纠错能力时预期会发生）
                miscorrect_count++;
            }
        }
        
        printf("Injected Errors: %d | Success: %5d | Failed: %5d | Miscorrected: %5d\n", 
               err_count, success_count, fail_count, miscorrect_count);
    }
    
    printf("------------------------------------------------------\n\n");
    return 0;
}