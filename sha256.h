#ifndef SHA256_H
#define SHA256_H

#define SHA256_BLOCK_SIZE 32

// SHA256 context structure
typedef struct {
    unsigned char data[64];
    unsigned int datalen;
    unsigned long long bitlen;
    unsigned int state[8];
} SHA256_CTX;

// Function declarations
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const unsigned char data[], unsigned int len);
void sha256_final(SHA256_CTX *ctx, unsigned char hash[]);
void sha256_hash(const unsigned char *data, unsigned int len, unsigned char hash[]);
void sha256_to_hex(const unsigned char hash[], char hex_output[]);

#endif // SHA256_H
