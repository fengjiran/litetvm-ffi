//
// Created by 赵丹 on 25-6-3.
//

#include "ffi/container/shape.h"

#include <gtest/gtest.h>

namespace {
using namespace litetvm::ffi;

TEST(Shape, Basic) {
    Shape shape = Shape({1, 2, 3});
    EXPECT_EQ(shape.size(), 3);
    EXPECT_EQ(shape[0], 1);
    EXPECT_EQ(shape[1], 2);
    EXPECT_EQ(shape[2], 3);

    Shape shape2 = Shape(Array<int64_t>({4, 5, 6, 7}));
    EXPECT_EQ(shape2.size(), 4);
    EXPECT_EQ(shape2[0], 4);
    EXPECT_EQ(shape2[1], 5);
    EXPECT_EQ(shape2[2], 6);
    EXPECT_EQ(shape2[3], 7);

    std::vector<int64_t> vec = {8, 9, 10};
    Shape shape3 = Shape(std::move(vec));
    EXPECT_EQ(shape3.size(), 3);
    EXPECT_EQ(shape3[0], 8);
    EXPECT_EQ(shape3[1], 9);
    EXPECT_EQ(shape3[2], 10);
    EXPECT_EQ(shape3.Product(), 8 * 9 * 10);

    Shape shape4 = Shape();
    EXPECT_EQ(shape4.size(), 0);
    EXPECT_EQ(shape4.Product(), 1);
}

TEST(Shape, AnyConvert) {
    Shape shape0 = Shape({1, 2, 3});
    Any any0 = shape0;

    auto shape1 = any0.cast<Shape>();
    EXPECT_EQ(shape1.size(), 3);
    EXPECT_EQ(shape1[0], 1);
    EXPECT_EQ(shape1[1], 2);
    EXPECT_EQ(shape1[2], 3);

    Array<Any> arr({1, 2});
    AnyView any_view0 = arr;
    auto shape2 = any_view0.cast<Shape>();
    EXPECT_EQ(shape2.size(), 2);
    EXPECT_EQ(shape2[0], 1);
    EXPECT_EQ(shape2[1], 2);
}

}// namespace