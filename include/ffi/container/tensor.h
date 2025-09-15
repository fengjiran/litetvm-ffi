//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_FFI_CONTAINER_NDARRAY_H
#define LITETVM_FFI_CONTAINER_NDARRAY_H

#include <ffi/container/shape.h>
#include <ffi/dtype.h>
#include <ffi/error.h>

#include <utility>

namespace litetvm {
namespace ffi {

/*!
 * \brief check if a DLTensor is contiguous.
 * \param arr The input DLTensor.
 * \return The check result.
 */
inline bool IsContiguous(const DLTensor& arr) {
    if (arr.strides == nullptr) {
        return true;
    }

    int64_t expected_stride = 1;
    for (int32_t i = arr.ndim; i != 0; --i) {
        int32_t k = i - 1;
        if (arr.shape[k] == 1) {
            // Skip stride check if shape[k] is 1, where the dimension is contiguous
            // regardless of the value of stride.
            //
            // For example, PyTorch will normalize stride to 1 if shape is 1 when exporting
            // to DLPack.
            // More context: https://github.com/pytorch/pytorch/pull/83158
            continue;
        }

        if (arr.strides[k] != expected_stride) {
            return false;
        }
        expected_stride *= arr.shape[k];
    }
    return true;
}

/**
 * \brief Check if the data in the DLTensor is aligned to the given alignment.
 * \param arr The input DLTensor.
 * \param alignment The alignment to check.
 * \return True if the data is aligned to the given alignment, false otherwise.
 */
inline bool IsAligned(const DLTensor& arr, size_t alignment) {
    // whether the device uses direct address mapping instead of indirect buffer
    bool direct_address = arr.device.device_type <= kDLCUDAHost ||
                          arr.device.device_type == kDLCUDAManaged ||
                          arr.device.device_type == kDLROCM ||
                          arr.device.device_type == kDLROCMHost;
    if (direct_address) {
        return reinterpret_cast<size_t>(static_cast<char*>(arr.data) + arr.byte_offset) % alignment == 0;
    }

    return arr.byte_offset % alignment == 0;
}

/*!
 * \brief return the total number bytes needs to store packed data
 *
 * \param numel the number of elements in the array
 * \param dtype the data type of the array
 * \return the total number bytes needs to store packed data
 */
inline size_t GetDataSize(int64_t numel, DLDataType dtype) {
    // compatible handling sub-byte uint1(bool), which usually stored as uint8_t
    // TODO(tqchen): revisit and switch to kDLBool
    if (dtype.code == kDLUInt && dtype.bits == 1 && dtype.lanes == 1) {
        return numel;
    }

    // for other sub-byte types, packing is preferred
    return (numel * dtype.bits * dtype.lanes + 7) / 8;
}

/*!
 * \brief return the size of data the DLTensor hold, in term of number of bytes
 *
 *  \param arr the input DLTensor
 *  \return number of  bytes of data in the DLTensor.
 */
inline size_t GetDataSize(const DLTensor& arr) {
    size_t size = 1;
    for (int i = 0; i < arr.ndim; ++i) {
        size *= static_cast<size_t>(arr.shape[i]);
    }
    return GetDataSize(size, arr.dtype);
}

/*! \brief An object representing an NDArray. */
class TensorObj : public Object, public DLTensor {
public:
    static constexpr uint32_t _type_index = kTVMFFINDArray;
    static constexpr const char* _type_key = StaticTypeKey::kTVMFFINDArray;
    TVM_FFI_DECLARE_STATIC_OBJECT_INFO(TensorObj, Object);

    /*!
   * \brief Move NDArray to a DLPack managed tensor.
   * \return The converted DLPack managed tensor.
   */
    NODISCARD DLManagedTensor* ToDLPack() const {
        auto* ret = new DLManagedTensor();
        auto* from = const_cast<TensorObj*>(this);
        ret->dl_tensor = *static_cast<DLTensor*>(from);
        ret->manager_ctx = from;
        ret->deleter = DLManagedTensorDeleter;
        details::ObjectUnsafe::IncRefObjectHandle(from);
        return ret;
    }

