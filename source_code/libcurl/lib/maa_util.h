#ifndef HEADER_CURL_MAA_UTIL_H
#define HEADER_CURL_MAA_UTIL_H

#include <stdint.h>
#include <zlib.h>

void maa_util_xxtea_encode(unsigned char *to_encrypt, uint32_t len, uint32_t *ret_len);

int maa_util_gzcompress(Bytef *zdata, uLong *nzdata, Bytef *data, uLong ndata);

#endif /* HEADER_CURL_MAA_UTIL_H */
