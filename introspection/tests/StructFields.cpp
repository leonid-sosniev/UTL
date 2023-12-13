#define CATCH_CONFIG_MAIN
//#include <utl/Catch2/single_include/catch2/catch.hpp>
#define TEST_CASE(...) int main()
#define REQUIRE(x) if (!(x)) { throw std::logic_error{std::to_string(__LINE__)}; }
#include <utl/introspection/StructFields.hpp>
#include <array>
#include <cassert>
#include <sstream>

enum class E : uint8_t {
    A=37, B=1, C=2
};

struct S {
    void * _v;
    const double _d;
    const char * _c;
};
struct M {
    S * s1;
    const char * _c;
    S s2;
};
struct D {
    int _i;
    S s2;
    M s1;
    S * sP;
};
struct YYYYY {
    char _1;
    char _2;
    char _3;
    double d;
    char _4;
    char _5;
};
struct XXXXX {
    YYYYY s;
    char c;
    double d;
    uint16_t h;
    YYYYY s2;
    int i;
    const char * cp;
    E e;
    void (*f)();
};

struct X1 {
    char c;
    uint32_t u;
};
struct X2 {
    uint32_t u;
    char c;
};
struct Y1 {
    X1 x;
    short s;
    E e;
};
struct Y2 {
    short s;
    X1 x;
    E e;
};
struct Y3 {
    X2 x;
    short s;
    union U {
        int k;
        X2 n;
    } u;
    E e;
};
struct Y4 {
    short s;
    X2 x;
    std::array<char,4> v;
    E e;
};

struct Visitor {
    std::stringstream str;
public:
    template<class T> void process(const T * v) {
        str << v << ' ';
    }
    template<class T> typename std::enable_if<std::is_fundamental<T>::value>::type process(const T & v) {
        str << v << ' ';
    }
    template<class T> typename std::enable_if<std::is_enum<T>::value>::type process(const T & v) {
        str << (int) v << ' ';
    }
    template<class T> typename std::enable_if<std::is_union<T>::value>::type process(const T & v) {
        str << "union(" << sizeof(T) << ") ";
    }
    template<class T> typename std::enable_if<std::is_class<T>::value && !std::is_enum<T>::value>::type process(const T & v) {
        _utl::PodIntrospection::processTopLevelFields(*this, v);
    }
    template<class T, class ...Args> void process(T(*v)(Args...)) {
        str << (void*) v << ' ';
    }
    template<class T, class C, class ...Args> void process(T(C::*v)(Args...)) {
        str << (void*) v << ' ';
    }
    void process(...) {
        str << "UNEXPECTED ";
    }
};

TEST_CASE("get number of fields in struct", "introspection")
{

    constexpr size_t numS = _utl::PodIntrospection::getFieldCount<S>();
    constexpr size_t numM = _utl::PodIntrospection::getFieldCount<M>();
    constexpr size_t numD = _utl::PodIntrospection::getFieldCount<D>();

    REQUIRE(numS == 3);
    REQUIRE(numM == 3);
    REQUIRE(numD == 4);

    constexpr size_t recS = _utl::PodIntrospection::getFieldCountRecursive<S>();
    constexpr size_t recM = _utl::PodIntrospection::getFieldCountRecursive<M>();
    constexpr size_t recD = _utl::PodIntrospection::getFieldCountRecursive<D>();

    REQUIRE(recS == 3);
    REQUIRE(recM == 5);
    REQUIRE(recD == 10);

    Visitor V;

    Y1 a{ {'?',1}, 2, E::A };
    Y2 b{ 3, {'!',4}, E::A };
    Y3 c{ {5,'?'}, 6, 1, E::A };
    Y4 d{ 7, {8,'!'}, { 'w', 'x', 'y', 'z' }, E::A };
    _utl::PodIntrospection::processTopLevelFields(V, a);
    _utl::PodIntrospection::processTopLevelFields(V, b);
    _utl::PodIntrospection::processTopLevelFields(V, c);
    _utl::PodIntrospection::processTopLevelFields(V, d);
    REQUIRE(V.str.str() == "? 1 2 37 3 ! 4 37 5 ? 6 union(8) 37 7 8 ! w x y z 37 ");

    V.str.str("");

    XXXXX pod = { { '@', 'b', '$', 5.12, '!', '`' }, '?', 0.5, 1234, { '`', '!', '$', 12.5, 'b', '@' }, -1, "literal", E::A, nullptr };
    _utl::PodIntrospection::processTopLevelFields(V, pod);
    REQUIRE(V.str.str() == "@ b $ 5.12 ! ` ? 0.5 1234 ` ! $ 12.5 b @ -1 literal 37 0 ");
}