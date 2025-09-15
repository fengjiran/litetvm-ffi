//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_FFI_CAST_H
#define LITETVM_FFI_CAST_H

#include "ffi/any.h"
#include "ffi/object.h"
#include "ffi/optional.h"

namespace litetvm {
namespace ffi {
/*!
 * \brief Get a reference type from a raw object ptr type
 *
 *  It is always important to get a reference type
 *  if we want to return a value as reference or keep
 *  the object alive beyond the scope of the function.
 *
 * \param ptr The object pointer
 * \tparam RefType The reference type
 * \tparam ObjectType The object type
 * \return The corresponding RefType
 */
template<typename RefType, typename ObjectType>
RefType GetRef(const ObjectType* ptr) {
    static_assert(std::is_base_of_v<typename RefType::ContainerType, ObjectType>, "Can only cast to the ref of same container type");

    if constexpr (is_optional_type_v<RefType> || RefType::_type_is_nullable) {
        if (ptr == nullptr) {
            return RefType(ObjectPtr<Object>(nullptr));
        }
    } else {
        TVM_FFI_ICHECK_NOTNULL(ptr);
    }
    return RefType(details::ObjectUnsafe::ObjectPtrFromUnowned<Object>(const_cast<Object*>(static_cast<const Object*>(ptr))));
}

/*!
 * \brief Get an object ptr type from a raw object ptr.
 *
 * \param ptr The object pointer
 * \tparam BaseType The reference type
 * \tparam ObjectType The object type
 * \return The corresponding RefType
 */
template<typename BaseType, typename ObjectType>
ObjectPtr<BaseType> GetObjectPtr(ObjectType* ptr) {
    static_assert(std::is_base_of_v<BaseType, ObjectType>, "Can only cast to the ref of same container type");
    return details::ObjectUnsafe::ObjectPtrFromUnowned<BaseType>(ptr);
}
}// namespace ffi

// Expose to the tvm namespace
// Rationale: convinience and no ambiguity
using ffi::GetObjectPtr;
using ffi::GetRef;
}// namespace litetvm

#endif//LITETVM_FFI_CAST_H
