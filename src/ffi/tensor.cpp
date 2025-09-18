//
// Created by richard on 5/15/25.
//

#include "ffi/container/tensor.h"
#include "ffi/c_api.h"
#include "ffi/function.h"
#include "ffi/reflection/registry.h"

namespace litetvm {
namespace ffi {

// Shape
TVM_FFI_STATIC_INIT_BLOCK() {
    namespace refl = litetvm::ffi::reflection;
    refl::GlobalDef().def_packed("ffi.Shape", [](ffi::PackedArgs args, Any* ret) {
        int64_t* mutable_data;
        ObjectPtr<ShapeObj> shape = details::MakeEmptyShape(args.size(), &mutable_data);
        for (int i = 0; i < args.size(); ++i) {
            if (auto opt_int = args[i].try_cast<int64_t>()) {
                mutable_data[i] = *opt_int;
            } else {
                TVM_FFI_THROW(ValueError) << "Expect shape to take list of int arguments";
            }
        }
        *ret = details::ObjectUnsafe::ObjectRefFromObjectPtr<Shape>(shape);
    });
}
}// namespace ffi
}// namespace litetvm

int TVMFFITensorFromDLPack(DLManagedTensor* from, int32_t min_alignment,
                           int32_t require_contiguous, TVMFFIObjectHandle* out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    Tensor nd = Tensor::FromDLPack(from, static_cast<size_t>(min_alignment), require_contiguous);
    *out = details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(nd));
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFITensorFromDLPackVersioned(DLManagedTensorVersioned* from, int32_t min_alignment,
                                    int32_t require_contiguous, TVMFFIObjectHandle* out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    Tensor nd = Tensor::FromDLPackVersioned(from, static_cast<size_t>(min_alignment), require_contiguous);
    *out = details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(nd));
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFITensorToDLPack(TVMFFIObjectHandle from, DLManagedTensor** out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    *out = details::ObjectUnsafe::RawObjectPtrFromUnowned<TensorObj>(static_cast<TVMFFIObject*>(from))
                   ->ToDLPack();
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFITensorToDLPackVersioned(TVMFFIObjectHandle from, DLManagedTensorVersioned** out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    *out = details::ObjectUnsafe::RawObjectPtrFromUnowned<TensorObj>(static_cast<TVMFFIObject*>(from))
                   ->ToDLPackVersioned();
    TVM_FFI_SAFE_CALL_END();
}
