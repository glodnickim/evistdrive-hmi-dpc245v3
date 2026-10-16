/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include <stdint.h>
void *memset(void *d,int c,size_t n){unsigned char *p=d;while(n--)*p++=(unsigned char)c;return d;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *o=d;const unsigned char *i=s;while(n--)*o++=*i++;return d;}
static uint32_t udiv32(uint32_t n,uint32_t d,uint32_t *rem){
    if(!d){if(rem)*rem=n;return 0xFFFFFFFFu;}
    uint32_t q=0,r=0;
    for(int i=31;i>=0;i--){r=(r<<1)|((n>>(unsigned)i)&1u);if(r>=d){r-=d;q|=1u<<(unsigned)i;}}
    if(rem)*rem=r;return q;
}
uint32_t __aeabi_uidiv(uint32_t n,uint32_t d){return udiv32(n,d,0);}
typedef struct { uint32_t quot, rem; } udivmod_t;
udivmod_t __aeabi_uidivmod(uint32_t n,uint32_t d){udivmod_t x; x.quot=udiv32(n,d,&x.rem); return x;}
int32_t __aeabi_idiv(int32_t n,int32_t d){
    uint32_t un=n<0?(uint32_t)(-(n+1))+1u:(uint32_t)n, ud=d<0?(uint32_t)(-(d+1))+1u:(uint32_t)d;
    uint32_t q=udiv32(un,ud,0); return ((n<0)^(d<0))?-(int32_t)q:(int32_t)q;
}
typedef struct { int32_t quot, rem; } idivmod_t;
idivmod_t __aeabi_idivmod(int32_t n,int32_t d){idivmod_t x; x.quot=__aeabi_idiv(n,d); x.rem=n-x.quot*d; return x;}
void __aeabi_memclr(void *d,size_t n){(void)memset(d,0,n);}
void __aeabi_memclr4(void *d,size_t n){(void)memset(d,0,n);}
void __aeabi_memclr8(void *d,size_t n){(void)memset(d,0,n);}
void __aeabi_memcpy(void *d,const void *s,size_t n){(void)memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n){(void)memcpy(d,s,n);}
void __aeabi_memcpy8(void *d,const void *s,size_t n){(void)memcpy(d,s,n);}
void *memmove(void *d,const void *s,size_t n){unsigned char *o=d;const unsigned char *i=s;if(o==i||n==0u)return d;if(o<i){for(size_t k=0;k<n;k++)o[k]=i[k];}else{for(size_t k=n;k>0u;k--)o[k-1u]=i[k-1u];}return d;}
void __aeabi_memmove(void *d,const void *s,size_t n){(void)memmove(d,s,n);}
void __aeabi_memmove4(void *d,const void *s,size_t n){(void)memmove(d,s,n);}
void __aeabi_memmove8(void *d,const void *s,size_t n){(void)memmove(d,s,n);}
