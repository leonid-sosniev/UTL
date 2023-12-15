#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <stdexcept>

namespace {

template<typename TItem, TItem ... tNumbers> struct TypedSum {
};
template<typename TItem, TItem tFirst, TItem ...tRest> struct TypedSum<TItem, tFirst, tRest...> {
    static constexpr TItem value = tFirst + TypedSum<TItem, tRest...>::value;
};
template<typename TItem> struct TypedSum<TItem> {
    static constexpr TItem value = TItem{};
};
template<size_t ... tNumbers> using Sum = TypedSum<size_t, tNumbers...>;


template<typename TItem, size_t tCount, TItem ...tNumbers> struct TypedSumOfFirst {
};
template<typename TItem, size_t tCount, TItem tFirst, TItem ...tRest> struct TypedSumOfFirst<TItem, tCount, tFirst, tRest...> {
    static constexpr TItem value = tFirst + TypedSumOfFirst<TItem, tCount-1, tRest...>::value;
};
template<typename TItem, TItem tFirst, TItem ...tRest> struct TypedSumOfFirst<TItem, 0, tFirst, tRest...> {
    static constexpr TItem value = TItem{};
};
template<size_t tCount, size_t ...tNumbers> using SumOfFirst = TypedSumOfFirst<size_t, tCount, tNumbers...>;


template<typename TItem, size_t tIndex, TItem ... tNumbers> struct TypedGetByIndex {
};
template<typename TItem, size_t tIndex, TItem tFirst, TItem ...tRest> struct TypedGetByIndex<TItem, tIndex, tFirst, tRest...> {
    static constexpr TItem value = TypedGetByIndex<TItem, tIndex-1, tRest...>::value;
};
template<typename TItem, TItem tFirst, TItem ...tRest> struct TypedGetByIndex<TItem, 0, tFirst, tRest...> {
    static constexpr TItem value = tFirst;
};
template<size_t tIndex, size_t ... tNumbers> using GetByIndex = TypedGetByIndex<size_t, tIndex, tNumbers...>;

template<size_t tBits> constexpr uintmax_t lsh(uintmax_t value) { return value << tBits; }
template<> constexpr uintmax_t lsh<sizeof(uintmax_t)*8>(uintmax_t value) { return 0; }

template<size_t tWholeBytesNumber> struct LittleEndianValueAccessor {
    static constexpr uintmax_t read(const uint8_t * bytes) {
#if defined(LITTLE_ENDIAN)
        constexpr uintmax_t mask = ~lsh<tWholeBytesNumber*8>(uintmax_t(-1));
        return *reinterpret_cast<const uintmax_t*>(bytes) & mask;
#else
        return *bytes | (LittleEndianValueAccessor<tWholeBytesNumber-1>::read(bytes+1) << 8);
#endif
    }
    static void write(uintmax_t value, uint8_t * bytes) {
#if defined(LITTLE_ENDIAN)
        constexpr uintmax_t mask = lsh<tWholeBytesNumber*8>(uintmax_t(-1));
        auto dst = reinterpret_cast<uintmax_t*>(bytes);
        *dst = (*dst & ~mask) | (value & mask);
#else
        *bytes = value;
        LittleEndianValueAccessor<tWholeBytesNumber-1>::write(value >> 8, bytes + 1);
#endif
    }
};
template<> struct LittleEndianValueAccessor<0> {
    static constexpr uintmax_t read(const uint8_t * bytes) { return 0; }
    static void write(uintmax_t value, uint8_t * bytes) { }
};

template<
    size_t tOffset, size_t tWidth, bool tWithinSingleByte = tOffset + tWidth <= 8
>
class BitFieldAccessor {
public:
    static constexpr uintmax_t read(const uint8_t * bytes)
    {
        constexpr uint8_t mask = (1 << tWidth) - 1;
        return (*bytes >> tOffset) & mask;
    }
    static void write(uintmax_t value, uint8_t * bytes)
    {
        constexpr uint8_t mask = (1 << tWidth) - 1;
        *bytes &= ~(mask << tOffset);
        *bytes |= (value & mask) << tOffset;
    }
};
template<size_t tWidth> class BitFieldAccessor<0, tWidth, false> {
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        return LittleEndianValueAccessor<(tWidth + 7) / 8>::read(bytes);
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        LittleEndianValueAccessor<(tWidth + 7) / 8>::write(value, bytes);
    }
};
template<size_t tOffset, size_t tWidth> class BitFieldAccessor<tOffset, tWidth, false> {
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        auto head = BitFieldAccessor<tOffset,8-tOffset>::read(bytes);
        auto rest = LittleEndianValueAccessor<tWidth/8>::read(bytes+1);
        return head | (rest << (8-tOffset));
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        BitFieldAccessor<tOffset,8-tOffset>::write(value, bytes);
        LittleEndianValueAccessor<tWidth/8>::write(value >> (8-tOffset), bytes+1);
    }
};

template<uintmax_t tValue, uintmax_t tMin, uintmax_t tMax> struct InRange {
    static constexpr bool value = (tMin <= tValue) && (tValue <= tMax);
};
template<bool tFirst, bool ...tRest> struct All { static constexpr bool value = tFirst && All<tRest...>::value; };
template<> struct All<true> { static constexpr bool value = true; };
template<> struct All<false> { static constexpr bool value = false; };

} // anonymous namespace


namespace _utl {

// Packet of little-endian LSB bit fields
template<size_t ...tWidths> class PortableBitFieldsContainer {
private:
    static const size_t DATA_BYTES_LENGTH = (Sum<tWidths...>::value + 7) / 8;
    uint8_t bytes_[DATA_BYTES_LENGTH];
private:
    static_assert(sizeof...(tWidths) > 0, "There must be at least one bit field width");
    static_assert(Sum<tWidths...>::value % 8 == 0, "Total width of bit fields must be a multiple of 8");
    static_assert(All<InRange<tWidths,1,sizeof(uintmax_t)*8>::value...>::value, "All bit field values must be non-zero and fit into uintmax_t");
public:
    template<size_t tFieldIndex> constexpr uintmax_t get() const
    {
        constexpr auto width = GetByIndex<tFieldIndex, tWidths...>::value;
        constexpr auto offset = SumOfFirst<tFieldIndex, tWidths...>::value;
        constexpr auto byteIndex = offset / 8;
        constexpr auto inbyteOffset = offset % 8;
        return BitFieldAccessor<inbyteOffset, width>::read(bytes_ + byteIndex);
    }
    template<size_t tFieldIndex> void put(uintmax_t value)
    {
        constexpr auto width = GetByIndex<tFieldIndex, tWidths...>::value;
        constexpr auto offset = SumOfFirst<tFieldIndex, tWidths...>::value;
        constexpr auto byteIndex = offset / 8;
        constexpr auto inbyteOffset = offset % 8;
        constexpr auto maxValidValue = uintmax_t(-1) >> (sizeof(uintmax_t)*8 - width);
        if (value > maxValidValue) {
            throw std::logic_error{ "The given value is too big for the field" };
        }
        BitFieldAccessor<inbyteOffset, width>::write(value, bytes_ + byteIndex);
    }
    const void * data() const { return &bytes_[0]; }
};

} // namespace _utl