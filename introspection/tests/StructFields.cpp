#define CATCH_CONFIG_MAIN
//#include <utl/Catch2/single_include/catch2/catch.hpp>
#define TEST_CASE(...) int main()
#define REQUIRE(x) if (!(x)) { throw std::logic_error{std::to_string(__LINE__)}; }
#include <utl/introspection/StructFields.hpp>
#include <array>
#include <cassert>
#include <sstream>
#include <vector>

enum class E : uint8_t {
    A=37, B=1, C=2
};
std::string to_string(const E& e) {
    switch (e) {
        case E::A: return "E::A";
        case E::B: return "E::B";
        case E::C: return "E::C";
        default: std::abort();
    };
}

struct Visitor {
    std::stringstream str;
    bool commaIsNeeded = false;
public:
    template<class T> typename std::enable_if<std::is_fundamental<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << v;
        commaIsNeeded = true;
    }
    template<class T> typename std::enable_if<std::is_enum<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << "\"" << to_string(v) << "\"";
        commaIsNeeded = true;
    }
    template<class T> typename std::enable_if<std::is_union<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << "union(sizeof=" << sizeof(T) << ")";
        commaIsNeeded = true;
    }
    template<class T> typename std::enable_if<std::is_class<T>::value && !std::is_enum<T>::value>::type process(const T & v) {
        if (commaIsNeeded) str << ", ";
        str << "{";
        commaIsNeeded = false;
        _utl::PodIntrospection::processTopLevelFields(*this, v);
        commaIsNeeded = true;
        str << "}";
    }
    template<
        class T,
        typename = typename std::enable_if<!std::is_void<T>::value>::type
    >
    void process(const T * v) {
        process<T>(*v);
        str << "*";
    }
    void process(const void * v) {
        if (commaIsNeeded) str << ", ";
        if (v) {
            str << std::hex << v;
        } else {
            str << "nullptr";
        }
        commaIsNeeded = true;
    }
    template<class T, class...Args> void process(T(*const v)(Args...)) {
        if (commaIsNeeded) str << ", ";
        str << "(func)";
        commaIsNeeded = true;
    }
    void process(const char * v) {
        if (commaIsNeeded) str << ", ";
        str << '"' << v << '"';
        commaIsNeeded = true;
    }
    void process(const bool v) {
        if (commaIsNeeded) str << ", ";
        str << (v ? "true" : "false");
        commaIsNeeded = true;
    }
    void process(...) {
        if (commaIsNeeded) str << ", ";
        str << "(UNEXPECTED)";
        commaIsNeeded = true;
    }
};

struct S0 {};
struct S1 {
    float d;
};
struct S2 {
    int i;
    double d;
};
struct S3 {
    void * vp;
    bool b;
    bool b2;
    const char * cp;
    const float f;
    E e;
    union { int l; double p; } u;
    S1 *sp;
    S2 pp;
    void (*fp)();
    std::array<char*,4> a
    ;
};

void func() {}
Visitor V;
char emptyStr[1] = "";

#include <iostream>
#include <cstring>
#include <utl/introspection/bitfields.hpp>

bool compare(const void * mem, std::initializer_list<uint8_t> bytes)
{
    auto a = static_cast<const uint8_t*>(mem);
    for (const uint8_t b : bytes) {
        if (b != *a++) return false;
    }
    return true;
}
uintmax_t revertBytes(uintmax_t x)
{
    uint8_t *rt = reinterpret_cast<uint8_t*>(&x) + sizeof(uintmax_t) - 1;
    uint8_t *lt = reinterpret_cast<uint8_t*>(&x);
    while (lt < rt) std::swap(*lt++, *rt--);
    return x;
}

template<size_t...Ws> using LEFields = _utl::PortableBitFieldsContainer<_utl::Endianness::Little, Ws...>;
template<size_t...Ws> using BEFields = _utl::PortableBitFieldsContainer<_utl::Endianness::Big, Ws...>;

