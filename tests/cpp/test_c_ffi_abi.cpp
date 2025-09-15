//
// Created by 赵丹 on 2025/9/8.
//

#include "ffi/c_api.h"
#include <gtest/gtest.h>

namespace {

TEST(ABIHeaderAlignment, Default) {
    TVMFFIObject value;
    value.type_index = 10;
    EXPECT_EQ(reinterpret_cast<TVMFFIAny*>(&value)->type_index, 10);
    static_assert(sizeof(TVMFFIObject) == 24);
}

}// namespace