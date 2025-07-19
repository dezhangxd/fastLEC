#pragma once

#include <chrono>
#include <iostream>
#include <vector>
#include <functional>
#include <random>

// ----------------------------------------------------------------------------
// basic.hpp
// This file contains some basic definitions and functions
// ----------------------------------------------------------------------------

namespace fastLEC
{

    // SAT solver return values
    enum ret_vals
    {
        ret_UNK = 0,
        ret_SAT = 10,
        ret_UNS = 20
    };

    // Runtime Manager - Singleton Pattern
    // ----------------------------------------------------------------------------
    class ResMgr
    {
    private:
        std::chrono::high_resolution_clock::time_point _start_time;
        bool _initialized = false;
        std::mt19937 _rng;

        // Private constructor for singleton
        ResMgr() : _rng(std::random_device{}()) {}

        // Delete copy constructor and assignment operator
        ResMgr(const ResMgr &) = delete;
        ResMgr &operator=(const ResMgr &) = delete;

    public:
        // Get singleton instance
        static ResMgr &get();

        // Initialize the runtime manager
        void init();

        // Get runtime in seconds
        double get_runtime() const;

        // Check if initialized
        bool is_initialized() const { return _initialized; }

        // Reset the timer
        void reset_timer();

        // Random number generation
        template <typename T>
        T random(T min, T max);

        uint64_t random_uint64();

        // Set random seed
        void set_seed(uint32_t seed);

        // Get random number generator reference
        std::mt19937 &get_rng() { return _rng; }

        // Destructor
        ~ResMgr() = default;
    };

    // ----------------------------------------------------------------------------

    // Bit Vector for Simulation
    // ----------------------------------------------------------------------------
    typedef uint64_t bv_unit_t;
    class BitVector
    {
        bv_unit_t _width = 0; // should not be too long
        std::vector<bv_unit_t> _array;

        size_t _hashval = 0;

    public:
        static const bv_unit_t size_correcter = 1ull;
        static const bv_unit_t bit1 = 1ull;
        static const bv_unit_t bit0 = 0ull;
        static const int unit_width = 8 * sizeof(bv_unit_t); // 64 bit for an uint64_t element

        BitVector() = default;
        BitVector(int n_bits);
        BitVector(const BitVector &rhs);
        ~BitVector() = default;

        int size() const { return _width; }
        void resize(int _width);
        size_t hash();
        size_t _std_hash_bit_vector() const;

        // operators
        // ---------------------------------
        void set();
        void reset();
        void random();
        void u64_pi(bv_unit_t pi_id);
        void cycle_festival(bv_unit_t cf);
        bv_unit_t operator[](int i) const;

        void set(bv_unit_t i);
        void reset(bv_unit_t i);

        bool operator==(const BitVector &rhs) const;
        bool operator!=(const BitVector &rhs) const;
        BitVector operator=(const BitVector &rhs);
        BitVector operator&(const BitVector &rhs) const;
        BitVector operator|(const BitVector &rhs) const;
        BitVector operator^(const BitVector &rhs) const;
        BitVector operator~() const;
        BitVector operator|=(const BitVector &rhs);
        BitVector operator&=(const BitVector &rhs);
        BitVector operator^=(const BitVector &rhs);

        BitVector operator++();
        BitVector operator--();

        bool has_one() const;

        friend std::ostream &operator<<(std::ostream &os, const BitVector &bv);
    };

} // namespace fastLEC

namespace std
{
    template <>
    struct hash<fastLEC::BitVector>
    {
        size_t operator()(const fastLEC::BitVector &bv) const
        {
            return bv._std_hash_bit_vector();
        };
    };

    template <>
    struct equal_to<fastLEC::BitVector>
    {
        bool operator()(const fastLEC::BitVector &lhs, const fastLEC::BitVector &rhs) const
        {
            return lhs == rhs;
        }
    };

    template <>
    struct hash<std::vector<int>>
    {
        size_t operator()(const std::vector<int> &vec) const
        {
            size_t seed = vec.size();
            for (const auto &i : vec)
            {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}
