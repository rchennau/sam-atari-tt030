#ifndef SHA256_H
#define SHA256_H

struct sha256 {
    unsigned long h[8], len;
    unsigned char buf[64];
    int fill;
};

void sha256_init(struct sha256 *s);
void sha256_update(struct sha256 *s, const unsigned char *p, unsigned long n);
void sha256_final(struct sha256 *s, unsigned char *out);

#endif
