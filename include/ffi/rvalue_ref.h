//
// Created by 赵丹 on 25-5-16.
//

#ifndef LITETVM_FFI_RVALUE_REF_H
#define LITETVM_FFI_RVALUE_REF_H

#include "ffi/object.h"
#include "ffi/type_traits.h"

#include <string>
#include <utility>

namespace litetvm {
namespace ffi {

/*!
 * \brief Helper class to define rvalue reference type.
 *
 * By default,  FFI pass all values by lvalue reference.
 *
 * However, we do allow users to intentionally mark a function parameter
 * as RValueRef<T>. In such cases, the caller can choose to pass parameter
 * wrapped by RValueRef<T> to the function. In which case the parameter
 * can be directly moved by the callee. The caller can also choose to pass
 * a normal lvalue to the function, in such case a copy will be triggered.
 *
 * To keep FFI checking overhead minimal, we do not handle case when rvalue
 * is passed, but the callee did not declare the parameter as RValueRef<T>.
 *
 * This design allows us to still leverage move semantics for parameters that
 * need copy on write scenarios (and requires an unique copy).
 *
 * \code
 *
 * void Example() {
 *   auto append = Function::FromTyped([](RValueRef<Array<int>> ref, int val) -> Array<int> {
 *     Array<int> arr = *std::move(ref);
 *     assert(arr.unique());
 *     arr.push_back(val);
 *     return arr;
 *   });
 *   Array<int> a = Array<int>({1, 2});
 *   // as we use rvalue ref to move a into append
 *   // we keep a single copy of the Array without creating new copies during copy-on-write
 *   a = append(RvalueRef(std::move(a)), 3);
 *   assert(a.size() == 3);
 * }
 *
 * \endcode
 */
template<typename TObjRef, typename = std::enable_if_t<std::is_base_of_v<ObjectRef, TObjRef>>>
class RValueRef {
public:
    /*! \brief only allow move constructor from rvalue of T */
    explicit RValueRef(TObjRef&& data)
        : data_(details::ObjectUnsafe::ObjectPtrFromObjectRef<Object>(std::move(data))) {}

    /*! \brief return the data as rvalue */
    TObjRef operator*() && {
        return TObjRef(std::move(data_));
    }

    /*! \return The use count of the ptr, for debug purposes */
    NODISCARD int use_count() const {
        return data_.use_count();
    }

private:
    mutable ObjectPtr<Object> data_;

    template<typename, typename>
    friend struct TypeTraits;
};

template<typename TObjRef>
inline constexpr bool use_default_type_traits_v<RValueRef<TObjRef>> = false;

template<typename TObjRef>
struct TypeTraits<RValueRef<TObjRef>> : TypeTraitsBase {
    static constexpr bool storage_enabled = false;

    TVM_FFI_INLINE static void CopyToAnyView(const RValueRef<TObjRef>& src, TVMFFIAny* result) {
        result->type_index = kTVMFFIObjectRValueRef;
        result->zero_padding = 0;
        // store the address of the ObjectPtr, which allows us to move the value
        // and set the original ObjectPtr to nullptr
        result->v_ptr = &src.data_;
    }

    TVM_FFI_INLINE static std::string GetMismatchTypeInfo(const TVMFFIAny* src) {
        if (src->type_index == kTVMFFIObjectRValueRef) {
            auto* rvalue_ref = static_cast<ObjectPtr<Object>*>(src->v_ptr);
            // object type does not match up, we need to try to convert the object
            // in this case we do not move the original rvalue ref since conversion creates a copy
            TVMFFIAny tmp_any;
            tmp_any.type_index = rvalue_ref->get()->type_index();
            tmp_any.zero_padding = 0;
            tmp_any.v_obj = reinterpret_cast<TVMFFIObject*>(rvalue_ref->get());
            return "RValueRef<" + TypeTraits<TObjRef>::GetMismatchTypeInfo(&tmp_any) + ">";
        }
        return TypeTraits<TObjRef>::GetMismatchTypeInfo(src);
    }

    TVM_FFI_INLINE static std::optional<RValueRef<TObjRef>> TryCastFromAnyView(const TVMFFIAny* src) {
        // first try rvalue conversion
        if (src->type_index == TypeIndex::kTVMFFIObjectRValueRef) {
            auto* rvalue_ref = static_cast<ObjectPtr<Object>*>(src->v_ptr);
            TVMFFIAny tmp_any;
            tmp_any.type_index = rvalue_ref->get()->type_index();
            tmp_any.zero_padding = 0;
            tmp_any.v_obj = reinterpret_cast<TVMFFIObject*>(rvalue_ref->get());
            // fast path, storage type matches, direct move the rvalue ref
            if (TypeTraits<TObjRef>::CheckAnyStrict(&tmp_any)) {
                return RValueRef<TObjRef>(TObjRef(std::move(*rvalue_ref)));
            }
            if (std::optional<TObjRef> opt = TypeTraits<TObjRef>::TryCastFromAnyView(&tmp_any)) {
                // object type does not match up, we need to try to convert the object
                // in this case we do not move the original rvalue ref since conversion creates a copy
                return RValueRef<TObjRef>(*std::move(opt));
            }
            return std::nullopt;
        }
        // try lvalue conversion
        if (std::optional<TObjRef> opt = TypeTraits<TObjRef>::TryCastFromAnyView(src)) {
            return RValueRef<TObjRef>(*std::move(opt));
        }
        return std::nullopt;
    }

    TVM_FFI_INLINE static std::string TypeStr() {
        return "RValueRef<" + TypeTraits<TObjRef>::TypeStr() + ">";
    }
};
}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_RVALUE_REF_H
