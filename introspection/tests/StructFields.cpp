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
}