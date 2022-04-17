//
// Created by taylor-santos on 4/16/2022 at 23:21.
//

#include "doctest/doctest.h"

#include "glpt.hpp"

TEST_SUITE_BEGIN("glpt");

TEST_CASE("glpt") {
    CHECK(glpt::foo() == 123);
}
