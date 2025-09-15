//
// Created by richard on 5/15/25.
//
#include "ffi/error.h"
#include "ffi/c_api.h"

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

void TVMFFIErrorSetRaised(TVMFFIObjectHandle error) {
    litetvm::ffi::SafeCallContext::ThreadLocal()->SetRaised(error);
}

void TVMFFIErrorMoveFromRaised(TVMFFIObjectHandle* result) {
    litetvm::ffi::SafeCallContext::ThreadLocal()->MoveFromRaised(result);
}

TVMFFIObjectHandle TVMFFIErrorCreate(const TVMFFIByteArray* kind, const TVMFFIByteArray* message,
                                     const TVMFFIByteArray* traceback) {
    TVM_FFI_LOG_EXCEPTION_CALL_BEGIN();
    litetvm::ffi::Error error(std::string(kind->data, kind->size),
                          std::string(message->data, message->size),
                          std::string(traceback->data, traceback->size));
    TVMFFIObjectHandle out =
            litetvm::ffi::details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(error));
    return out;
    TVM_FFI_LOG_EXCEPTION_CALL_END(TVMFFIErrorCreate);
}