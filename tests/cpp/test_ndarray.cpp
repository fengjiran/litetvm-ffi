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


TEST(NDArray, Basic) {
    Tensor nd = Empty(Shape({1, 2, 3}), DLDataType({kDLFloat, 32, 1}), DLDevice({kDLCPU, 0}));
    Shape shape = nd.shape();
    EXPECT_EQ(shape.size(), 3);
    EXPECT_EQ(shape[0], 1);
    EXPECT_EQ(shape[1], 2);
    EXPECT_EQ(shape[2], 3);
    EXPECT_EQ(nd.dtype(), DLDataType({kDLFloat, 32, 1}));
    for (int64_t i = 0; i < shape.Product(); ++i) {
        static_cast<float*>(nd->data)[i] = static_cast<float>(i);
    }

    Any any0 = nd;
    Tensor nd2 = any0.as<Tensor>().value();
    EXPECT_EQ(nd2.shape(), shape);
    EXPECT_EQ(nd2.dtype(), DLDataType({kDLFloat, 32, 1}));
    for (int64_t i = 0; i < shape.Product(); ++i) {
        EXPECT_EQ(static_cast<float*>(nd2->data)[i], i);
    }

    EXPECT_EQ(nd.IsContiguous(), true);
    EXPECT_EQ(nd2.use_count(), 3);
}

TEST(NDArray, DLPack) {
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

TEST(NDArray, DLPackVersioned) {
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

}// namespace
