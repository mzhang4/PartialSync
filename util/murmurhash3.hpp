#ifndef MURMURHASH3_HPP
#define MURMURHASH3_HPP

#include <inttypes.h>
#include <vector>

extern uint32_t MurmurHash3(uint32_t nHashSeed, const std::vector<unsigned char>& vDataToHash);

#endif
