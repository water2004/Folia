// SPDX-License-Identifier: EPL-2.0
// java.util.Random as translated by the pinned ELK 0.9.3 GWT worker.
export module folia.mermaid.elk_random;
import std;

export namespace folia::mermaid {
// A graph owns one RNG across crossing minimization and orthogonal routing.
// LayerSweep resets this same object to its sampled long seed before sweeping.
struct ElkRandom {
    std::uint64_t state;
    explicit ElkRandom(std::uint64_t seed = 1) { set_seed(seed); }
    void set_seed(std::uint64_t seed) { state=(seed^0x5deece66dULL)&((1ULL<<48)-1); }
    std::uint32_t next(int bits) {
        state=(state*0x5deece66dULL+11)&((1ULL<<48)-1);
        return static_cast<std::uint32_t>(state>>(48-bits));
    }
    std::uint64_t next_long() {
        const auto high=static_cast<std::uint64_t>(next(32))<<32;
        const auto low=static_cast<std::int64_t>(std::bit_cast<std::int32_t>(next(32)));
        return high+static_cast<std::uint64_t>(low);
    }
    double next_double() {
        const auto high=next(26),low=next(27);
        return high*0x1p-26+low*0x1p-53;
    }
    double next_float() { return next(24)*0x1p-24; }
    std::size_t next_int(std::size_t bound) {
        const auto b=static_cast<std::uint32_t>(bound);
        if((b&(b-1))==0)return(static_cast<std::uint64_t>(b)*next(31))>>31;
        for(;;) {
            const auto bits=next(31),value=bits%b;
            if(bits-value+(b-1)<0x80000000U)return value;
        }
    }
};
}
