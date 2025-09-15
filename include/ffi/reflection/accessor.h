//
// Created by richard on 7/16/25.
//

/*!
 * \file ffi/reflection/accessor.h
 * \brief Reflection-based accessor for object fields and methods.
 */
#ifndef LITETVM_FFI_REFLECTION_ACCESSOR_H
#define LITETVM_FFI_REFLECTION_ACCESSOR_H

#include "ffi/any.h"
#include "ffi/c_api.h"
#include "ffi/function.h"

#include <string>
#include <utility>

namespace litetvm {
namespace ffi {
namespace reflection {

/*!
 * \brief helper function to get reflection field info by type key and field name
 */
inline const TVMFFIFieldInfo* GetFieldInfo(std::string_view type_key, const char* field_name) {
    int32_t type_index;
    TVMFFIByteArray type_key_array = {type_key.data(), type_key.size()};
    TVM_FFI_CHECK_SAFE_CALL(TVMFFITypeKeyToIndex(&type_key_array, &type_index));
    const TypeInfo* info = TVMFFIGetTypeInfo(type_index);
    for (int32_t i = 0; i < info->num_fields; ++i) {
        if (std::strncmp(info->fields[i].name.data, field_name, info->fields[i].name.size) == 0) {
            return &(info->fields[i]);
        }
    }
    TVM_FFI_THROW(RuntimeError) << "Cannot find field `" << field_name << "` in " << type_key;
    TVM_FFI_UNREACHABLE();
}

/*!
 * \brief helper wrapper class to obtain a getter.
 */
class FieldGetter {
public:
    explicit FieldGetter(const TVMFFIFieldInfo* field_info) : field_info_(field_info) {}

    explicit FieldGetter(std::string_view type_key, const char* field_name)
        : FieldGetter(GetFieldInfo(type_key, field_name)) {}

    Any operator()(const Object* obj_ptr) const {
        Any result;
        const void* addr = reinterpret_cast<const char*>(obj_ptr) + field_info_->offset;
        TVM_FFI_CHECK_SAFE_CALL(
                field_info_->getter(const_cast<void*>(addr), reinterpret_cast<TVMFFIAny*>(&result)));
        return result;
    }

    Any operator()(const ObjectPtr<Object>& obj_ptr) const { return operator()(obj_ptr.get()); }

    Any operator()(const ObjectRef& obj) const { return operator()(obj.get()); }

private:
    const TVMFFIFieldInfo* field_info_;
};

/*!
 * \brief helper wrapper class to obtain a setter.
 */
class FieldSetter {
public:
    explicit FieldSetter(const TVMFFIFieldInfo* field_info) : field_info_(field_info) {}

    explicit FieldSetter(std::string_view type_key, const char* field_name)
        : FieldSetter(GetFieldInfo(type_key, field_name)) {}

    void operator()(const Object* obj_ptr, AnyView value) const {
        const void* addr = reinterpret_cast<const char*>(obj_ptr) + field_info_->offset;
        TVM_FFI_CHECK_SAFE_CALL(
                field_info_->setter(const_cast<void*>(addr), reinterpret_cast<const TVMFFIAny*>(&value)));
    }

    void operator()(const ObjectPtr<Object>& obj_ptr, AnyView value) const {
        operator()(obj_ptr.get(), value);
    }

    void operator()(const ObjectRef& obj, AnyView value) const { operator()(obj.get(), value); }

private:
    const TVMFFIFieldInfo* field_info_;
};

class TypeAttrColumn {
public:
    explicit TypeAttrColumn(std::string_view attr_name) {
        TVMFFIByteArray attr_name_array = {attr_name.data(), attr_name.size()};
        column_ = TVMFFIGetTypeAttrColumn(&attr_name_array);
        if (column_ == nullptr) {
            TVM_FFI_THROW(RuntimeError) << "Cannot find type attribute " << attr_name;
        }
    }

