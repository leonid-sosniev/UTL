#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <stdexcept>

namespace _utl {
    enum class Endianness { Big, Little };
}

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

template<uintmax_t tValue, uintmax_t tMin, uintmax_t tMax> struct InRange {
    static constexpr bool value = (tMin <= tValue) && (tValue <= tMax);
};
template<bool tFirst, bool ...tRest> struct All { static constexpr bool value = tFirst && All<tRest...>::value; };
template<> struct All<true> { static constexpr bool value = true; };
template<> struct All<false> { static constexpr bool value = false; };

template<size_t tBits> constexpr uintmax_t left_shift                     (uintmax_t value) { return value << tBits; }
template<            > constexpr uintmax_t left_shift<sizeof(uintmax_t)*8>(uintmax_t value) { return 0; }
template<size_t tBits> constexpr uintmax_t right_shift                     (uintmax_t value) { return value >> tBits; }
template<            > constexpr uintmax_t right_shift<sizeof(uintmax_t)*8>(uintmax_t value) { return 0; }

enum class FieldStructure { SingleByte, Multibyte };

template<
    _utl::Endianness tEndianness, size_t tOffsetInByte, size_t tWidth,
    FieldStructure tFieldStructure = (tOffsetInByte + tWidth <= 8) ? FieldStructure::SingleByte : FieldStructure::Multibyte
>
class BitFieldAccessor {
    static constexpr uintmax_t mask = left_shift<tWidth>(1) - 1;
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        return right_shift<tOffsetInByte>(*bytes) & mask;
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        *bytes &= ~left_shift<tOffsetInByte>(mask);
        *bytes |=  left_shift<tOffsetInByte>(value & mask);
    }
};
template<size_t tOffsetInByte, size_t tWidth> class BitFieldAccessor<_utl::Endianness::Little, tOffsetInByte, tWidth, FieldStructure::Multibyte> {
    static constexpr auto H = 8 - tOffsetInByte;
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        uintmax_t head = BitFieldAccessor<_utl::Endianness::Little, tOffsetInByte, H>::read(bytes);
        uintmax_t rest = BitFieldAccessor<_utl::Endianness::Little, 0,      tWidth-H>::read(bytes+1);
        return head | (rest << H);
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        BitFieldAccessor<_utl::Endianness::Little, tOffsetInByte,       H>::write(value, bytes);
        BitFieldAccessor<_utl::Endianness::Little, 0,            tWidth-H>::write(value >> H, bytes+1);
    }
};
template<size_t tOffsetInByte, size_t tWidth> class BitFieldAccessor<_utl::Endianness::Big, tOffsetInByte, tWidth, FieldStructure::Multibyte> {
    static constexpr auto H = 8 - tOffsetInByte;
    static constexpr auto R = tWidth - H;
    static constexpr uintmax_t Rmask = left_shift<R>(1) - 1;
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        uintmax_t head = BitFieldAccessor<_utl::Endianness::Big, tOffsetInByte, H>::read(bytes);
        uintmax_t rest = BitFieldAccessor<_utl::Endianness::Big, 0,             R>::read(bytes+1);
        return left_shift<R>(head) | rest;
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        BitFieldAccessor<_utl::Endianness::Big, tOffsetInByte, H>::write(value >> R, bytes);
        BitFieldAccessor<_utl::Endianness::Big, 0,             R>::write(value & Rmask, bytes+1);
        // will reordering of the 2 above lines improve performance?
    }
};
template<size_t tWidth> class BitFieldAccessor<_utl::Endianness::Little, 0, tWidth, FieldStructure::Multibyte> {
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        uintmax_t head = *bytes;
        uintmax_t rest = BitFieldAccessor<_utl::Endianness::Little, 0, tWidth-8>::read(bytes+1);
        return head | (rest << 8);
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        *bytes = value;
        BitFieldAccessor<_utl::Endianness::Little, 0, tWidth-8>::write(value >> 8, bytes+1);
    }
};
template<size_t tWidth> class BitFieldAccessor<_utl::Endianness::Big, 0, tWidth, FieldStructure::Multibyte> {
    static constexpr auto R = tWidth - 8;
    static constexpr uintmax_t Rmask = left_shift<R>(1) - 1;
public:
    static constexpr uintmax_t read(const uint8_t * bytes) {
        uintmax_t head = *bytes;
        uintmax_t rest = BitFieldAccessor<_utl::Endianness::Big, 0, R>::read(bytes+1);
        return left_shift<R>(head) | rest;
    }
    static void write(uintmax_t value, uint8_t * bytes) {
        *bytes = value >> R;
        BitFieldAccessor<_utl::Endianness::Big, 0, R>::write(value & Rmask, bytes+1);
    }
};

template<size_t tFieldIndex, size_t ...tWidths> struct FieldPositioning {
    static constexpr auto width = GetByIndex<tFieldIndex, tWidths...>::value;
    static constexpr auto offset = SumOfFirst<tFieldIndex, tWidths...>::value;
    static constexpr auto mask = left_shift<width>(1u) - 1;
};

} // anonymous namespace


namespace _utl {

template<typename TBase, size_t ...tWidths>
class Bitfields {
    TBase raw_;
    static_assert(std::is_unsigned<TBase>::value,                           "The given base type must be unsigned!");
    static_assert(Sum<tWidths...>::value == sizeof(TBase)*8,                "Total width of the bitfields must be equal to the base type size!");
    static_assert(All<InRange<tWidths,1,sizeof(TBase)*8>::value...>::value, "All bit field values must be non-zero and fit into the base type");
public:
    template<size_t tFieldIndex> constexpr TBase get() const
    {
        constexpr FieldPositioning<tFieldIndex,tWidths...> fp;
        return (raw_ >> fp.offset) & fp.mask;
    }
    template<size_t tFieldIndex> void put(TBase value)
    {
        constexpr FieldPositioning<tFieldIndex,tWidths...> fp;
        if (value > fp.mask) {
            throw std::logic_error{ "The given value is too great!" };
        }
        raw_ = (raw_ & ~(fp.mask << fp.offset)) | (value << fp.offset);
    }
    const TBase &raw() const { return raw_; }
};

// Packet of LSB fields with the given endiannes
template<Endianness tEndianness, size_t ...tWidths> class PortableBitFieldsContainer {
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
        constexpr FieldPositioning<tFieldIndex, tWidths...> fp;
        constexpr auto byteIndex = fp.offset / 8;
        constexpr auto inbyteOffset = fp.offset % 8;
        return BitFieldAccessor<tEndianness, inbyteOffset, fp.width>::read(bytes_ + byteIndex) & fp.mask;
        return 0;
    }
    template<size_t tFieldIndex> void put(uintmax_t value)
    {
        constexpr FieldPositioning<tFieldIndex, tWidths...> fp;
        constexpr auto byteIndex = fp.offset / 8;
        constexpr auto inbyteOffset = fp.offset % 8;
        if (value > fp.mask) {
            throw std::logic_error{ "The given value is too big for the field" };
        }
        BitFieldAccessor<tEndianness, inbyteOffset, fp.width>::write(value, bytes_ + byteIndex);
    }
    const void * data() const { return &bytes_[0]; }
};

} // namespace _utl