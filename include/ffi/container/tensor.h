//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_FFI_CONTAINER_NDARRAY_H
#define LITETVM_FFI_CONTAINER_NDARRAY_H

#include <ffi/container/shape.h>
#include <ffi/dtype.h>
#include <ffi/error.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace litetvm {
namespace ffi {

/*!
 * \brief Check if the device uses direct address, where address of data indicate alignment.
 * \param device The input device.
 * \return True if the device uses direct address, false otherwise.
 */
inline bool IsDirectAddressDevice(const DLDevice& device) {
    return device.device_type <= kDLCUDAHost || device.device_type == kDLCUDAManaged ||
           device.device_type == kDLROCM || device.device_type == kDLROCMHost;
}

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
    if (IsDirectAddressDevice(arr.device)) {
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
    TVM_FFI_DECLARE_OBJECT_INFO_STATIC(StaticTypeKey::kTVMFFITensor, TensorObj, Object);

    /*!
   * \brief Move NDArray to a DLPack managed tensor.
   * \return The converted DLPack managed tensor.
   */
    NODISCARD DLManagedTensor* ToDLPack() const {
        TensorObj* self = const_cast<TensorObj*>(this);
        DLManagedTensor* ret = new DLManagedTensor();
        ret->dl_tensor = *static_cast<DLTensor*>(self);
        ret->manager_ctx = self;
        ret->deleter = DLManagedTensorDeleter<DLManagedTensor>;
        details::ObjectUnsafe::IncRefObjectHandle(self);
        return ret;
    }

    /*!
   * \brief Move  NDArray to a DLPack managed tensor.
   * \return The converted DLPack managed tensor.
   */
    NODISCARD DLManagedTensorVersioned* ToDLPackVersioned() const {
        TensorObj* self = const_cast<TensorObj*>(this);
        DLManagedTensorVersioned* ret = new DLManagedTensorVersioned();
        ret->version.major = DLPACK_MAJOR_VERSION;
        ret->version.minor = DLPACK_MINOR_VERSION;
        ret->dl_tensor = *static_cast<DLTensor*>(self);
        ret->manager_ctx = self;
        ret->deleter = DLManagedTensorDeleter<DLManagedTensorVersioned>;
        details::ObjectUnsafe::IncRefObjectHandle(self);
        return ret;
    }

protected:
    template<typename TDLManagedTensor>
    static void DLManagedTensorDeleter(TDLManagedTensor* tensor) {
        TensorObj* obj = static_cast<TensorObj*>(tensor->manager_ctx);
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
class TensorObjFromNDAlloc : public TensorObj {
public:
    using Self = TensorObjFromNDAlloc;
    template<typename... ExtraArgs>
    TensorObjFromNDAlloc(TNDAlloc alloc, ShapeView shape, DLDataType dtype, DLDevice device,
                         ExtraArgs&&... extra_args) : alloc_(alloc) {
        this->device = device;
        this->ndim = static_cast<int>(shape.size());
        this->dtype = dtype;
        this->byte_offset = 0;
        // inplace alloc shape and strides after data structure
        this->shape = reinterpret_cast<int64_t*>(reinterpret_cast<char*>(this) + sizeof(Self));
        this->strides = this->shape + shape.size();
        std::copy(shape.begin(), shape.end(), this->shape);
        details::FillStridesFromShape(shape, this->strides);
        // call allocator to alloc data
        alloc_.AllocData(static_cast<DLTensor*>(this), std::forward<ExtraArgs>(extra_args)...);
    }

    ~TensorObjFromNDAlloc() {
        alloc_.FreeData(static_cast<DLTensor*>(this));
    }

private:
    TNDAlloc alloc_;
};

/*! \brief helper class to import from DLPack legacy DLManagedTensor */
template<typename TDLPackManagedTensor>
class TensorObjFromDLPack : public TensorObj {
public:
    using Self = TensorObjFromDLPack;

    explicit TensorObjFromDLPack(TDLPackManagedTensor* tensor, bool extra_strides_at_tail)
        : tensor_(tensor) {
        *static_cast<DLTensor*>(this) = tensor_->dl_tensor;
        if (extra_strides_at_tail) {
            this->strides = reinterpret_cast<int64_t*>(reinterpret_cast<char*>(this) + sizeof(Self));
            details::FillStridesFromShape(ShapeView(tensor_->dl_tensor.shape, tensor_->dl_tensor.ndim),
                                          this->strides);
        }
    }

    ~TensorObjFromDLPack() {
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
    ShapeView shape() const {
        const TensorObj* obj = get();
        return ShapeView(obj->shape, obj->ndim);
    }

    ShapeView strides() const {
        const TensorObj* obj = get();
        TVM_FFI_ICHECK(obj->strides != nullptr);
        return ShapeView(obj->strides, obj->ndim);
    }

    /*!
   * \brief Get the data pointer of the Tensor.
   * \return The data pointer of the Tensor.
   */
    void* data_ptr() const { return (*this)->data; }

    /*!
     * \brief Get the number of dimensions in the Tensor.
     * \return The number of dimensions in the Tensor.
     */
    int32_t ndim() const { return (*this)->ndim; }

    /*!
     * \brief Get the number of elements in the Tensor.
     * \return The number of elements in the Tensor.
     */
    int64_t numel() const { return this->shape().Product(); }

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
   * \brief Check if the Tensor data is aligned to the given alignment.
   * \param alignment The alignment to check.
   * \return True if the Tensor data is aligned to the given alignment, false otherwise.
   */
    NODISCARD bool IsAligned(size_t alignment) const { return ffi::IsAligned(*get(), alignment); }

    /*!
   * \brief Create a NDArray from a NDAllocator.
   * \param alloc The NDAllocator.
   * \param shape The shape of the NDArray.
   * \param dtype The data type of the NDArray.
   * \param device The device of the NDArray.
   * \param extra_args Extra arguments to be forwarded to TNDAlloc.
   * \return The created NDArray.
   * \tparam TNDAlloc The type of the NDAllocator, impelments Alloc and Free.
   * \tparam ExtraArgs Extra arguments to be passed to Alloc.
   */
    template<typename TNDAlloc, typename... ExtraArgs>
    static Tensor FromNDAlloc(TNDAlloc alloc, ShapeView shape, DLDataType dtype, DLDevice device,
                              ExtraArgs&&... extra_args) {
        // inplace alloc shape and strides after data structure (as a result why multiply 2)
        size_t num_extra_i64_at_tail = shape.size() * 2;
        return Tensor(make_inplace_array_object<details::TensorObjFromNDAlloc<TNDAlloc>, int64_t>(
                num_extra_i64_at_tail, alloc, shape, dtype, device,
                std::forward<ExtraArgs>(extra_args)...));
    }

    /*!
   * \brief Create a Tensor from a DLPackTensorAllocator
   *
   * This function can be used together with TVMFFIEnvSetTensorAllocator
   * in the extra/c_env_api.h to create Tensor from the thread-local
   * environment allocator.
   *
   * \code
   *
   * ffi::Tensor tensor = ffi::Tensor::FromDLPackAlloc(
   *   TVMFFIEnvGetTensorAllocator(), shape, dtype, device
   * );
   * \endcode
   *
   * \param allocator The DLPack allocator.
   * \param shape The shape of the Tensor.
   * \param dtype The data type of the Tensor.
   * \param device The device of the Tensor.
   * \return The created Tensor.
   */
    static Tensor FromDLPackAlloc(DLPackTensorAllocator allocator, ffi::Shape shape, DLDataType dtype,
                                  DLDevice device) {
        if (allocator == nullptr) {
            TVM_FFI_THROW(RuntimeError)
                    << "FromDLPackAlloc: allocator is nullptr, "
                    << "likely because TVMFFIEnvSetTensorAllocator has not been called.";
        }
        DLTensor prototype;
        prototype.device = device;
        prototype.dtype = dtype;
        prototype.shape = const_cast<int64_t*>(shape.data());
        prototype.ndim = static_cast<int>(shape.size());
        prototype.strides = nullptr;
        prototype.byte_offset = 0;
        prototype.data = nullptr;
        DLManagedTensorVersioned* tensor = nullptr;
        // error context to be used to propagate error
        struct ErrorContext {
            std::string kind;
            std::string message;
            static void SetError(void* error_ctx, const char* kind, const char* message) {
                ErrorContext* error_context = static_cast<ErrorContext*>(error_ctx);
                error_context->kind = kind;
                error_context->message = message;
            }
        };
        ErrorContext error_context;
        int ret = (*allocator)(&prototype, &tensor, &error_context, ErrorContext::SetError);
        if (ret != 0) {
            throw Error(error_context.kind, error_context.message,
                        TVMFFITraceback(__FILE__, __LINE__, __func__));
        }
        if (tensor->dl_tensor.strides != nullptr) {
            return Tensor(make_object<details::TensorObjFromDLPack<DLManagedTensorVersioned>>(
                    tensor, /*extra_strides_at_tail=*/false));
        }
        return Tensor(
                make_inplace_array_object<details::TensorObjFromDLPack<DLManagedTensorVersioned>,
                                          int64_t>(tensor->dl_tensor.ndim, tensor,
                                                   /*extra_strides_at_tail=*/true));
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
        if (require_alignment != 0 && !ffi::IsAligned(tensor->dl_tensor, require_alignment)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Data is not aligned to " << require_alignment
                                        << " bytes.";
        }
        if (require_contiguous && !ffi::IsContiguous(tensor->dl_tensor)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Tensor is not contiguous.";
        }
        if (tensor->dl_tensor.strides != nullptr) {
            return Tensor(make_object<details::TensorObjFromDLPack<DLManagedTensor>>(
                    tensor, /*extra_strides_at_tail=*/false));
        }
        return Tensor(
                make_inplace_array_object<details::TensorObjFromDLPack<DLManagedTensor>, int64_t>(
                        tensor->dl_tensor.ndim, tensor, /*extra_strides_at_tail=*/true));
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
        if (require_alignment != 0 && !ffi::IsAligned(tensor->dl_tensor, require_alignment)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Data is not aligned to " << require_alignment
                                        << " bytes.";
        }
        if (require_contiguous && !ffi::IsContiguous(tensor->dl_tensor)) {
            TVM_FFI_THROW(RuntimeError) << "FromDLPack: Tensor is not contiguous.";
        }
        if (tensor->flags & DLPACK_FLAG_BITMASK_IS_SUBBYTE_TYPE_PADDED) {
            TVM_FFI_THROW(RuntimeError) << "Subbyte type padded is not yet supported";
        }
        if (tensor->dl_tensor.strides != nullptr) {
            return Tensor(make_object<details::TensorObjFromDLPack<DLManagedTensorVersioned>>(
                    tensor, /*extra_strides_at_tail=*/false));
        }
        return Tensor(
                make_inplace_array_object<details::TensorObjFromDLPack<DLManagedTensorVersioned>,
                                          int64_t>(tensor->dl_tensor.ndim, tensor,
                                                   /*extra_strides_at_tail=*/true));
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

    TVM_FFI_DEFINE_OBJECT_REF_METHODS_NULLABLE(Tensor, ObjectRef, TensorObj);

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
