//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_FFI_MEMORY_H
#define LITETVM_FFI_MEMORY_H

#include "ffi/object.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace litetvm {
namespace ffi {

/*! \brief Deleter function for obeject */
// typedef void (*FObjectDeleter)(TVMFFIObject* obj);
using FObjectDeleter = void (*)(void* obj, int flags);

// Detail implementations after this
//
// The current design allows swapping the
// allocator pattern when necessary.
//
// Possible future allocator optimizations:
// - Arena allocator that gives ownership of memory to arena (deleter = nullptr)
// - Thread-local object pools: one pool per size and alignment requirement.
// - Can specialize by type of object to give the specific allocator to each object.
namespace details {

/*!
 * \brief Allocate aligned memory.
 * \param size The size.
 * \tparam align The alignment, must be a power of 2.
 * \return The pointer to the allocated memory.
 */
template<size_t align>
TVM_FFI_INLINE void* AlignedAlloc(size_t size) {
    static_assert(align != 0 && (align & (align - 1)) == 0, "align must be a power of 2");
#ifdef _MSC_VER
    // MSVC have to use _aligned_malloc
    if (void* ptr = _aligned_malloc(size, align)) {
        return ptr;
    }
    throw std::bad_alloc();
#else
    if constexpr (align <= alignof(std::max_align_t)) {
        // malloc guarantees alignment of std::max_align_t
        if (void* ptr = std::malloc(size)) {
            return ptr;
        }
        throw std::bad_alloc();
    } else {
        void* ptr;
        // for other alignments, use posix_memalign
        if (posix_memalign(&ptr, align, size) != 0) {
            throw std::bad_alloc();
        }
        return ptr;
    }
#endif
}

/*!
 * \brief Free aligned memory.
 * \param data The pointer to the memory to free.
 */
TVM_FFI_INLINE void AlignedFree(void* data) {
#ifdef _MSC_VER
    // MSVC have to use _aligned_free
    _aligned_free(data);
#else
    std::free(data);
#endif
}


/*!
 * \brief Base class of object allocators that implements make.
 *  Use curiously recurring template pattern (CRTP).
 *
 * \tparam Derived The derived class.
 */
template<typename Derived>
class ObjAllocatorBase {
public:
    /*!
   * \brief Make a new object using the allocator.
   * \tparam T The type to be allocated.
   * \tparam Args The constructor signature.
   * \param args The arguments.
   */
    template<typename T, typename... Args>
    ObjectPtr<T> make_object(Args&&... args) {
        using Handler = Derived::template Handler<T>;
        static_assert(std::is_base_of_v<Object, T>, "make can only be used to create Object");
        T* ptr = Handler::New(static_cast<Derived*>(this), std::forward<Args>(args)...);
        TVMFFIObject* ffi_ptr = ObjectUnsafe::GetHeader(ptr);
        ffi_ptr->combined_ref_count = kCombinedRefCountBothOne;
        ffi_ptr->type_index = T::RuntimeTypeIndex();
        ffi_ptr->__padding = 0;
        ffi_ptr->deleter = Handler::Deleter();
        return ObjectUnsafe::ObjectPtrFromOwned<T>(ptr);
    }

    /*!
   * \tparam ArrayType The type to be allocated.
   * \tparam ElemType The type of array element.
   * \tparam Args The constructor signature.
   * \param num_elems The number of array elements.
   * \param args The arguments.
   */
    template<typename ArrayType, typename ElemType, typename... Args>
    ObjectPtr<ArrayType> make_inplace_array(size_t num_elems, Args&&... args) {
        using Handler = Derived::template ArrayHandler<ArrayType, ElemType>;
        static_assert(std::is_base_of_v<Object, ArrayType>, "make_inplace_array can only be used to create Object");
        ArrayType* ptr = Handler::New(static_cast<Derived*>(this), num_elems, std::forward<Args>(args)...);
        TVMFFIObject* ffi_ptr = ObjectUnsafe::GetHeader(ptr);
        ffi_ptr->combined_ref_count = kCombinedRefCountBothOne;
        ffi_ptr->type_index = ArrayType::RuntimeTypeIndex();
        ffi_ptr->__padding = 0;
        ffi_ptr->deleter = Handler::Deleter();
        return ObjectUnsafe::ObjectPtrFromOwned<ArrayType>(ptr);
    }
};

// Simple allocator that uses new/delete.
class SimpleObjAllocator : public ObjAllocatorBase<SimpleObjAllocator> {
public:
    template<typename T>
    class Handler {
    public:
        template<typename... Args>
        static T* New(SimpleObjAllocator*, Args&&... args) {
            // NOTE: the first argument is not needed for SimpleObjAllocator
            // It is reserved for special allocators that needs to recycle
            // the object to itself (e.g. in the case of object pool).
            //
            // In the case of an object pool, an allocator needs to create
            // a special chunk memory that hides reference to the allocator
            // and call allocator's release function in the deleter.

            // NOTE2: Use inplace new to allocate
            // This is used to get rid of warning when deleting a virtual
            // class with non-virtual destructor.
            // We are fine here as we captured the right deleter during construction.
            // This is also the right way to get storage type for an object pool.
            void* data = AlignedAlloc<alignof(T)>(sizeof(T));
            new (data) T(std::forward<Args>(args)...);
            return reinterpret_cast<T*>(data);
        }

