//
// Created by taylor-santos on 4/16/2022 at 23:21.
//

#include "doctest/doctest.h"

#include "clpt.hpp"

TEST_SUITE_BEGIN("clpt");

TEST_CASE("clpt") {
    CHECK(clpt::foo() == 123);
}
