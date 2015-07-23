#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "maa_util.h"

#define MAA_XXTEA_MX    \
    ((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4) ^ (sum ^ y) + (k[p & 3 ^ e] ^ z))
#define MAA_XXTEA_DELTA 0x9e3779b9

/* "0123456789abcdef" */
static const uint32_t maa_xxtea_key[4] =
    {0x33323130, 0x37363534, 0x62613938, 0x66656463};
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void maa_util_xxtea_encrypt(uint32_t *v, uint32_t len, const uint32_t *k)
{
  uint32_t n = len - 1;
  uint32_t z = v[n], y = v[0], p, q = 6 + 52 / (n + 1), sum = 0, e;

  if (n < 1) {
    return;
  }

  while (0 < q--) {
    sum += MAA_XXTEA_DELTA;
    e = sum >> 2 & 3;
    for (p = 0; p < n; p++) {
      y = v[p + 1];
      z = v[p] += MAA_XXTEA_MX;
    }
    y = v[0];
    z = v[n] += MAA_XXTEA_MX;
  }
}

static uint32_t *maa_util_xxtea_2long_array(const unsigned char *data,
                    uint32_t len, uint32_t *ret_len)
{
  uint32_t i, n;
  uint32_t *result;

  n = len >> 2;
  n = (((len & 3) == 0) ? n : n + 1);

  result = (uint32_t *)malloc((n + 1) << 2); /* include length */
  if (result == NULL) {
    *ret_len = 0;
    return NULL;
  }
  result[n] = len;
  *ret_len = n + 1;

  memset(result, 0, n << 2);
  for (i = 0; i < len; i++) {
    result[i >> 2] |= (uint32_t)data[i] << ((i & 3) << 3);
  }

  return result;
}

static unsigned char *maa_util_xxtea_2byte_array(uint32_t *data, 
                        uint32_t len, uint32_t *ret_len)
{
  uint32_t i, n;
  unsigned char *result;

  n = len << 2;
  result = (unsigned char *)malloc(n + 1);
  if (result == NULL) {
    *ret_len = 0;
    return NULL;
  }
  *ret_len = n;

  for (i = 0; i < n; i++) {
    result[i] = (unsigned char)((data[i >> 2] >> ((i & 3) << 3)) & 0xff);
  }
  result[n] = '\0';

  return result;
}

static unsigned char *maa_util_base64_encode(const unsigned char *byte_array, 
                        uint32_t len)
{
  int i,j;
  unsigned char *buff;
  uint32_t buff_len;

  if (len % 3 == 0) {
    buff_len = len / 3 * 4;
  } else {
    buff_len = (len / 3 + 1) * 4;
  }

  buff = (unsigned char *)malloc(buff_len + 1);
  if (buff == NULL) {
    return NULL;
  }
  buff[buff_len] = '\0';

  for (i = 0, j = 0; i < buff_len - 2; j += 3, i += 4) {
    buff[i] = base64_table[byte_array[j] >> 2];
    buff[i + 1] = base64_table[(byte_array[j] & 0x3) << 4 | (byte_array[j + 1] >> 4)];
    buff[i + 2] = base64_table[(byte_array[j + 1] & 0xf) << 2 | (byte_array[j + 2] >> 6)];
    buff[i + 3] = base64_table[byte_array[j + 2] & 0x3f];
  }

  if (len % 3 == 1) {
    buff[i - 2] = '=';
    buff[i - 1] = '=';
  } else if (len % 3 == 2) {
    buff[i - 1] = '=';
  }
  return buff;
}

void maa_util_xxtea_encode(unsigned char *to_encrypt, uint32_t len, 
        uint32_t *ret_len)
{    
  if (to_encrypt && len > 0 && ret_len) {
    *ret_len = len; /* returns unencode length if failed */

    uint32_t *long_array;
    uint32_t long_array_len;

    long_array = maa_util_xxtea_2long_array(to_encrypt, len, &long_array_len);
    if (long_array) {
      maa_util_xxtea_encrypt(long_array, long_array_len, maa_xxtea_key);

      unsigned char *byte_array;
      uint32_t byte_array_len;

      byte_array = maa_util_xxtea_2byte_array(long_array, long_array_len,
                    &byte_array_len);
      if (byte_array) {
        unsigned char *base_str;
        uint32_t base_str_len;

        base_str = maa_util_base64_encode(byte_array, byte_array_len);
        if (base_str) {
          base_str_len = strlen((const char *)base_str);
          memcpy(to_encrypt, base_str, base_str_len);
          to_encrypt[base_str_len] = '\0';
          *ret_len = base_str_len;
          free(base_str);
        }
        free(byte_array);
      }
      free(long_array);
    }
  }
}

int maa_util_gzcompress(Bytef *zdata, uLong *nzdata, Bytef *data, uLong ndata)
{
  z_stream c_stream;
  int err = 0;

  if (data && ndata > 0) {
    c_stream.zalloc = NULL;
    c_stream.zfree = NULL;
    c_stream.opaque = NULL;

    if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
        MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
      return -1;
    }

    c_stream.next_in = data;
    c_stream.avail_in = ndata;
    c_stream.next_out = zdata;
    c_stream.avail_out = *nzdata;

    while (c_stream.avail_in != 0 && c_stream.total_out < *nzdata) {
      if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK) {
        return -1;
      }
    }
    if (c_stream.avail_in != 0) {
      return -1;
    }
    for (;;) {
      if ((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END) {
        break;
      }
      if (err != Z_OK) {
        return -1;
      }
    }
    if (deflateEnd(&c_stream) != Z_OK) {
      return -1;
    }
    *nzdata = c_stream.total_out;
    return 0;
  }

  return -1;
}