    /*!
   * \brief Move  NDArray to a DLPack managed tensor.
   * \return The converted DLPack managed tensor.
   */
    NODISCARD DLManagedTensorVersioned* ToDLPackVersioned() const {
        auto* ret = new DLManagedTensorVersioned();
        auto* from = const_cast<TensorObj*>(this);
        ret->version.major = DLPACK_MAJOR_VERSION;
        ret->version.minor = DLPACK_MINOR_VERSION;
        ret->dl_tensor = *static_cast<DLTensor*>(from);
        ret->manager_ctx = from;
        ret->deleter = DLManagedTensorVersionedDeleter;
        ret->flags = 0;
        details::ObjectUnsafe::IncRefObjectHandle(from);
        return ret;
    }

protected:
    // backs up the shape of the NDArray
    Optional<Shape> shape_data_;
    Optional<Shape> stride_data_;

    static void DLManagedTensorDeleter(DLManagedTensor* tensor) {
        auto* obj = static_cast<TensorObj*>(tensor->manager_ctx);
        details::ObjectUnsafe::DecRefObjectHandle(obj);
        delete tensor;
    }

    static void DLManagedTensorVersionedDeleter(DLManagedTensorVersioned* tensor) {
        auto* obj = static_cast<TensorObj*>(tensor->manager_ctx);
        details::ObjectUnsafe::DecRefObjectHandle(obj);
        delete tensor;
    }

    friend class Tensor;
};

namespace details {
/*!
 *\brief Helper class to create an NDArrayObj from an NDAllocator
 *
 * The underlying allocator needs to be implemented by user.
 */
template<typename TNDAlloc>
class NDArrayObjFromNDAlloc : public TensorObj {
public:
    template<typename... ExtraArgs>
    NDArrayObjFromNDAlloc(TNDAlloc alloc, Shape shape, DLDataType dtype, DLDevice device,
                          ExtraArgs&&... extra_args): alloc_(alloc) {
        this->device = device;
        this->ndim = static_cast<int>(shape.size());
        this->dtype = dtype;
        this->shape = const_cast<int64_t*>(shape.data());
        Shape strides = Shape(details::MakeStridesFromShape(this->ndim, this->shape));
        this->strides = const_cast<int64_t*>(strides.data());
        this->byte_offset = 0;
        this->shape_data_ = std::move(shape);
        this->stride_data_ = std::move(strides);
        alloc_.AllocData(static_cast<DLTensor*>(this), std::forward<ExtraArgs>(extra_args)...);
    }

    ~NDArrayObjFromNDAlloc() {
        alloc_.FreeData(static_cast<DLTensor*>(this));
    }

private:
    TNDAlloc alloc_;
};

/*! \brief helper class to import from DLPack legacy DLManagedTensor */
template<typename TDLPackManagedTensor>
class TensorObjFromNDAlloc : public TensorObj {
public:
    explicit TensorObjFromNDAlloc(TDLPackManagedTensor* tensor) : tensor_(tensor) {
        *static_cast<DLTensor*>(this) = tensor_->dl_tensor;
        if (tensor_->dl_tensor.strides == nullptr) {
            Shape strides = Shape(details::MakeStridesFromShape(ndim, shape));
            this->strides = const_cast<int64_t*>(strides.data());
            this->stride_data_ = std::move(strides);
        }
    }

    ~TensorObjFromNDAlloc() {
        // run DLPack deleter if needed.
        if (tensor_->deleter != nullptr) {
            (*tensor_->deleter)(tensor_);
        }
    }

private:
    TDLPackManagedTensor* tensor_;
};
}// namespace details

/*!
 * \brief Managed NDArray.
 *  The array is backed by reference counted blocks.
 *
 * \note This class can be subclassed to implement downstream customized
 *       NDArray types that are backed by the same NDArrayObj storage type.
 */
class Tensor : public ObjectRef {
public:
    /*!
   * \brief Get the shape of the NDArray.
   * \return The shape of the NDArray.
   */
    NODISCARD Shape shape() const {
        TensorObj* obj = get_mutable();
        if (!obj->shape_data_.has_value()) {
            obj->shape_data_ = Shape(obj->shape, obj->shape + obj->ndim);
        }
        return *obj->shape_data_;
    }