        static FObjectDeleter Deleter() {
            return Deleter_;
        }

    private:
        static void Deleter_(void* objptr, int flags) {
            T* tptr = ObjectUnsafe::RawObjectPtrFromUnowned<T>(static_cast<TVMFFIObject*>(objptr));

            if (flags & kTVMFFIObjectDeleterFlagBitMaskStrong) {
                // It is important to do tptr->T::~T(),
                // so that we explicitly call the specific destructor
                // instead of tptr->~T(), which could mean the intention
                // call a virtual destructor(which may not be available and is not required).
                tptr->T::~T();
            }
            if (flags & kTVMFFIObjectDeleterFlagBitMaskWeak) {
                AlignedFree(static_cast<void*>(tptr));
            }
        }
    };

    // Array handler that uses new/delete.
    template<typename ArrayType, typename ElemType>
    class ArrayHandler {
    public:
        template<typename... Args>
        static ArrayType* New(SimpleObjAllocator*, size_t num_elems, Args&&... args) {
            // NOTE: the first argument is not needed for ArrayObjAllocator
            // It is reserved for special allocators that needs to recycle
            // the object to itself (e.g. in the case of object pool).
            //
            // In the case of an object pool, an allocator needs to create
            // a special chunk memory that hides reference to the allocator
            // and call allocator's release function in the deleter.
            // NOTE2: Use inplace new to allocate
            // This is used to get rid of warning when deleting a virtual
            // class with non-virtual destructor.
            // We are fine here as we captured the right deleter during construction.
            // This is also the right way to get storage type for an object pool.
            // for now only support elements that aligns with array header.
            static_assert(
                    alignof(ArrayType) % alignof(ElemType) == 0 && sizeof(ArrayType) % alignof(ElemType) == 0,
                    "element alignment constraint");
            size_t size = sizeof(ArrayType) + sizeof(ElemType) * num_elems;
            // round up to the nearest multiple of align
            constexpr size_t align = alignof(ArrayType);
            // C++ standard always guarantees that alignof operator returns a power of 2
            size_t aligned_size = (size + (align - 1)) & ~(align - 1);
            void* data = AlignedAlloc<align>(aligned_size);
            new (data) ArrayType(std::forward<Args>(args)...);
            return reinterpret_cast<ArrayType*>(data);
        }

        static FObjectDeleter Deleter() {
            return Deleter_;
        }

    private:
        static void Deleter_(void* objptr, int flags) {
            ArrayType* tptr = ObjectUnsafe::RawObjectPtrFromUnowned<ArrayType>(static_cast<TVMFFIObject*>(objptr));
            if (flags & kTVMFFIObjectDeleterFlagBitMaskStrong) {
                // It is important to do tptr->ArrayType::~ArrayType(),
                // so that we explicitly call the specific destructor
                // instead of tptr->~ArrayType(), which could mean the intention
                // call a virtual destructor(which may not be available and is not required).
                tptr->ArrayType::~ArrayType();
            }
            if (flags & kTVMFFIObjectDeleterFlagBitMaskWeak) {
                AlignedFree(static_cast<void*>(tptr));
            }
        }
    };
};

}// namespace details


template<typename T, typename... Args>
ObjectPtr<T> make_object(Args&&... args) {
    return details::SimpleObjAllocator().make_object<T>(std::forward<Args>(args)...);
}

template<typename ArrayType, typename ElemType, typename... Args>
ObjectPtr<ArrayType> make_inplace_array_object(size_t num_elems, Args&&... args) {
    return details::SimpleObjAllocator().make_inplace_array<ArrayType, ElemType>(num_elems, std::forward<Args>(args)...);
}

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_MEMORY_H
