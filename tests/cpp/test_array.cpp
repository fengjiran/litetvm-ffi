//
// Created by richard on 5/28/25.
//
#include "ffi/container/array.h"
#include "testing_object.h"

#include <gtest/gtest.h>

namespace {
using namespace litetvm::ffi;
using namespace litetvm::ffi::testing;

TEST(Array, basic) {
    Array<TInt> arr = {TInt(11), TInt(12)};
    EXPECT_EQ(arr.capacity(), 2);
    EXPECT_EQ(arr.size(), 2);
    TInt v1 = arr[0];
    EXPECT_EQ(v1->value, 11);
}

TEST(Array, COWSet) {
    Array<TInt> arr = {TInt(11), TInt(12)};
    Array<TInt> arr2 = arr;
    EXPECT_EQ(arr.use_count(), 2);
    arr.Set(1, TInt(13));
    EXPECT_EQ(arr.use_count(), 1);
    EXPECT_EQ(arr[1]->value, 13);
    EXPECT_EQ(arr2[1]->value, 12);
}

TEST(Array, MutateInPlaceForUniqueReference) {
    TInt x(1);
    Array<TInt> arr{x, x};
    EXPECT_TRUE(arr.unique());
    auto* before = arr.get();

    arr.MutateByApply([](const TInt&) { return TInt(2); });
    auto* after = arr.get();
    EXPECT_EQ(before, after);
}

TEST(Array, CopyWhenMutatingNonUniqueReference) {
    TInt x(1);
    Array<TInt> arr{x, x};
    Array<TInt> arr2 = arr;

    EXPECT_TRUE(!arr.unique());
    auto* before = arr.get();

    arr.MutateByApply([](const TInt&) { return TInt(2); });
    auto* after = arr.get();
    EXPECT_NE(before, after);
}

TEST(Array, Map) {
    // Basic functionality
    TInt x(1), y(1);
    Array<TInt> var_arr{x, y};
    auto f = [](const TInt& var) -> TNumber {
        return TFloat(static_cast<double>(var->value + 1));
    };
    Array<TNumber> expr_arr = var_arr.Map(f);

    EXPECT_NE(var_arr.get(), expr_arr.get());
    EXPECT_TRUE(expr_arr[0]->IsInstance<TFloatObj>());
    EXPECT_TRUE(expr_arr[1]->IsInstance<TFloatObj>());
}

TEST(Array, Iterator) {
    Array<int> array{1, 2, 3};
    std::vector<int> vector(array.begin(), array.end());
    EXPECT_EQ(vector[1], 2);
    EXPECT_EQ(*array.begin(), 1);
    EXPECT_EQ(array.front(), 1);
}

TEST(Array, PushPop) {
    Array<int> a;
    std::vector<int> b;
    for (int i = 0; i < 10; ++i) {
        a.push_back(i);
        b.push_back(i);
        ASSERT_EQ(a.front(), b.front());
        ASSERT_EQ(a.back(), b.back());
        ASSERT_EQ(a.size(), b.size());
        int n = static_cast<int>(a.size());
        for (int j = 0; j < n; ++j) {
            ASSERT_EQ(a[j], b[j]);
        }
    }

    for (int i = 9; i >= 0; --i) {
        ASSERT_EQ(a.front(), b.front());
        ASSERT_EQ(a.back(), b.back());
        ASSERT_EQ(a.size(), b.size());
        a.pop_back();
        b.pop_back();
        int n = static_cast<int>(a.size());
        for (int j = 0; j < n; ++j) {
            ASSERT_EQ(a[j], b[j]);
        }
    }
    ASSERT_EQ(a.empty(), true);
}

TEST(Array, ResizeReserveClear) {
    for (size_t n = 0; n < 10; ++n) {
        Array<int> a;
        Array<int> b;
        a.resize(n);
        b.reserve(n);
        ASSERT_EQ(a.size(), n);
        ASSERT_GE(a.capacity(), n);
        a.clear();
        b.clear();
        ASSERT_EQ(a.size(), 0);
        ASSERT_EQ(b.size(), 0);
    }
}

TEST(Array, InsertErase) {
    Array<int> a;
    std::vector<int> b;
    for (int n = 1; n <= 10; ++n) {
        a.insert(a.end(), n);
        b.insert(b.end(), n);
        for (int pos = 0; pos <= n; ++pos) {
            a.insert(a.begin() + pos, pos);
            b.insert(b.begin() + pos, pos);
            ASSERT_EQ(a.front(), b.front());
            ASSERT_EQ(a.back(), b.back());
            ASSERT_EQ(a.size(), n + 1);
            ASSERT_EQ(b.size(), n + 1);
            for (int k = 0; k <= n; ++k) {
                ASSERT_EQ(a[k], b[k]);
            }
            a.erase(a.begin() + pos);
            b.erase(b.begin() + pos);
        }
        ASSERT_EQ(a.front(), b.front());
        ASSERT_EQ(a.back(), b.back());
        ASSERT_EQ(a.size(), n);
    }
}

TEST(Array, InsertEraseRange) {
    Array<int> range_a{-1, -2, -3, -4};
    std::vector<int> range_b{-1, -2, -3, -4};
    Array<int> a;
    std::vector<int> b;

    static_assert(std::is_same_v<decltype(*range_a.begin()), int>);
    for (size_t n = 1; n <= 10; ++n) {
        a.insert(a.end(), static_cast<int>(n));
        b.insert(b.end(), static_cast<int>(n));
        for (size_t pos = 0; pos <= n; ++pos) {
            a.insert(a.begin() + pos, range_a.begin(), range_a.end());
            b.insert(b.begin() + pos, range_b.begin(), range_b.end());
            ASSERT_EQ(a.front(), b.front());
            ASSERT_EQ(a.back(), b.back());
            ASSERT_EQ(a.size(), n + range_a.size());
            ASSERT_EQ(b.size(), n + range_b.size());
            size_t m = n + range_a.size();
            for (size_t k = 0; k < m; ++k) {
                ASSERT_EQ(a[k], b[k]);
            }
            a.erase(a.begin() + pos, a.begin() + pos + range_a.size());
            b.erase(b.begin() + pos, b.begin() + pos + range_b.size());
        }
        ASSERT_EQ(a.front(), b.front());
        ASSERT_EQ(a.back(), b.back());
        ASSERT_EQ(a.size(), n);
    }
}

TEST(Array, FuncArrayAnyArg) {
    Function fadd_one = Function::FromTyped([](Array<Any> a) -> Any { return a[0].cast<int>() + 1; });
    EXPECT_EQ(fadd_one(Array<Any>{1}).cast<int>(), 2);
}

TEST(Array, MapUniquePropogation) {
    // Basic functionality
    Array<TInt> var_arr{TInt(1), TInt(2)};
    var_arr.MutateByApply([](TInt x) -> TInt {
        EXPECT_TRUE(x.unique());
        return x;
    });
}

TEST(Array, AnyImplicitConversion) {
    Array<Any> arr0_mixed = {11.1, 1};
    EXPECT_EQ(arr0_mixed[1].cast<int>(), 1);

    AnyView view0 = arr0_mixed;
    EXPECT_EQ(view0.CopyToTVMFFIAny().type_index, kTVMFFIArray);
    // return;

    auto arr0_float = view0.cast<Array<double>>();
    // return;
    // they are not the same because arr_mixed
    // stores arr_mixed[1] as int but we need to convert to float
    EXPECT_TRUE(!arr0_float.same_as(arr0_mixed));
    EXPECT_EQ(arr0_float[1], 1.0);

    Any any1 = arr0_float;
    // if storage check passes, the same array get returned
    auto arr1_float = any1.cast<Array<double>>();
    EXPECT_TRUE(arr1_float.same_as(arr0_float));
    // total count equals 3 include any1
    EXPECT_EQ(arr1_float.use_count(), 3);

    // convert to Array<Any> do not need any conversion
    auto arr1_mixed = any1.cast<Array<Any>>();
    EXPECT_TRUE(arr1_mixed.same_as(arr1_float));
    EXPECT_EQ(arr1_float.use_count(), 4);
}

TEST(Array, AnyConvertCheck) {
    Array<Any> arr = {11.1, 1};
    EXPECT_EQ(arr[1].cast<int>(), 1);
    return;

    AnyView view0 = arr;
    auto arr1 = view0.cast<Array<double>>();
    EXPECT_EQ(arr1[0], 11.1);
    EXPECT_EQ(arr1[1], 1.0);

    Any any1 = arr;

    EXPECT_THROW(
            {
                try {
                    [[maybe_unused]] auto arr2 = any1.cast<Array<int>>();
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "TypeError");
                    std::string what = error.what();
                    EXPECT_NE(what.find("Cannot convert from type `Array[index 0: float]` to `Array<int>`"),
                              std::string::npos);
                    throw;
                }
            },
            ::litetvm::ffi::Error);

