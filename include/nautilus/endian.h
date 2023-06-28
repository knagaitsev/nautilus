#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include<nautilus/arch.h>

static inline uint16_t htobe16(uint16_t host) {
  return !arch_little_endian() ? host : __builtin_bswap16(host);
}
static inline uint16_t htole16(uint16_t host) {
  return arch_little_endian() ? host : __builtin_bswap16(host);
}
static inline uint16_t be16toh(uint16_t be) {
  return !arch_little_endian() ? be : __builtin_bswap16(be);
}
static inline uint16_t le16toh(uint16_t le) {
  return arch_little_endian() ? le : __builtin_bswap16(le);
}

static inline uint32_t htobe32(uint32_t host) {
  return !arch_little_endian() ? host : __builtin_bswap32(host);
}
static inline uint32_t htole32(uint32_t host) {
  return arch_little_endian() ? host : __builtin_bswap32(host);
}
static inline uint32_t be32toh(uint32_t be) {
  return !arch_little_endian() ? be : __builtin_bswap32(be);
}
static inline uint32_t le32toh(uint32_t le) {
  return arch_little_endian() ? le : __builtin_bswap32(le);
}

static inline uint64_t htobe64(uint64_t host) {
  return !arch_little_endian() ? host : __builtin_bswap64(host);
}
static inline uint64_t htole64(uint64_t host) {
  return arch_little_endian() ? host : __builtin_bswap64(host);
}
static inline uint64_t be64toh(uint64_t be) {
  return !arch_little_endian() ? be : __builtin_bswap64(be);
}
static inline uint64_t le64toh(uint64_t le) {
  return arch_little_endian() ? le : __builtin_bswap64(le);
}

#endif
