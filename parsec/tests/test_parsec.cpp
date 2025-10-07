#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <ps/parsec.hpp>

TEST_CASE("add adds two integers", "add") {
    REQUIRE(ps::add(2, 3) == 5);
    REQUIRE(ps::add(-1, 1) == 0);
}