    Array<Array<TNumber>> arr_nested = {{}, {TInt(1), TFloat(2)}};
    any1 = arr_nested;
    auto arr1_nested = any1.cast<Array<Array<TNumber>>>();
    EXPECT_EQ(arr1_nested.use_count(), 3);

    EXPECT_THROW(
            {
                try {
                    [[maybe_unused]] auto arr2 = any1.cast<Array<Array<int>>>();
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "TypeError");
                    std::string what = error.what();
                    EXPECT_NE(what.find("`Array[index 1: Array[index 0: test.Int]]` to `Array<Array<int>>`"),
                              std::string::npos);
                    throw;
                }
            },
            ::litetvm::ffi::Error);
}

TEST(Array, Upcast) {
    Array<int> a0 = {1, 2, 3};
    Array<Any> a1 = a0;
    EXPECT_EQ(a1[0].cast<int>(), 1);
    EXPECT_EQ(a1[1].cast<int>(), 2);
    EXPECT_EQ(a1[2].cast<int>(), 3);

    Array<Array<int>> a2 = {a0};
    Array<Array<Any>> a3 = a2;
    Array<Array<Any>> a4 = a2;

    static_assert(details::type_contains_v<Array<Any>, Array<int>>);
    static_assert(details::type_contains_v<Any, Array<float>>);
}

}// namespace