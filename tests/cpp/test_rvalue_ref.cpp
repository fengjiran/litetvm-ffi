//
// Created by 赵丹 on 25-6-6.
//
#include "ffi/any.h"
#include "ffi/container/array.h"
#include "ffi/function.h"
#include "ffi/rvalue_ref.h"
#include "testing_object.h"

#include <gtest/gtest.h>

namespace {
using namespace litetvm::ffi;
using namespace litetvm::ffi::testing;

TEST(RValueRef, use_count) {
    RValueRef<Array<int>> a(Array<int>({1, 2}));
    EXPECT_EQ(a.use_count(), 1);
    Array<int> b = *std::move(a);
    EXPECT_EQ(b.use_count(), 1);
}

TEST(RValueRef, Basic) {
    // GTEST_SKIP();
    auto append =
            Function::FromTyped([](RValueRef<Array<int>> ref, int val, bool is_unique) -> Array<int> {
                Array<int> arr = *std::move(ref);
                EXPECT_TRUE(arr.unique() == is_unique);
                arr.push_back(val);
                return arr;
            });
    auto a = append(RValueRef(Array<int>({1, 2})), 3, true).cast<Array<int>>();
    EXPECT_EQ(a.size(), 3);
    a = append(RValueRef(std::move(a)), 4, true).cast<Array<int>>();
    EXPECT_EQ(a.size(), 4);
    // pass in lvalue instead, the append still will succeed but array will not be unique
    a = append(a, 5, false).cast<Array<int>>();
    EXPECT_EQ(a.size(), 5);
}

TEST(RValueRef, ParamChecking) {
    // GTEST_SKIP();
    // try decution
    Function fadd1 = Function::FromTyped([](TInt a) -> int64_t { return a->value + 1; });

    // convert that triggers error
    EXPECT_THROW(
            {
                try {
                    fadd1(RValueRef(TInt(1)));
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "TypeError");
                    EXPECT_EQ(error.message(),
                              "Mismatched type on argument #0 when calling: `(0: test.Int) -> int`. "
                              "Expected `test.Int` but got `ObjectRValueRef`");
                    throw;
                }
            },
            ::litetvm::ffi::Error);

    Function fadd2 = Function::FromTyped([](RValueRef<Array<int>> a) -> int {
        Array<int> arr = *std::move(a);
        return arr[0] + 1;
    });

    // convert that triggers error
    EXPECT_THROW(
            {
                try {
                    fadd2(RValueRef(Array<Any>({1, 2.2})));
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "TypeError");
                    EXPECT_EQ(
                            error.message(),
                            "Mismatched type on argument #0 when calling: `(0: RValueRef<Array<int>>) -> int`. "
                            "Expected `RValueRef<Array<int>>` but got `RValueRef<Array[index 1: float]>`");
                    throw;
                }
            },
            ::litetvm::ffi::Error);
    // triggered a rvalue based conversion
    Function func3 = Function::FromTyped([](RValueRef<TPrimExpr> a) -> String {
        TPrimExpr expr = *std::move(a);
        return expr->dtype;
    });
    // EXPECT_EQ(func3(RValueRef(String("int32"))).cast<String>(), "int32");
    // triggered a lvalue based conversion
    // EXPECT_EQ(func3(String("int32")).cast<String>(), "int32");
}
}// namespace