    AnyView operator[](int32_t type_index) const {
        size_t tindex = static_cast<size_t>(type_index);
        if (tindex >= column_->size) {
            return AnyView();
        }
        const AnyView* any_view_data = reinterpret_cast<const AnyView*>(column_->data);
        return any_view_data[tindex];
    }

private:
    const TVMFFITypeAttrColumn* column_;
};

/*!
 * \brief helper function to get reflection method info by type key and method name
 *
 * \param type_key The type key.
 * \param method_name The name of the method.
 * \return The method info.
 */
inline const TVMFFIMethodInfo* GetMethodInfo(std::string_view type_key, const char* method_name) {
    int32_t type_index;
    TVMFFIByteArray type_key_array = {type_key.data(), type_key.size()};
    TVM_FFI_CHECK_SAFE_CALL(TVMFFITypeKeyToIndex(&type_key_array, &type_index));
    const TypeInfo* info = TVMFFIGetTypeInfo(type_index);
    for (int32_t i = 0; i < info->num_methods; ++i) {
        if (std::strncmp(info->methods[i].name.data, method_name, info->methods[i].name.size) == 0) {
            return &(info->methods[i]);
        }
    }
    TVM_FFI_THROW(RuntimeError) << "Cannot find method " << method_name << " in " << type_key;
    TVM_FFI_UNREACHABLE();
}

/*!
 * \brief helper function to get reflection method function by method info
 *
 * \param type_key The type key.
 * \param method_name The name of the method.
 * \return The method function.
 */
inline Function GetMethod(std::string_view type_key, const char* method_name) {
    const TVMFFIMethodInfo* info = GetMethodInfo(type_key, method_name);
    return AnyView::CopyFromTVMFFIAny(info->method).cast<Function>();
}

/*!
 * \brief Visit each field info of the type info and run callback.
 *
 * \tparam Callback The callback function type.
 *
 * \param type_info The type info.
 * \param callback The callback function.
 *
 * \note This function calls both the child and parent type info.
 */
template<typename Callback>
void ForEachFieldInfo(const TypeInfo* type_info, Callback callback) {
    using ResultType = decltype(callback(type_info->fields));
    static_assert(std::is_same_v<ResultType, void>, "Callback must return void");
    // iterate through acenstors in parent to child order
    // skip the first one since it is always the root object
    for (int i = 1; i < type_info->type_depth; ++i) {
        const TVMFFITypeInfo* parent_info = type_info->type_acenstors[i];
        for (int j = 0; j < parent_info->num_fields; ++j) {
            callback(parent_info->fields + j);
        }
    }
    for (int i = 0; i < type_info->num_fields; ++i) {
        callback(type_info->fields + i);
    }
}

/*!
 * \brief Visit each field info of the type info and run callback which returns bool for early stop.
 *
 * \tparam Callback The callback function type, which returns bool for early stop.
 *
 * \param type_info The type info.
 * \param callback_with_early_stop The callback function.
 * \return true if any of early stop is triggered.
 *
 * \note This function calls both the child and parent type info and can be used for searching.
 */
template<typename Callback>
bool ForEachFieldInfoWithEarlyStop(const TypeInfo* type_info, Callback callback_with_early_stop) {
    // iterate through acenstors in parent to child order
    // skip the first one since it is always the root object
    for (int i = 1; i < type_info->type_depth; ++i) {
        const auto* parent_info = type_info->type_acenstors[i];
        for (int j = 0; j < parent_info->num_fields; ++j) {
            if (callback_with_early_stop(parent_info->fields + j)) {
                return true;
            }
        }
    }

    for (int i = 0; i < type_info->num_fields; ++i) {
        if (callback_with_early_stop(type_info->fields + i)) {
            return true;
        }
    }
    return false;
}

}// namespace reflection
}// namespace ffi
}// namespace litetvm


#endif//LITETVM_FFI_REFLECTION_ACCESSOR_H
