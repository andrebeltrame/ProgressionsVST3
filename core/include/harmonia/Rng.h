#pragma once

#include <cstdint>
#include <vector>

namespace harmonia
{

/** Small deterministic PRNG - the same seed always produces the same idea. */
class Rng
{
public:
    explicit Rng(uint32_t seed = 1u) : state(seed == 0u ? 0x9e3779b9u : seed) {}

    uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /** Uniform in [0, 1). */
    float unit() { return static_cast<float>(next() & 0xffffff) / 16777216.0f; }

    /** Uniform integer in [lo, hi]. */
    int range(int lo, int hi)
    {
        if (hi <= lo)
            return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }

    bool chance(float probability) { return unit() < probability; }

    /** Picks an index from a list of non-negative weights. */
    int weighted(const std::vector<float>& weights)
    {
        float total = 0.0f;
        for (float w : weights)
            total += w > 0.0f ? w : 0.0f;
        if (total <= 0.0f)
            return weights.empty() ? -1 : static_cast<int>(next() % weights.size());

        float pick = unit() * total;
        for (size_t i = 0; i < weights.size(); ++i)
        {
            const float w = weights[i] > 0.0f ? weights[i] : 0.0f;
            if (pick < w)
                return static_cast<int>(i);
            pick -= w;
        }
        return static_cast<int>(weights.size()) - 1;
    }

    void reseed(uint32_t seed) { state = seed == 0u ? 0x9e3779b9u : seed; }

private:
    uint32_t state;
};

} // namespace harmonia
