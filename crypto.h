#ifndef _AOL_UTIL_H_
#define _AOL_UTIL_H_

#ifdef __APPLE__
/* Apple has deprecated openssl in favor of Common Crypto.  Ignore for now. */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

char*  sign(char *hash_string, const char *key);
char *base64decode(char *base64encoded, size_t length);
char *base64encode(char *normal, size_t length);
char *HMACSHA1(const unsigned char *hash_string, void *key, size_t key_len) ;
#endif