TEST_CASE("get number of fields in struct", "introspection")
{
    {
    _utl::Bitfields<uint64_t, 1, 8, 32, 23> bits;
    bits.put<3>(1);
    bits.put<2>(1);
    bits.put<1>(0b10000001);
    bits.put<0>(1);
    REQUIRE(bits.get<0>() == 1);
    REQUIRE(bits.get<1>() == 0b10000001);
    REQUIRE(bits.get<2>() == 1);
    REQUIRE(bits.get<3>() == 1);
    REQUIRE(bits.raw() == 0b00000000'00000001'00000000'00000000'00000000'00000001'10000001'1ull);
    }

    {
    // one byte
    LEFields<1,7> s;
    s.put<0>(1);
    s.put<1>(0b1100011);
    s.put<0>(0);
    REQUIRE(compare(s.data(), {0b11000110}));

    // multibyte
    uint8_t raw[9] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 };
    const uint8_t raw2[9] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 }; // same array to compare with later
    auto *m = reinterpret_cast<LEFields<4,64,4>*>(raw);

    REQUIRE(m->get<0>() == 0x1);
    REQUIRE(m->get<2>() == 0x9);
    REQUIRE(m->get<1>() == 0x9887766554433221ull);

    m->put<0>(1);
    REQUIRE(std::memcmp(raw, raw2, 9) == 0);
    m->put<1>(0x9887766554433221ull);
    REQUIRE(std::memcmp(raw, raw2, 9) == 0);
    m->put<2>(9);
    REQUIRE(std::memcmp(raw, raw2, 9) == 0);
    }

    {
    // one byte
    BEFields<1,7> s;
    s.put<0>(1);
    s.put<1>(0b1100011);
    s.put<0>(0);
    REQUIRE(compare(s.data(), {0b11000110}));

    // multibyte
    uint8_t raw[9] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 };
    const uint8_t raw2[9] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 }; // same array to compare with later
    auto *m = reinterpret_cast<BEFields<4,64,4>*>(raw);

    REQUIRE(m->get<0>() == 0x1);
    auto n = m->get<1>();
    REQUIRE(m->get<1>() == 0x1223344556677889ull);
    REQUIRE(m->get<2>() == 0x9);

    m->put<0>(1);
    REQUIRE(std::memcmp(raw, raw2, 9) == 0);
    m->put<1>(0x1223344556677889ull);
    REQUIRE(std::memcmp(raw, raw2, 9) == 0);
    m->put<2>(9);
    REQUIRE(std::memcmp(raw, raw2, 9) == 0);
    }

    REQUIRE(0  == _utl::PodIntrospection::getFieldCount<S0>());
    REQUIRE(0  == _utl::PodIntrospection::getFieldCountRecursive<S0>());
    REQUIRE(1  == _utl::PodIntrospection::getFieldCount<S1>());
    REQUIRE(1  == _utl::PodIntrospection::getFieldCountRecursive<S1>());
    REQUIRE(2  == _utl::PodIntrospection::getFieldCount<S2>());
    REQUIRE(2  == _utl::PodIntrospection::getFieldCountRecursive<S2>());
    REQUIRE(11 == _utl::PodIntrospection::getFieldCount<S3>());
    REQUIRE(15 == _utl::PodIntrospection::getFieldCountRecursive<S3>());

    S1 s1{ -4.f };
    _utl::PodIntrospection::processTopLevelFields(V, s1);
    V.commaIsNeeded = false; V.str << "\n";

    S2 s2{ 1, 0.5 };
    _utl::PodIntrospection::processTopLevelFields(V, s2);
    V.commaIsNeeded = false; V.str << "\n";

    S3 s3{ nullptr, true, false, "hello", 0.1f, E::A, {-1}, &s1, s2, &func, {emptyStr,emptyStr,emptyStr,emptyStr} };
    _utl::PodIntrospection::processTopLevelFields(V, s3);
    V.commaIsNeeded = false; V.str << "\n";

    std::cout << V.str.str() << std::endl;
}