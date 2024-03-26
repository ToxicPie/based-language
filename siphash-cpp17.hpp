#include <cstdint>
#include <cstring>
#include <string>

#include "codeforces-bit.hpp"


namespace hash_lib {


#if defined(_WIN32)
#define CODEFORCES 1
#endif

// utility function that fills a buffer with random bytes from more reliable
// sources (usually provided by the os).
// supports: linux, windows
// does not care about any errors that may happen. use at your own risk

#if defined(__linux__)

#include <sys/random.h>
void fill_random_bytes(void *buffer, size_t len) {
    char *byte_buf = (char *)buffer;
    while (len != 0) {
        ssize_t count = getrandom(byte_buf, len, 0);
        if (count > 0) {
            len -= count;
            byte_buf += count;
        }
    }
}

#elif defined(_WIN32) // defined(__linux__)

#if defined(CODEFORCES)

// clang-format off
#include <windows.h>
#include <wincrypt.h>
// clang-format on
void fill_random_bytes(void *buffer, size_t len) {
    HCRYPTPROV prov;
    CryptAcquireContext(&prov, 0, 0, PROV_RSA_FULL,
                        CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    CryptGenRandom(prov, len, (BYTE *)buffer);
    CryptReleaseContext(prov, 0);
}

#else // defined(CODEFORCES)

// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
void fill_random_bytes(void *buffer, size_t len) {
    BCryptGenRandom(NULL, (PUCHAR)buffer, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

#endif // defined(CODEFORCES)

#endif // defined(_WIN32)


template <size_t C, size_t D> class SipHash {
  public:
    SipHash() = delete;
    SipHash(uint64_t k0, uint64_t k1)
        : v0(k0 ^ V0INIT), v1(k1 ^ V1INIT), v2(k0 ^ V2INIT), v3(k1 ^ V3INIT) {}

    void update(uint64_t block) {
        v3 ^= block;
        for (size_t i = 0; i < C; i++) {
            round();
        }
        v0 ^= block;
    }

    uint64_t finalize() {
        v2 ^= 0xff;
        for (size_t i = 0; i < D; i++) {
            round();
        }
        return v0 ^ v1 ^ v2 ^ v3;
    }

  private:
    void round() {
        v0 += v1;
        v2 += v3;
        v1 = std::rotl(v1, 13);
        v3 = std::rotl(v3, 16);
        v1 ^= v0;
        v3 ^= v2;
        v0 = std::rotl(v0, 32);
        v2 += v1;
        v0 += v3;
        v1 = std::rotl(v1, 17);
        v3 = std::rotl(v3, 21);
        v1 ^= v2;
        v3 ^= v0;
        v2 = std::rotl(v2, 32);
    }

    static constexpr uint64_t V0INIT = 0x736f6d6570736575ULL;
    static constexpr uint64_t V1INIT = 0x646f72616e646f6dULL;
    static constexpr uint64_t V2INIT = 0x6c7967656e657261ULL;
    static constexpr uint64_t V3INIT = 0x7465646279746573ULL;

    uint64_t v0, v1, v2, v3;
};


// wrapper for SipHash, intended as a replacement for std::hash
template <size_t C, size_t D> class SipHasher {
  public:
    SipHasher() {
        uint64_t k[2];
        fill_random_bytes(&k, sizeof(k));
        new (this) SipHasher(k[0], k[1]);
    }
    SipHasher(uint64_t k0_, uint64_t k1_) : k0(k0_), k1(k1_) {}

    size_t operator()(const std::string &str) const {
        SipHash<C, D> hasher(k0, k1);
        size_t blocks = str.size() / 8;
        for (size_t i = 0; i < blocks; i++) {
            uint64_t block = 0;
            std::memcpy(&block, str.data() + i * 8, 8);
            hasher.update(block);
        }
        uint64_t final_block = 0;
        std::memcpy(&final_block, str.data() + blocks * 8,
                    str.size() - blocks * 8);
        final_block |= uint64_t(str.size()) << 56;
        hasher.update(final_block);
        return hasher.finalize();
    }

  private:
    uint64_t k0, k1;
};


} // namespace hash_lib
