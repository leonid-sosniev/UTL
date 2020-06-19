#include <utl/Catch2/single_include/catch2/catch.hpp>
#include "../StdStreamWriter.hpp"
#include <sstream>

//TODO: also run tests for abstract writer

TEST_CASE("StdStreamWriter", "[io]")
{
    SECTION("construct with null") {
        REQUIRE_THROWS_AS( _utl::StdStreamWriter{nullptr}, std::invalid_argument );
    }

    std::stringstream OUT;
    _utl::StdStreamWriter wtr{&OUT};

    SECTION(".write()") {
        REQUIRE( wtr.write("12\034", 5) == 5 );
        REQUIRE( wtr.write(nullptr, 0) == 0 );
        REQUIRE_THROWS_AS( wtr.write(nullptr, 01), std::invalid_argument );
        REQUIRE_THROWS_AS( wtr.write(nullptr, 10), std::invalid_argument );
    }
}