#include "basic.hpp"

#include <cstring>
#include <cinttypes>

using namespace fastLEC;

namespace fastLEC
{
std::atomic<bool> global_solved_for_PPE(false);
}

ResMgr &ResMgr::get()
{
    static ResMgr instance;
    return instance;
}

void ResMgr::init()
{
    _start_time = std::chrono::high_resolution_clock::now();
    _initialized = true;
}

double ResMgr::get_runtime() const
{
    if (!_initialized)
    {
        return 0.0;
    }
    auto clk_now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double>>(
               clk_now - _start_time)
        .count();
}

void ResMgr::reset_timer()
{
    _start_time = std::chrono::high_resolution_clock::now();
}

// Random number generation implementations
template <typename T> T ResMgr::random(T min, T max)
{
    if constexpr (std::is_integral_v<T>)
    {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(_rng);
    }
    else
    {
        std::uniform_real_distribution<T> dist(min, max);
        return dist(_rng);
    }
}

double ResMgr::random_double(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(_rng);
}

bv_unit_t ResMgr::random_uint64()
{
    std::uniform_int_distribution<bv_unit_t> dist(0, UINT64_MAX);
    return dist(_rng);
}

// Explicit template instantiations for common types
template int ResMgr::random<int>(int min, int max);
template double ResMgr::random<double>(double min, double max);
template bv_unit_t ResMgr::random<bv_unit_t>(bv_unit_t min, bv_unit_t max);

void ResMgr::set_seed(uint32_t seed) { _rng.seed(seed); }

// ----------------------------------------------------------------------------
// Bitset
// ----------------------------------------------------------------------------

BitVector::BitVector(int width) { this->resize(width); }

void BitVector::resize(int width)
{
    _width = width;
    if (_width % unit_width != 0)
    {
        printf("c [BitVector] error: nBits %" PRIu64
               " is not a multiple of bit_width "
               "%d\n",
               _width,
               unit_width);
        exit(0);
    }
    _array.resize(_width / unit_width);
}

BitVector::BitVector(const BitVector &rhs)
{
    _width = rhs._width;
    _array.resize(_width / unit_width);
    _array = rhs._array;
}

size_t BitVector::hash()
{
    _hashval = _std_hash_bit_vector();
    return _hashval;
}

size_t BitVector::_std_hash_bit_vector() const
{
    size_t hashval = 0;
    if (_array.empty())
        hashval = 0;
    else
        hashval = _array[0];
    for (unsigned i = 1; i < _array.size(); i++)
        hashval = hashval + i * _array[i];
    return hashval;
}

// -------------------------------------------------
void BitVector::set() { std::fill(_array.begin(), _array.end(), ~0ull); }

void BitVector::reset() { std::fill(_array.begin(), _array.end(), 0ull); }

void BitVector::random()
{
    for (unsigned i = 0; i < _array.size(); i++)
    {
        _array[i] = ResMgr::get().random_uint64();
    }
}

bv_unit_t BitVector::operator[](int i) const
{
    int id = i / unit_width;
    int pos = i % unit_width;

    return (_array[id] >> (unit_width - pos - 1)) & 1;
}

bool BitVector::operator==(const BitVector &rhs) const
{
    if (_width != rhs._width)
        return false;
    else
        return _array == rhs._array;
}

BitVector BitVector::operator=(const BitVector &rhs)
{
    _width = rhs._width;
    _array.resize(_width / unit_width);
    _array = rhs._array;
    return *this;
}

BitVector BitVector::operator&(const BitVector &rhs) const
{
    BitVector res(_width);
    for (unsigned i = 0; i < _array.size(); i++)
        res._array[i] = _array[i] & rhs._array[i];
    return res;
}

BitVector BitVector::operator|(const BitVector &rhs) const
{
    BitVector res(_width);
    for (unsigned i = 0; i < _array.size(); i++)
        res._array[i] = _array[i] | rhs._array[i];
    return res;
}

BitVector BitVector::operator^(const BitVector &rhs) const
{
    BitVector res(_width);
    for (unsigned i = 0; i < _array.size(); i++)
        res._array[i] = _array[i] ^ rhs._array[i];
    return res;
}

BitVector BitVector::operator~() const
{
    BitVector res(_width);
    for (unsigned i = 0; i < _array.size(); i++)
        res._array[i] = ~_array[i];
    return res;
}

