#define CATCH_CONFIG_MAIN
#include <utl/Catch2/single_include/catch2/catch.hpp>
#include <utl/introspection/StructFields.hpp>
#include <sstream>

enum E : uint8_t {
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
    YYYYY y;
    char c;
    double _d;
    uint16_t h;
    int i;
    const char * _c;
    E e_;
    void (*f)();
};

struct Visitor {
    std::stringstream str;
    void process(const char * v) {
        str << v << ' ';
    }
    void process(void * v) {
        str << v << ' ';
    }
    template<class T> void process(const T * v) {
        str << *v << ' ';
    }
    template<class T> typename std::enable_if<std::is_fundamental<T>::value>::type process(const T & v) {
        str << v << ' ';
    }
    template<class T> void process(const E & v) {
        str << (typename std::underlying_type<E>::type) v << ' ';
    }
    void process(const void(*v)()) {
        str << (void*) v << ' ';
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

    constexpr auto FM = _utl::PodIntrospection::StructFieldsMap<XXXXX>{};
    auto fmIter = FM.begin();
    XXXXX f;
    REQUIRE(  fmIter[0x0].offset == ((char*)&f.y._1 - (char*)&f)  );
    REQUIRE(  fmIter[0x1].offset == ((char*)&f.y._2 - (char*)&f)  );
    REQUIRE(  fmIter[0x2].offset == ((char*)&f.y._3 - (char*)&f)  );
    REQUIRE(  fmIter[0x3].offset == 8  );
    REQUIRE(  fmIter[0x3].offset == ((char*)&f.y.d  - (char*)&f)  );
    REQUIRE(  fmIter[0x4].offset == ((char*)&f.y._4 - (char*)&f)  );
    REQUIRE(  fmIter[0x5].offset == ((char*)&f.y._5 - (char*)&f)  );
    REQUIRE(  fmIter[0x6].offset == ((char*)&f.c    - (char*)&f)  );
    REQUIRE(  fmIter[0x7].offset == ((char*)&f._d   - (char*)&f)  );
    REQUIRE(  fmIter[0x8].offset == ((char*)&f.h    - (char*)&f)  );
    REQUIRE(  fmIter[0x9].offset == ((char*)&f.i    - (char*)&f)  );
    REQUIRE(  fmIter[0xA].offset == ((char*)&f._c   - (char*)&f)  );
    REQUIRE(  fmIter[0xB].offset == ((char*)&f.e_ - (char*)&f)  );
    REQUIRE(  fmIter[0xC].offset == ((char*)&f.f - (char*)&f)  );

    for (auto &it : FM) {
        std::cout << "[" << it.offset << "] " << (char) it.type << std::endl;
    }

    f = { '@', 'b', '$', 5.12, '!', '`', '?', 0.5, 1234, -1, "literal", E::A, nullptr };
    Visitor V;
    _utl::PodIntrospection::processStructFields(V, f);
    REQUIRE(V.str.str() == "@ b $ 5.12 ! ` ? 0.5 1234 -1 literal 37 nullptr ");
}