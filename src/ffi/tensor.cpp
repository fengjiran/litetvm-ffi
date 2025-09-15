//
// Created by richard on 5/15/25.
//

#include "ffi/c_api.h"
#include "ffi/container/tensor.h"
#include "ffi/function.h"
#include "ffi/reflection/registry.h"

namespace litetvm {
namespace ffi {

// Shape
TVM_FFI_STATIC_INIT_BLOCK({
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
        *ret = Shape(shape);
    });
});
}// namespace ffi
}// namespace litetvm

int TVMFFINDArrayFromDLPack(DLManagedTensor* from, int32_t min_alignment,
                            int32_t require_contiguous, TVMFFIObjectHandle* out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    Tensor nd = Tensor::FromDLPack(from, static_cast<size_t>(min_alignment), require_contiguous);
    *out = details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(nd));
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFINDArrayFromDLPackVersioned(DLManagedTensorVersioned* from, int32_t min_alignment,
                                     int32_t require_contiguous, TVMFFIObjectHandle* out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    Tensor nd = Tensor::FromDLPackVersioned(from, static_cast<size_t>(min_alignment), require_contiguous);
    *out = details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(nd));
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFINDArrayToDLPack(TVMFFIObjectHandle from, DLManagedTensor** out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    *out = details::ObjectUnsafe::RawObjectPtrFromUnowned<TensorObj>(static_cast<TVMFFIObject*>(from))
                   ->ToDLPack();
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFINDArrayToDLPackVersioned(TVMFFIObjectHandle from, DLManagedTensorVersioned** out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    *out = details::ObjectUnsafe::RawObjectPtrFromUnowned<TensorObj>(static_cast<TVMFFIObject*>(from))
                   ->ToDLPackVersioned();
    TVM_FFI_SAFE_CALL_END();
}
