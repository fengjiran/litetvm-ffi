//
// Created by richard on 6/7/25.
//
#include "ffi/any.h"
#include "ffi/error.h"
#include "ffi/optional.h"

#include <gtest/gtest.h>

namespace {
using namespace litetvm::ffi;

void ThrowRuntimeError() {
    TVM_FFI_THROW(RuntimeError) << "test0";

    // details::ErrorBuilder("RuntimeError",
    //     TVMFFITraceback("test_error.cpp", 14, __PRETTY_FUNCTION__),
    //     0).stream() << "test0";
    // std::cout << std::endl;
}

TEST(Error, Traceback) {
    EXPECT_THROW(
            {
                try {
                    ThrowRuntimeError();
                } catch (const Error& error) {
                    EXPECT_EQ(error.message(), "test0");
                    EXPECT_EQ(error.kind(), "RuntimeError");
                    std::string what = error.what();
                    // std::cout << what << std::endl;
                    // EXPECT_NE(what.find("line"), std::string::npos);
                    // EXPECT_NE(what.find("ThrowRuntimeError"), std::string::npos);
                    EXPECT_NE(what.find("RuntimeError: test0"), std::string::npos);
                    throw;
                }
            },
            ::litetvm::ffi::Error);
}

TEST(CheckError, Traceback) {
    EXPECT_THROW(
            {
                try {
                    TVM_FFI_ICHECK_GT(2, 3);
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "InternalError");
                    std::string what = error.what();
                    // EXPECT_NE(what.find("line"), std::string::npos);
                    EXPECT_NE(what.find("2 > 3"), std::string::npos);
                    throw;
                }
            },
            ::litetvm::ffi::Error);
}

TEST(CheckError, ValueError) {
    int value = -5;
    EXPECT_THROW(
            {
                try {
                    TVM_FFI_CHECK(value >= 0, ValueError) << "Value must be non-negative, got " << value;
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "ValueError");
                    std::string what = error.what();
                    EXPECT_NE(what.find("line"), std::string::npos);
                    EXPECT_NE(what.find("Check failed: (value >= 0) is false"), std::string::npos);
                    EXPECT_NE(what.find("Value must be non-negative, got -5"), std::string::npos);
                    throw;
                }
            },
            ::litetvm::ffi::Error);
}

TEST(CheckError, IndexError) {
    int index = 10;
    int array_size = 5;
    EXPECT_THROW(
            {
                try {
                    TVM_FFI_CHECK(index < array_size, IndexError)
                            << "Index " << index << " out of bounds for array of size " << array_size;
                } catch (const Error& error) {
                    EXPECT_EQ(error.kind(), "IndexError");
                    std::string what = error.what();
                    EXPECT_NE(what.find("line"), std::string::npos);
                    EXPECT_NE(what.find("Check failed: (index < array_size) is false"), std::string::npos);
                    EXPECT_NE(what.find("Index 10 out of bounds for array of size 5"), std::string::npos);
                    throw;
                }
            },
            ::litetvm::ffi::Error);
}

TEST(CheckError, PassingCondition) {
    // This should not throw
    EXPECT_NO_THROW(TVM_FFI_CHECK(true, ValueError));
    EXPECT_NO_THROW(TVM_FFI_CHECK(5 < 10, IndexError));
}


TEST(Error, AnyConvert) {
    Any any = Error("TypeError", "here", "test0");
    Optional<Error> opt_err = any.as<Error>();
    EXPECT_EQ(opt_err.value().kind(), "TypeError");
    EXPECT_EQ(opt_err.value().message(), "here");
}

}// namespace