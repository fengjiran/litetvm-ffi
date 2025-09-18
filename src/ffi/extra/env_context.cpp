//
// Created by 赵丹 on 2025/8/28.
//
#include "ffi/extra/c_env_api.h"
#include "ffi/function.h"

#include <vector>

namespace litetvm {
namespace ffi {

class EnvContext {
public:
    void SetStream(int32_t device_type, int32_t device_id, TVMFFIStreamHandle stream,
                   TVMFFIStreamHandle* out_original_stream) {
        if (static_cast<size_t>(device_type) >= stream_table_.size()) {
            stream_table_.resize(device_type + 1);
        }
        if (static_cast<size_t>(device_id) >= stream_table_[device_type].size()) {
            stream_table_[device_type].resize(device_id + 1, nullptr);
        }
        if (out_original_stream != nullptr) {
            *out_original_stream = stream_table_[device_type][device_id];
        }
        stream_table_[device_type][device_id] = stream;
    }

    TVMFFIStreamHandle GetStream(int32_t device_type, int32_t device_id) {
        if (static_cast<size_t>(device_type) < stream_table_.size() &&
            static_cast<size_t>(device_id) < stream_table_[device_type].size()) {
            return stream_table_[device_type][device_id];
        }
        return nullptr;
    }

    DLPackTensorAllocator GetDLPackTensorAllocator() {
        if (dlpack_allocator_ != nullptr) {
            return dlpack_allocator_;
        }
        return GlobalTensorAllocator();
    }

    void SetDLPackTensorAllocator(DLPackTensorAllocator allocator, int write_to_global_context,
                                  DLPackTensorAllocator* opt_out_original_allocator) {
        dlpack_allocator_ = allocator;
        if (write_to_global_context != 0) {
            GlobalTensorAllocator() = allocator;
        }
        if (opt_out_original_allocator != nullptr) {
            *opt_out_original_allocator = dlpack_allocator_;
        }
        dlpack_allocator_ = allocator;
    }

    static EnvContext* ThreadLocal() {
        static thread_local EnvContext inst;
        return &inst;
    }

private:
    // use static function to avoid static initialization order issue
    static DLPackTensorAllocator& GlobalTensorAllocator() {// NOLINT(*)
        static DLPackTensorAllocator allocator = nullptr;
        return allocator;
    }
    std::vector<std::vector<TVMFFIStreamHandle>> stream_table_;
    DLPackTensorAllocator dlpack_allocator_ = nullptr;
};

}// namespace ffi
}// namespace litetvm

int TVMFFIEnvSetStream(int32_t device_type, int32_t device_id, TVMFFIStreamHandle stream,
                       TVMFFIStreamHandle* out_original_stream) {
    TVM_FFI_SAFE_CALL_BEGIN();
    litetvm::ffi::EnvContext::ThreadLocal()->SetStream(device_type, device_id, stream,
                                                       out_original_stream);
    TVM_FFI_SAFE_CALL_END();
}

TVMFFIStreamHandle TVMFFIEnvGetStream(int32_t device_type, int32_t device_id) {
    TVM_FFI_LOG_EXCEPTION_CALL_BEGIN();
    return litetvm::ffi::EnvContext::ThreadLocal()->GetStream(device_type, device_id);
    TVM_FFI_LOG_EXCEPTION_CALL_END(TVMFFIEnvGetStream);
}

int TVMFFIEnvSetTensorAllocator(DLPackTensorAllocator allocator, int write_to_global_context,
                                DLPackTensorAllocator* opt_out_original_allocator) {
    TVM_FFI_SAFE_CALL_BEGIN();
    litetvm::ffi::EnvContext::ThreadLocal()->SetDLPackTensorAllocator(allocator, write_to_global_context,
                                                                      opt_out_original_allocator);
    TVM_FFI_SAFE_CALL_END();
}

DLPackTensorAllocator TVMFFIEnvGetTensorAllocator() {
    TVM_FFI_LOG_EXCEPTION_CALL_BEGIN();
    return litetvm::ffi::EnvContext::ThreadLocal()->GetDLPackTensorAllocator();
    TVM_FFI_LOG_EXCEPTION_CALL_END(TVMFFIEnvGetTensorAllocator);
}