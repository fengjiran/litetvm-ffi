//
// Created by 赵丹 on 25-6-5.
//

#include "ffi/container/tensor.h"

#include <gtest/gtest.h>

#include <utility>

namespace {
using namespace litetvm::ffi;

struct CPUNDAlloc {
    void AllocData(DLTensor* tensor) {
        tensor->data = malloc(GetDataSize(*tensor));
    }

    void FreeData(DLTensor* tensor) {
        free(tensor->data);
    }
};

Tensor Empty(Shape shape, DLDataType dtype, DLDevice device) {
    return Tensor::FromNDAlloc(CPUNDAlloc(), std::move(shape), dtype, device);
}

int TestDLPackTensorAllocator(DLTensor* prototype, DLManagedTensorVersioned** out, void* error_ctx,
                              void (*SetError)(void* error_ctx, const char* kind,
                                               const char* message)) {
    Shape shape(prototype->shape, prototype->shape + prototype->ndim);
    Tensor nd = Empty(shape, prototype->dtype, prototype->device);
    *out = nd.ToDLPackVersioned();
    return 0;
}

int TestDLPackTensorAllocatorError(DLTensor* prototype, DLManagedTensorVersioned** out,
                                   void* error_ctx,
                                   void (*SetError)(void* error_ctx, const char* kind,
                                                    const char* message)) {
    SetError(error_ctx, "RuntimeError", "TestDLPackTensorAllocatorError");
    return -1;
}

TEST(Tensor, Basic) {
    Tensor nd = Empty({1, 2, 3}, DLDataType({kDLFloat, 32, 1}), DLDevice({kDLCPU, 0}));
    Shape shape = nd.shape();
    EXPECT_EQ(shape.size(), 3);
    EXPECT_EQ(shape[0], 1);
    EXPECT_EQ(shape[1], 2);
    EXPECT_EQ(shape[2], 3);
    EXPECT_EQ(nd.dtype(), DLDataType({kDLFloat, 32, 1}));
    for (int64_t i = 0; i < shape.Product(); ++i) {
        static_cast<float*>(nd->data)[i] = static_cast<float>(i);
    }

    EXPECT_EQ(nd.numel(), 6);
    EXPECT_EQ(nd.ndim(), 3);
    EXPECT_EQ(nd.data_ptr(), nd->data);

    Any any0 = nd;
    Tensor nd2 = any0.as<Tensor>().value();
    for (int64_t i = 0; i < shape.Product(); ++i) {
        EXPECT_EQ(static_cast<float*>(nd2->data)[i], i);
    }

    EXPECT_EQ(nd.IsContiguous(), true);
    EXPECT_EQ(nd2.use_count(), 3);
}

TEST(Tensor, DLPack) {
    Tensor nd = Empty({1, 2, 3}, DLDataType({kDLInt, 16, 1}), DLDevice({kDLCPU, 0}));
    DLManagedTensor* dlpack = nd.ToDLPack();
    EXPECT_EQ(dlpack->dl_tensor.ndim, 3);
    EXPECT_EQ(dlpack->dl_tensor.shape[0], 1);
    EXPECT_EQ(dlpack->dl_tensor.shape[1], 2);
    EXPECT_EQ(dlpack->dl_tensor.shape[2], 3);
    EXPECT_EQ(dlpack->dl_tensor.dtype.code, kDLInt);
    EXPECT_EQ(dlpack->dl_tensor.dtype.bits, 16);
    EXPECT_EQ(dlpack->dl_tensor.dtype.lanes, 1);
    EXPECT_EQ(dlpack->dl_tensor.device.device_type, kDLCPU);
    EXPECT_EQ(dlpack->dl_tensor.device.device_id, 0);
    EXPECT_EQ(dlpack->dl_tensor.byte_offset, 0);
    EXPECT_EQ(dlpack->dl_tensor.strides[0], 6);
    EXPECT_EQ(dlpack->dl_tensor.strides[1], 3);
    EXPECT_EQ(dlpack->dl_tensor.strides[2], 1);
    EXPECT_EQ(nd.use_count(), 2);
    {
        Tensor nd2 = Tensor::FromDLPack(dlpack);
        EXPECT_EQ(nd2.use_count(), 1);
        EXPECT_EQ(nd2->data, nd->data);
        EXPECT_EQ(nd.use_count(), 2);
        EXPECT_EQ(nd2.use_count(), 1);
    }
    EXPECT_EQ(nd.use_count(), 1);
}

TEST(Tensor, DLPackVersioned) {
    DLDataType dtype = DLDataType({kDLFloat4_e2m1fn, 4, 1});
    EXPECT_EQ(GetDataSize(2, dtype), 2 * 4 / 8);
    Tensor nd = Empty({2}, dtype, DLDevice({kDLCPU, 0}));
    DLManagedTensorVersioned* dlpack = nd.ToDLPackVersioned();
    EXPECT_EQ(dlpack->version.major, DLPACK_MAJOR_VERSION);
    EXPECT_EQ(dlpack->version.minor, DLPACK_MINOR_VERSION);
    EXPECT_EQ(dlpack->dl_tensor.ndim, 1);
    EXPECT_EQ(dlpack->dl_tensor.shape[0], 2);
    EXPECT_EQ(dlpack->dl_tensor.dtype.code, kDLFloat4_e2m1fn);
    EXPECT_EQ(dlpack->dl_tensor.dtype.bits, 4);
    EXPECT_EQ(dlpack->dl_tensor.dtype.lanes, 1);
    EXPECT_EQ(dlpack->dl_tensor.device.device_type, kDLCPU);
    EXPECT_EQ(dlpack->dl_tensor.device.device_id, 0);
    EXPECT_EQ(dlpack->dl_tensor.byte_offset, 0);
    EXPECT_EQ(dlpack->dl_tensor.strides[0], 1);

    EXPECT_EQ(nd.use_count(), 2);
    {
        Tensor nd2 = Tensor::FromDLPackVersioned(dlpack);
        EXPECT_EQ(nd2.use_count(), 1);
        EXPECT_EQ(nd2->data, nd->data);
        EXPECT_EQ(nd.use_count(), 2);
        EXPECT_EQ(nd2.use_count(), 1);
    }
    EXPECT_EQ(nd.use_count(), 1);
}

TEST(Tensor, DLPackAlloc) {
    // Test successful allocation
    Tensor tensor = Tensor::FromDLPackAlloc(TestDLPackTensorAllocator, {1, 2, 3},
                                            DLDataType({kDLFloat, 32, 1}), DLDevice({kDLCPU, 0}));
    EXPECT_EQ(tensor.use_count(), 1);
    EXPECT_EQ(tensor.shape().size(), 3);
    EXPECT_EQ(tensor.shape()[0], 1);
    EXPECT_EQ(tensor.shape()[1], 2);
    EXPECT_EQ(tensor.shape()[2], 3);
    EXPECT_EQ(tensor.dtype().code, kDLFloat);
    EXPECT_EQ(tensor.dtype().bits, 32);
    EXPECT_EQ(tensor.dtype().lanes, 1);
    EXPECT_EQ(tensor->device.device_type, kDLCPU);
    EXPECT_EQ(tensor->device.device_id, 0);
    EXPECT_NE(tensor->data, nullptr);
}

TEST(Tensor, DLPackAllocError) {
    // Test error handling in DLPackAlloc
    EXPECT_THROW(
            {
                Tensor::FromDLPackAlloc(TestDLPackTensorAllocatorError, {1, 2, 3},
                                        DLDataType({kDLFloat, 32, 1}), DLDevice({kDLCPU, 0}));
            },
            litetvm::ffi::Error);
}

TEST(Tensor, TensorView) {
    Tensor tensor = Empty({1, 2, 3}, DLDataType({kDLFloat, 32, 1}), DLDevice({kDLCPU, 0}));
    TensorView tensor_view = tensor;

    EXPECT_EQ(tensor_view.shape().size(), 3);
    EXPECT_EQ(tensor_view.shape()[0], 1);
    EXPECT_EQ(tensor_view.shape()[1], 2);
    EXPECT_EQ(tensor_view.shape()[2], 3);
    EXPECT_EQ(tensor_view.dtype().code, kDLFloat);
    EXPECT_EQ(tensor_view.dtype().bits, 32);
    EXPECT_EQ(tensor_view.dtype().lanes, 1);

    AnyView result = tensor_view;
    EXPECT_EQ(result.type_index(), TypeIndex::kTVMFFIDLTensorPtr);
    TensorView tensor_view2 = result.as<TensorView>().value();
    EXPECT_EQ(tensor_view2.shape().size(), 3);
    EXPECT_EQ(tensor_view2.shape()[0], 1);
    EXPECT_EQ(tensor_view2.shape()[1], 2);
    EXPECT_EQ(tensor_view2.shape()[2], 3);
    EXPECT_EQ(tensor_view2.dtype().code, kDLFloat);
    EXPECT_EQ(tensor_view2.dtype().bits, 32);
    EXPECT_EQ(tensor_view2.dtype().lanes, 1);
}

}// namespace