    /*!
   * \brief Get the data type of the NDArray.
   * \return The data type of the NDArray.
   */
    NODISCARD DLDataType dtype() const {
        return (*this)->dtype;
    }

    /*!
   * \brief Check if the NDArray is contiguous.
   * \return True if the NDArray is contiguous, false otherwise.
   */
    NODISCARD bool IsContiguous() const {
        return ffi::IsContiguous(*get());
    }

    /*!
   * \brief Create a NDArray from a NDAllocator.
   * \param alloc The NDAllocator.
   * \param shape The shape of the NDArray.
   * \param dtype The data type of the NDArray.
   * \param device The device of the NDArray.
   * \return The created NDArray.
   * \tparam TNDAlloc The type of the NDAllocator, impelments Alloc and Free.
   * \tparam ExtraArgs Extra arguments to be passed to Alloc.
   */
    template<typename TNDAlloc, typename... ExtraArgs>
    static Tensor FromNDAlloc(TNDAlloc alloc, Shape shape, DLDataType dtype, DLDevice device,
                               ExtraArgs&&... extra_args) {
        return Tensor(make_object<details::NDArrayObjFromNDAlloc<TNDAlloc>>(
                alloc, shape, dtype, device, std::forward<ExtraArgs>(extra_args)...));
    }

    /*!
   * \brief Create a NDArray from a DLPack managed tensor, pre v1.0 API.
   * \param tensor The input DLPack managed tensor.
   * \param require_alignment The minimum alignment requored of the data + byte_offset.
   * \param require_contiguous Boolean flag indicating if we need to check for contiguity.
   * \note This function will not run any checks on flags.
   * \return The created NDArray.
   */
    static Tensor FromDLPack(DLManagedTensor* tensor, size_t require_alignment = 0,
                              bool require_contiguous = false) {
        if (require_alignment != 0 && !IsAligned(tensor->dl_tensor, require_alignment)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Data is not aligned to " << require_alignment
                                        << " bytes.";
        }
        if (require_contiguous && !ffi::IsContiguous(tensor->dl_tensor)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Tensor is not contiguous.";
        }
        return Tensor(make_object<details::TensorObjFromNDAlloc<DLManagedTensor>>(tensor));
    }

    /*!
   * \brief Create a NDArray from a DLPack managed tensor, post v1.0 API.
   * \param tensor The input DLPack managed tensor.
   * \param require_alignment The minimum alignment requored of the data + byte_offset.
   * \param require_contiguous Boolean flag indicating if we need to check for contiguity.
   * \return The created NDArray.
   */
    static Tensor FromDLPackVersioned(DLManagedTensorVersioned* tensor, size_t require_alignment = 0,
                                       bool require_contiguous = false) {
        if (require_alignment != 0 && !IsAligned(tensor->dl_tensor, require_alignment)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Data is not aligned to " << require_alignment
                                        << " bytes.";
        }
        if (require_contiguous && !ffi::IsContiguous(tensor->dl_tensor)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Tensor is not contiguous.";
        }
        if (tensor->flags & DLPACK_FLAG_BITMASK_IS_SUBBYTE_TYPE_PADDED) {
            TVM_FFI_THROW(RuntimeError) << "Subbyte type padded is not yet supported";
        }
        return Tensor(make_object<details::TensorObjFromNDAlloc<DLManagedTensorVersioned>>(tensor));
    }

    /*!
   * \brief Convert the NDArray to a DLPack managed tensor.
   * \return The converted DLPack managed tensor.
   */
    NODISCARD DLManagedTensor* ToDLPack() const {
        return get_mutable()->ToDLPack();
    }

    /*!
   * \brief Convert the NDArray to a DLPack managed tensor.
   * \return The converted DLPack managed tensor.
   */
    NODISCARD DLManagedTensorVersioned* ToDLPackVersioned() const {
        return get_mutable()->ToDLPackVersioned();
    }

    TVM_FFI_DEFINE_OBJECT_REF_METHODS(Tensor, ObjectRef, TensorObj);

protected:
    /*!
   * \brief Get mutable internal container pointer.
   * \return a mutable container pointer.
   */
    NODISCARD TensorObj* get_mutable() const {
        return const_cast<TensorObj*>(get());
    }
};

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_CONTAINER_NDARRAY_H
