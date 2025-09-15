//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_FFI_ERROR_H
#define LITETVM_FFI_ERROR_H

#include "ffi/base_details.h"
#include "ffi/c_api.h"
#include "ffi/memory.h"
#include "ffi/object.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

/*!
 * \brief Macro defines whether we enable libbacktrace
 */
#ifndef TVM_FFI_USE_LIBBACKTRACE
#define TVM_FFI_USE_LIBBACKTRACE 1
#endif

/*!
 * \brief Macro defines whether to install signal handler
 *   and print backtrace during segfault
 */
#ifndef TVM_FFI_BACKTRACE_ON_SEGFAULT
#define TVM_FFI_BACKTRACE_ON_SEGFAULT 1
#endif

#ifndef TVM_FFI_ALWAYS_LOG_BEFORE_THROW
#define TVM_FFI_ALWAYS_LOG_BEFORE_THROW 0
#endif

namespace litetvm {
namespace ffi {

/*!
 * \brief Error already set in frontend env.
 *
 *  This error can be thrown by EnvCheckSignals to indicate
 *  that there is an error set in the frontend environment(e.g.
 *  python interpreter). The TVM FFI should catch this error
 *  and return a proper code tell the frontend caller about
 *  this fact.
 *
 * \code
 *
 * void ExampleLongRunningFunction() {
 *   if (TVMFFIEnvCheckSignals() != 0) {
 *     throw ::tvm::ffi::EnvErrorAlreadySet();
 *   }
 *   // do work here
 * }
 *
 * \endcode
 */
struct EnvErrorAlreadySet : std::exception {};

/*!
 * \brief Error object class.
 */
class ErrorObj : public Object, public TVMFFIErrorCell {
public:
    static constexpr int32_t _type_index = kTVMFFIError;
    static constexpr const char* _type_key = "ffi.Error";

    TVM_FFI_DECLARE_STATIC_OBJECT_INFO(ErrorObj, Object);
};

namespace details {
class ErrorObjFromStd : public ErrorObj {
public:
    ErrorObjFromStd(std::string kind, std::string message, std::string traceback)
        : kind_data_(std::move(kind)), message_data_(std::move(message)), traceback_data_(std::move(traceback)) {
        this->kind = TVMFFIByteArray{kind_data_.data(), kind_data_.length()};
        this->message = TVMFFIByteArray{message_data_.data(), message_data_.length()};
        this->traceback = TVMFFIByteArray{traceback_data_.data(), traceback_data_.length()};
        this->update_traceback = UpdateTraceback;
    }

private:
    /*!
   * \brief Update the traceback of the error object.
   * \param traceback_str The traceback to update.
   */
    static void UpdateTraceback(TVMFFIObjectHandle self, const TVMFFIByteArray* traceback_str) {
        auto* obj = static_cast<ErrorObjFromStd*>(self);
        obj->traceback_data_ = std::string(traceback_str->data, traceback_str->size);
        obj->traceback = TVMFFIByteArray{obj->traceback_data_.data(), obj->traceback_data_.length()};
    }

    std::string kind_data_;
    std::string message_data_;
    std::string traceback_data_;
};
}// namespace details

/*!
 * \brief Managed reference to ErrorObj
 * \sa Error Object
 */
class Error : public ObjectRef, public std::exception {
public:
    Error(const std::string& kind, const std::string& message, const std::string& traceback) {
        data_ = make_object<details::ErrorObjFromStd>(kind, message, traceback);
    }

    Error(const std::string& kind, const std::string& message, const TVMFFIByteArray* traceback)
        : Error(kind, message, std::string(traceback->data, traceback->size)) {}

    NODISCARD std::string kind() const {
        auto* obj = static_cast<ErrorObj*>(data_.get());
        return {obj->kind.data, obj->kind.size};
    }

    NODISCARD std::string message() const {
        auto* obj = static_cast<ErrorObj*>(data_.get());
        return {obj->message.data, obj->message.size};
    }

    NODISCARD std::string traceback() const {
        auto* obj = static_cast<ErrorObj*>(data_.get());
        return {obj->traceback.data, obj->traceback.size};
    }

    void UpdateTraceback(const TVMFFIByteArray* traceback_str) const {
        auto* obj = static_cast<ErrorObj*>(data_.get());
        obj->update_traceback(obj, traceback_str);
    }

    NODISCARD const char* what() const noexcept override {
        thread_local std::string what_data;
        auto* obj = static_cast<ErrorObj*>(data_.get());
        what_data = std::string("Traceback (most recent call last):\n") +
                    std::string(obj->traceback.data, obj->traceback.size) +
                    std::string(obj->kind.data, obj->kind.size) + std::string(": ") +
                    std::string(obj->message.data, obj->message.size) + '\n';
        return what_data.c_str();
    }

    TVM_FFI_DEFINE_NOTNULLABLE_OBJECT_REF_METHODS(Error, ObjectRef, ErrorObj);
};

namespace details {

class ErrorBuilder {
public:
    explicit ErrorBuilder(std::string kind, std::string traceback, bool log_before_throw)
        : kind_(std::move(kind)), traceback_(std::move(traceback)), log_before_throw_(log_before_throw) {}

