//
// Created by 赵丹 on 2025/8/28.
//
#include "ffi/extra/c_env_api.h"
#include "ffi/function.h"

#include <vector>

namespace litetvm {
namespace ffi {

class StreamContext {
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

    static StreamContext* ThreadLocal() {
        static thread_local StreamContext inst;
        return &inst;
    }

private:
    std::vector<std::vector<TVMFFIStreamHandle>> stream_table_;
};

}// namespace ffi
}// namespace litetvm

int TVMFFIEnvSetStream(int32_t device_type, int32_t device_id, TVMFFIStreamHandle stream,
                       TVMFFIStreamHandle* out_original_stream) {
    TVM_FFI_SAFE_CALL_BEGIN();
    litetvm::ffi::StreamContext::ThreadLocal()->SetStream(device_type, device_id, stream,
                                                          out_original_stream);
    TVM_FFI_SAFE_CALL_END();
}

TVMFFIStreamHandle TVMFFIEnvGetCurrentStream(int32_t device_type, int32_t device_id) {
    TVM_FFI_LOG_EXCEPTION_CALL_BEGIN();
    return litetvm::ffi::StreamContext::ThreadLocal()->GetStream(device_type, device_id);
    TVM_FFI_LOG_EXCEPTION_CALL_END(TVMFFIEnvGetCurrentStream);
}
