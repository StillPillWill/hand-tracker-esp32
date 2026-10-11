#pragma once

// ESP-IDF 5 removed the old hwcrypto/sha.h path. The upstream server only
// uses esp_sha for its optional WebSocket handshake; retain that API using
// the Mbed TLS SHA-1 function on newer ESP32 Arduino cores.
#include <mbedtls/sha1.h>

static inline void esp_sha(esp_sha_type type, const unsigned char *input,
                           size_t length, unsigned char *output) {
  if (type == SHA1) (void)mbedtls_sha1_ret(input, length, output);
}
