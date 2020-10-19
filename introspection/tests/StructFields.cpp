#define CATCH_CONFIG_MAIN
#include <utl/Catch2/single_include/catch2/catch.hpp>
#include <utl/introspection/StructFields.hpp>

TEST_CASE("get number of fields in struct", "introspection")
{
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
        char c;
        double _d;
        uint16_t h;
        int i;
        char * _c;
        YYYYY y;
    };

    constexpr size_t numS = _utl::getFieldCount<S>();
    constexpr size_t numM = _utl::getFieldCount<M>();
    constexpr size_t numD = _utl::getFieldCount<D>();

    REQUIRE(numS == 3);
    REQUIRE(numM == 3);
    REQUIRE(numD == 4);

    constexpr size_t recS = _utl::getFieldCountRecursive<S>();
    constexpr size_t recM = _utl::getFieldCountRecursive<M>();
    constexpr size_t recD = _utl::getFieldCountRecursive<D>();

    REQUIRE(recS == 3);
    REQUIRE(recM == 5);
    REQUIRE(recD == 10);

    constexpr auto FM = _utl::StructFieldsMap<XXXXX>{};
    auto fmIter = FM.begin();
    XXXXX f;
    REQUIRE(  (fmIter+0x0)->offset == ((char*)&f.c    - (char*)&f)  );
    REQUIRE(  (fmIter+0x1)->offset == ((char*)&f._d   - (char*)&f)  );
    REQUIRE(  (fmIter+0x2)->offset == ((char*)&f.h    - (char*)&f)  );
    REQUIRE(  (fmIter+0x3)->offset == ((char*)&f.i    - (char*)&f)  );
    REQUIRE(  (fmIter+0x4)->offset == ((char*)&f._c   - (char*)&f)  );
    REQUIRE(  (fmIter+0x5)->offset == ((char*)&f.y._1 - (char*)&f)  );
    REQUIRE(  (fmIter+0x6)->offset == ((char*)&f.y._2 - (char*)&f)  );
    REQUIRE(  (fmIter+0x7)->offset == ((char*)&f.y._3 - (char*)&f)  );
    REQUIRE(  (fmIter+0x8)->offset == ((char*)&f.y.d  - (char*)&f)  );
    REQUIRE(  (fmIter+0x9)->offset == ((char*)&f.y._4 - (char*)&f)  );
    REQUIRE(  (fmIter+0xA)->offset == ((char*)&f.y._5 - (char*)&f)  );
}