    explicit ErrorBuilder(std::string kind, const TVMFFIByteArray* traceback, bool log_before_throw)
        : ErrorBuilder(std::move(kind), std::string(traceback->data, traceback->size), log_before_throw) {}

// MSVC disable warning in error builder as it is exepected
#ifdef _MSC_VER
#pragma disagnostic push
#pragma warning(disable : 4722)
#endif
    // avoid inline to reduce binary size, error throw path do not need to be fast
    [[noreturn]] ~ErrorBuilder() noexcept(false) {
        Error error(kind_, stream_.str(), traceback_);
        if (log_before_throw_) {
            std::cerr << error.what();
        }
        throw error;
    }
#ifdef _MSC_VER
#pragma disagnostic pop
#endif

    std::ostringstream& stream() {
        return stream_;
    }

protected:
    std::string kind_;
    std::ostringstream stream_;
    std::string traceback_;
    bool log_before_throw_;
};

// define traceback here as call into traceback function
#define TVM_FFI_TRACEBACK_HERE TVMFFITraceback(__FILE__, __LINE__, TVM_FFI_FUNC_SIG)
}// namespace details

/*!
 * \brief Helper macro to throw an error with traceback and message
 *
 * \code
 *
 *   void ThrowError() {
 *     TVM_FFI_THROW(RuntimeError) << "error message";
 *   }
 *
 * \endcode
 */
#define TVM_FFI_THROW(ErrorKind)                                              \
    ::litetvm::ffi::details::ErrorBuilder(#ErrorKind, TVM_FFI_TRACEBACK_HERE, \
                                          TVM_FFI_ALWAYS_LOG_BEFORE_THROW)    \
            .stream()

/*!
 * \brief Explicitly log error in stderr and then throw the error.
 *
 * \note This is only necessary on startup functions where we know error
 *  cannot be caught, and it is better to have a clear log message.
 *  In most cases, we should use use TVM_FFI_THROW.
 */
#define TVM_FFI_LOG_AND_THROW(ErrorKind) \
    ::litetvm::ffi::details::ErrorBuilder(#ErrorKind, TVM_FFI_TRACEBACK_HERE, true).stream()

// Glog style checks with TVM_FFI prefix
// NOTE: we explicitly avoid glog style generic macros (LOG/CHECK) in tvm ffi
// to avoid potential conflict of downstream users who might have their own GLOG style macros
namespace details {

template<typename X, typename Y>
TVM_FFI_INLINE std::unique_ptr<std::string> LogCheckFormat(const X& x, const Y& y) {
    std::ostringstream os;
    os << " (" << x << " vs. " << y << ") ";// CHECK_XX(x, y) requires x and y can be serialized to
                                            // string. Use CHECK(x OP y) otherwise.
    return std::make_unique<std::string>(os.str());
}

#define TVM_FFI_CHECK_FUNC(name, op)                                                     \
    template<typename X, typename Y>                                                     \
    TVM_FFI_INLINE std::unique_ptr<std::string> LogCheck##name(const X& x, const Y& y) { \
        if (x op y) return nullptr;                                                      \
        return LogCheckFormat(x, y);                                                     \
    }                                                                                    \
    TVM_FFI_INLINE std::unique_ptr<std::string> LogCheck##name(int x, int y) {           \
        return LogCheck##name<int, int>(x, y);                                           \
    }

// Inline _Pragma in macros does not work reliably on old version of MSVC and
// GCC. We wrap all comparisons in a function so that we can use #pragma to
// silence bad comparison warnings.
#if defined(__GNUC__) || defined(__clang__)// GCC and Clang
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#elif defined(_MSC_VER)// MSVC
#pragma warning(push)
#pragma warning(disable : 4389)// '==' : signed/unsigned mismatch
#endif

TVM_FFI_CHECK_FUNC(_LT, <)
TVM_FFI_CHECK_FUNC(_GT, >)
TVM_FFI_CHECK_FUNC(_LE, <=)
TVM_FFI_CHECK_FUNC(_GE, >=)
TVM_FFI_CHECK_FUNC(_EQ, ==)
TVM_FFI_CHECK_FUNC(_NE, !=)

#if defined(__GNUC__) || defined(__clang__)// GCC and Clang
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)// MSVC
#pragma warning(pop)
#endif
}// namespace details

#define TVM_FFI_ICHECK_BINARY_OP(name, op, x, y)                              \
    if (auto __tvm__log__err = ::litetvm::ffi::details::LogCheck##name(x, y)) \
    TVM_FFI_THROW(InternalError) << "Check failed: " << #x " " #op " " #y << *__tvm__log__err << ": "

#define TVM_FFI_ICHECK(x) \
    if (!(x)) TVM_FFI_THROW(InternalError) << "Check failed: (" #x << ") is false: "

#define TVM_FFI_ICHECK_LT(x, y) TVM_FFI_ICHECK_BINARY_OP(_LT, <, x, y)
#define TVM_FFI_ICHECK_GT(x, y) TVM_FFI_ICHECK_BINARY_OP(_GT, >, x, y)
#define TVM_FFI_ICHECK_LE(x, y) TVM_FFI_ICHECK_BINARY_OP(_LE, <=, x, y)
#define TVM_FFI_ICHECK_GE(x, y) TVM_FFI_ICHECK_BINARY_OP(_GE, >=, x, y)
#define TVM_FFI_ICHECK_EQ(x, y) TVM_FFI_ICHECK_BINARY_OP(_EQ, ==, x, y)
#define TVM_FFI_ICHECK_NE(x, y) TVM_FFI_ICHECK_BINARY_OP(_NE, !=, x, y)
#define TVM_FFI_ICHECK_NOTNULL(x)                                                   \
    ((x) == nullptr ? TVM_FFI_THROW(InternalError) << "Check not null: " #x << ' ', \
     (x)            : (x))// NOLINT(*)
}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_ERROR_H