BitVector BitVector::operator|=(const BitVector &rhs)
{
    for (unsigned i = 0; i < _array.size(); i++)
        _array[i] |= rhs._array[i];
    return *this;
}

BitVector BitVector::operator&=(const BitVector &rhs)
{
    for (unsigned i = 0; i < _array.size(); i++)
        _array[i] &= rhs._array[i];
    return *this;
}

bool BitVector::operator!=(const BitVector &rhs) const
{
    if (_width != rhs._width)
        return true;
    else
        return _array != rhs._array;
}

void BitVector::set(bv_unit_t i)
{
    int id = i / unit_width;
    int pos = i % unit_width;
    _array[id] = _array[id] | (1ull << (unit_width - pos - 1));
}

void BitVector::reset(bv_unit_t i)
{
    int id = i / unit_width;
    int pos = i % unit_width;
    _array[id] = _array[id] & ~(1ull << (unit_width - pos - 1));
}

const uint64_t festivals[6] = {0xAAAAAAAAAAAAAAAA,
                               0xCCCCCCCCCCCCCCCC,
                               0xF0F0F0F0F0F0F0F0,
                               0xFF00FF00FF00FF00,
                               0xFFFF0000FFFF0000,
                               0xFFFFFFFF00000000};

void BitVector::u64_pi(bv_unit_t pi_id)
{
    if (pi_id < 6)
    {
        for (unsigned i = 0; i < _array.size(); i++)
            _array[i] = festivals[pi_id];
    }
    else
    {
        unsigned cf = 1 << (pi_id - 6);
        uint64_t val = ~0ull;
        for (unsigned i = 0; i < _array.size(); i += cf)
        {
            memset(_array.data() + i, val, cf * sizeof(uint64_t));
            val = ~val;
        }
    }
}

void BitVector::cycle_festival(bv_unit_t cf)
{
    if (cf > _width)
        set();
    else if (cf == _width || cf == 0)
        reset();
    else
    {
        bool val = 0;
        bv_unit_t i = 0;
        // reset();

        while (i < _width)
        {
            if (val)
            {
                for (bv_unit_t j = 0; j < cf; j++)
                {
                    set(i);
                    i++;
                }
                val = 0;
            }
            else
            {
                for (bv_unit_t j = 0; j < cf; j++)
                {
                    reset(i);
                    i++;
                }
                val = 1;
            }
        }
    }
}

bool BitVector::has_one() const
{
    for (unsigned i = 0; i < _array.size(); i++)
        if (_array[i] != 0)
            return true;
    return false;
}

uint64_t BitVector::num_ones()
{
    return std::accumulate(_array.begin(),
                           _array.end(),
                           0ull,
                           [](uint64_t sum, uint64_t val)
                           {
                               return sum + __builtin_popcountll(val);
                           });
}

uint64_t BitVector::num_zeros()
{
    return std::accumulate(_array.begin(),
                           _array.end(),
                           0ull,
                           [](uint64_t sum, uint64_t val)
                           {
                               return sum + unit_width -
                                   __builtin_popcountll(val);
                           });
}

BitVector BitVector::operator^=(const BitVector &rhs)
{
    for (unsigned i = 0; i < _array.size(); i++)
        _array[i] ^= rhs._array[i];
    return *this;
}

BitVector BitVector::operator++()
{
    for (int i = _array.size() - 1; i >= 0; i--)
    {
        if (_array[i] < UINT64_MAX)
        {
            _array[i]++;
            return *this;
        }
        _array[i] = 0;
    }
    return *this;
}

BitVector BitVector::operator--()
{
    for (int i = _array.size() - 1; i >= 0; i--)
    {
        if (_array[i] > 0)
        {
            _array[i]--;
            return *this;
        }
        _array[i] = UINT64_MAX;
    }
    return *this;
}

namespace fastLEC
{
std::ostream &operator<<(std::ostream &os, const BitVector &bv)
{
    int ct = 0;
    for (unsigned i = 0; i < bv._array.size(); i++)
    {
        for (unsigned j = 0; j < bv.unit_width; j++)
        {
            os << ((bv._array[i] >> (bv.unit_width - j - 1)) & 1);
            if (ct % 8 == 7)
                os << " ";
            ct++;
        }
    }
    return os;
}
} // namespace fastLEC