//
// Created by richard on 5/15/25.
//
#include "ffi/error.h"
#include "ffi/c_api.h"

#include <cstring>

namespace litetvm {
namespace ffi {

class SafeCallContext {
public:
    void SetRaised(TVMFFIObjectHandle error) {
        last_error_ =
                details::ObjectUnsafe::ObjectPtrFromUnowned<ErrorObj>(static_cast<TVMFFIObject*>(error));
    }

    void SetRaisedByCstr(const char* kind, const char* message, const TVMFFIByteArray* traceback) {
        Error error(kind, message, traceback);
        last_error_ = details::ObjectUnsafe::ObjectPtrFromObjectRef<ErrorObj>(std::move(error));
    }

    void SetRaisedByCstrParts(const char* kind, const char** message_parts, int32_t num_parts,
                            const TVMFFIByteArray* backtrace) {
        std::string message;
        size_t total_len = 0;
        for (int i = 0; i < num_parts; ++i) {
            if (message_parts[i] != nullptr) {
                total_len += std::strlen(message_parts[i]);
            }
        }
        message.reserve(total_len);
        for (int i = 0; i < num_parts; ++i) {
            if (message_parts[i] != nullptr) {
                message.append(message_parts[i]);
            }
        }
        Error error(kind, message, backtrace);
        last_error_ = details::ObjectUnsafe::ObjectPtrFromObjectRef<ErrorObj>(std::move(error));
    }

    void MoveFromRaised(TVMFFIObjectHandle* result) {
        result[0] = details::ObjectUnsafe::MoveObjectPtrToTVMFFIObjectPtr(std::move(last_error_));
    }

    static SafeCallContext* ThreadLocal() {
        static thread_local SafeCallContext ctx;
        return &ctx;
    }

private:
    ObjectPtr<ErrorObj> last_error_;
};

}// namespace ffi
}// namespace litetvm

void TVMFFIErrorSetRaisedFromCStr(const char* kind, const char* message) {
    // NOTE: run traceback here to simplify the depth of tracekback
    litetvm::ffi::SafeCallContext::ThreadLocal()->SetRaisedByCstr(kind, message, TVM_FFI_TRACEBACK_HERE);
}

void TVMFFIErrorSetRaisedFromCStrParts(const char* kind, const char** message_parts,
                                       int32_t num_parts) {
    // NOTE: run backtrace here to simplify the depth of tracekback
    litetvm::ffi::SafeCallContext::ThreadLocal()->SetRaisedByCstrParts(
        kind, message_parts, num_parts, TVMFFITraceback(nullptr, 0, nullptr));
}

void TVMFFIErrorSetRaised(TVMFFIObjectHandle error) {
    litetvm::ffi::SafeCallContext::ThreadLocal()->SetRaised(error);
}

void TVMFFIErrorMoveFromRaised(TVMFFIObjectHandle* result) {
    litetvm::ffi::SafeCallContext::ThreadLocal()->MoveFromRaised(result);
}

int TVMFFIErrorCreate(const TVMFFIByteArray* kind, const TVMFFIByteArray* message,
                      const TVMFFIByteArray* backtrace, TVMFFIObjectHandle* out) {
    // log other errors to the logger
    TVM_FFI_LOG_EXCEPTION_CALL_BEGIN();
    try {
        litetvm::ffi::Error error(std::string(kind->data, kind->size),
                                  std::string(message->data, message->size),
                                  std::string(backtrace->data, backtrace->size));
        *out = litetvm::ffi::details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(error));
        return 0;
    } catch (const std::bad_alloc& e) {
        return -1;
    }
    TVM_FFI_LOG_EXCEPTION_CALL_END(TVMFFIErrorCreate);
}