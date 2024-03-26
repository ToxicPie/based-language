#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

#include "codeforces-bit.hpp"


namespace rand_lib {

template <size_t ROUNDS> class ChaCha {
  public:
    ChaCha() = delete;
    ChaCha(const std::array<uint8_t, 32> &key, uint64_t nonce) {
        std::memcpy(&state[0], SIGMA.data(), SIGMA.size());
        std::memcpy(&state[4], key.data(), key.size());
        counter_mut() = 0;
        std::memcpy(&state[14], &nonce, sizeof(nonce));
    }

    std::array<uint32_t, 16> next_block() {
        std::array<uint32_t, 16> result = state;
        for (size_t i = 0; i < ROUNDS; i += 2) {
            quarter_round(result[0], result[4], result[8], result[12]);
            quarter_round(result[1], result[5], result[9], result[13]);
            quarter_round(result[2], result[6], result[10], result[14]);
            quarter_round(result[3], result[7], result[11], result[15]);
            quarter_round(result[0], result[5], result[10], result[15]);
            quarter_round(result[1], result[6], result[11], result[12]);
            quarter_round(result[2], result[7], result[8], result[13]);
            quarter_round(result[3], result[4], result[9], result[14]);
        }
        for (size_t i = 0; i < 16; i++) {
            result[i] += state[i];
        }
        counter_mut()++;
        return result;
    }

  private:
    uint64_t &counter_mut() {
        return *reinterpret_cast<uint64_t *>(&state[12]);
    }

    static void quarter_round(uint32_t &a, uint32_t &b, uint32_t &c,
                              uint32_t &d) {
        a += b, d ^= a, d = std::rotl(d, 16);
        c += d, b ^= c, b = std::rotl(b, 12);
        a += b, d ^= a, d = std::rotl(d, 8);
        c += d, b ^= c, b = std::rotl(b, 7);
    }

    static constexpr std::string_view SIGMA = "expand 32-byte k";

    std::array<uint32_t, 16> state{};
};

template <size_t ROUNDS> class ChaChaRng {
  public:
    ChaChaRng() = delete;
    explicit ChaChaRng(const std::array<uint8_t, 32> &seed)
        : core(seed, 0), buffer(), index(buffer.size()) {}

    uint32_t next_u32() {
        if (index >= buffer.size()) {
            index = 0;
            buffer = core.next_block();
        }
        return buffer[index++];
    }

    uint64_t next_u64() {
        return next_u32() | (uint64_t)next_u32() << 32;
    }

    using result_type = uint32_t;
    static constexpr result_type min() {
        return std::numeric_limits<result_type>::min();
    }
    static constexpr result_type max() {
        return std::numeric_limits<result_type>::max();
    }
    result_type operator()() {
        return next_u32();
    }

  private:
    ChaCha<ROUNDS> core;
    std::array<uint32_t, 16> buffer;
    size_t index;
};

std::array<uint8_t, 32> derive_seed(uint64_t x) {
    std::array<uint8_t, 32> result{};
    constexpr uint64_t MUL = 6364136223846793005ULL;
    constexpr uint64_t INC = 15726070495360670683ULL;
    for (size_t i = 0; i < 32; i += 4) {
        uint64_t old_x = x;
        x = old_x * MUL + (INC | 1);
        uint32_t xorshifted = ((old_x >> 18) ^ old_x) >> 27;
        uint32_t rot = old_x >> 59;
        uint32_t next = std::rotr(xorshifted, rot);
        std::memcpy(&result[i], &next, sizeof(next));
    }
    return result;
}

std::array<uint8_t, 32> derive_seed_from_argv(int argc, char **argv) {
    std::string args;
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            args += '\0';
        }
        args += argv[i];
    }
    std::array<uint8_t, 32> result{};
    for (size_t i = 0; i < args.size(); i += 8) {
        std::string substr = args.substr(i, 8);
        uint64_t hash = std::hash<std::string>{}(substr);
        std::array<uint8_t, 32> seed = derive_seed(hash + uint64_t(i));
        for (int i = 0; i < 32; i++) {
            result[i] ^= seed[i];
        }
    }
    return result;
}

} // namespace rand_lib
