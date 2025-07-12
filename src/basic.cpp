#include "basic.hpp"

using namespace fastLEC;

ResMgr& ResMgr::get()
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
    if (!_initialized) {
        return 0.0;
    }
    auto clk_now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double>>(clk_now - _start_time).count();
}

void ResMgr::reset_timer()
{
    _start_time = std::chrono::high_resolution_clock::now();
}

// Random number generation implementations
template<typename T>
T ResMgr::random(T min, T max)
{
    if constexpr (std::is_integral_v<T>) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(_rng);
    } else {
        std::uniform_real_distribution<T> dist(min, max);
        return dist(_rng);
    }
}

uint64_t ResMgr::random_uint64()
{
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(_rng);
}

// Explicit template instantiations for common types
template int ResMgr::random<int>(int min, int max);
template double ResMgr::random<double>(double min, double max);
template uint64_t ResMgr::random<uint64_t>(uint64_t min, uint64_t max);

void ResMgr::set_seed(uint32_t seed)
{
    _rng.seed(seed);
}




// ----------------------------------------------------------------------------
// Bitset
// ----------------------------------------------------------------------------

void BitVector::resize(int num_bits)
{
    _nBits = num_bits;
    if (_nBits % bit_width != 0)
    {
        printf("c [BitVector] error: nBits %ld is not a multiple of bit_width %d\n", _nBits, bit_width);
        exit(0);
    }
    _nArray = _nBits / bit_width;
    if (_array != nullptr)
        delete[] _array;
    _array = new uint64_t[_nArray];
}

BitVector::BitVector(const BitVector &rhs)
{
    _nBits = rhs._nBits;
    _nArray = rhs._nArray;
    if (_array != nullptr)
        delete[] _array;
    _array = new uint64_t[_nArray];
    std::copy(rhs._array, rhs._array + _nArray, _array);
}

BitVector::~BitVector()
{
    if (_array != nullptr)
        delete[] _array;
}

size_t BitVector::hash()
{
    _hashval = _std_hash_bit_vector();
    return _hashval;
}

size_t BitVector::_std_hash_bit_vector() const
{
    size_t hashval = 0;
    if (_array == nullptr)
        hashval = 0;
    hashval = _array[0];
    for (unsigned i = 1; i < _nArray; i++)
        hashval = hashval + i * _array[i];
    return hashval;
}

// -------------------------------------------------
void BitVector::set()
{
    std::fill(_array, _array + _nArray, ~0ull);
}

void BitVector::reset()
{
    std::fill(_array, _array + _nArray, 0ull);
}

void BitVector::random()
{
    for (unsigned i = 0; i < _nArray; i++)
    {
        _array[i] = ResMgr::get().random_uint64();
    }
}

uint64_t BitVector::operator[](int i) const
{
    int id = i / bit_width;
    int pos = i % bit_width;

    return (_array[id] >> (bit_width - pos - 1)) & 1;
}

bool BitVector::operator==(const BitVector &rhs) const
{
    if (_nBits != rhs._nBits)
        return false;
    else
        return std::equal(_array, _array + _nArray, rhs._array);
}

BitVector BitVector::operator=(const BitVector &rhs)
{
    if (_array != nullptr)
        delete[] _array;
    _nBits = rhs._nBits;
    _nArray = rhs._nArray;
    _array = new uint64_t[_nArray];
    for (unsigned i = 0; i < _nArray; i++)
        _array[i] = rhs._array[i];
    return *this;
}

BitVector BitVector::operator&(const BitVector &rhs) const
{
    BitVector res(_nBits);
    for (unsigned i = 0; i < _nArray; i++)
        res._array[i] = _array[i] & rhs._array[i];
    return res;
}

BitVector BitVector::operator|(const BitVector &rhs) const
{
    BitVector res(_nBits);
    for (unsigned i = 0; i < _nArray; i++)
        res._array[i] = _array[i] | rhs._array[i];
    return res;
}

BitVector BitVector::operator^(const BitVector &rhs) const
{
    BitVector res(_nBits);
    for (unsigned i = 0; i < _nArray; i++)
        res._array[i] = _array[i] ^ rhs._array[i];
    return res;
}

BitVector BitVector::operator~() const
{
    BitVector res(_nBits);
    for (unsigned i = 0; i < _nArray; i++)
        res._array[i] = ~_array[i];
    return res;
}

BitVector BitVector::operator|=(const BitVector &rhs)
{
    for (unsigned i = 0; i < _nArray; i++)
        _array[i] |= rhs._array[i];
    return *this;
}

BitVector BitVector::operator&=(const BitVector &rhs)
{
    for (unsigned i = 0; i < _nArray; i++)
        _array[i] &= rhs._array[i];
    return *this;
}

bool BitVector::operator!=(const BitVector &rhs) const
{
    if (_nBits != rhs._nBits)
        return true;
    else
        return !std::equal(_array, _array + _nArray, rhs._array);
}

void BitVector::set(uint64_t i)
{
    int id = i / bit_width;
    int pos = i % bit_width;
    _array[id] = _array[id] | (1ull << (bit_width - pos - 1));
}

void BitVector::reset(uint64_t i)
{
    int id = i / bit_width;
    int pos = i % bit_width;
    _array[id] = _array[id] & ~(1ull << (bit_width - pos - 1));
}

void BitVector::cycle_festival(uint64_t cf)
{
    if (cf > _nBits)
        set();
    else if (cf == _nBits || cf == 0)
        reset();
    else
    {
        bool val = 0;
        uint64_t i = 0;
        // reset();

        while (i < _nBits)
        {
            if (val)
            {
                for (uint64_t j = 0; j < cf; j++)
                {
                    set(i);
                    i++;
                }
                val = 0;
            }
            else
            {
                for (uint64_t j = 0; j < cf; j++)
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
    for (unsigned i = 0; i < _nArray; i++)
        if (_array[i] != 0)
            return true;
    return false;
}

BitVector BitVector::operator^=(const BitVector &rhs)
{
    for (unsigned i = 0; i < _nArray; i++)
        _array[i] ^= rhs._array[i];
    return *this;
}

BitVector::BitVector(int num_bits, uint64_t val)
{
    resize(num_bits);
    std::fill(_array, _array + _nArray, val);
}

BitVector BitVector::operator++()
{
    for (int i = _nArray - 1; i >= 0; i--)
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
    for (int i = _nArray - 1; i >= 0; i--)
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
