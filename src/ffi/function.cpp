//
// Created by richard on 5/15/25.
//

#include "ffi/function.h"

#include "ffi/any.h"
#include "ffi/c_api.h"
#include "ffi/cast.h"
#include "ffi/container/array.h"
#include "ffi/container/map.h"
#include "ffi/error.h"
#include "ffi/memory.h"
#include "ffi/reflection/registry.h"
#include "ffi/string.h"
#include <utility>

namespace litetvm {
namespace ffi {

/*!
 * \brief Global function table.
 *
 * \note We do not use mutex to guard updating of GlobalFunctionTable
 *
 * The assumption is that updating of GlobalFunctionTable will be done
 * in the main thread during initialization or loading, or
 * explicitly locked from the caller.
 *
 * Then the followup code will leverage the information
 */
class GlobalFunctionTable {
public:
    // Note: this class is hidden from the public API, so we just
    // use it as a private class as ObjectRef
    class Entry : public Object, public TVMFFIMethodInfo {
    public:
        String name_data;
        String doc_data;
        String type_schema_data;
        Function func_data;

        explicit Entry(const TVMFFIMethodInfo* method_info) {
            // make a copy of the metadata
            name_data = String(method_info->name.data, method_info->name.size);
            doc_data = String(method_info->doc.data, method_info->doc.size);
            type_schema_data = String(method_info->type_schema.data, method_info->type_schema.size);
            func_data = AnyView::CopyFromTVMFFIAny(method_info->method).cast<Function>();
            SyncMethodInfo(method_info->flags);
            // no need to update method pointer as it would remain the same as func and we retained
        }

        explicit Entry(String name, Function func)
            : name_data(std::move(name)), func_data(std::move(func)) {
            SyncMethodInfo(kTVMFFIFieldFlagBitMaskIsStaticMethod);
        }

    private:
        void SyncMethodInfo(int64_t flags) {
            this->flags = flags;
            this->name = TVMFFIByteArray{name_data.data(), name_data.size()};
            this->doc = TVMFFIByteArray{doc_data.data(), doc_data.size()};
            this->type_schema = TVMFFIByteArray{type_schema_data.data(), type_schema_data.size()};
        }
    };

    void Update(const String& name, const Function& func, bool can_override) {
        if (table_.count(name)) {
            if (!can_override) {
                TVM_FFI_THROW(RuntimeError) << "Global Function `" << name << "` is already registered";
            }
        }
        table_.Set(name, ObjectRef(make_object<Entry>(name, func)));
    }

    void Update(const TVMFFIMethodInfo* method_info, bool can_override) {
        String name(method_info->name.data, method_info->name.size);
        if (table_.count(name)) {
            if (!can_override) {
                TVM_FFI_LOG_AND_THROW(RuntimeError)
                        << "Global Function `" << name << "` is already registered, possible causes:\n"
                        << "- Two GlobalDef().def registrations for the same function \n"
                        << "Please remove the duplicate registration.";
            }
        }
        table_.Set(name, ObjectRef(make_object<Entry>(method_info)));
    }

    bool Remove(const String& name) {
        auto it = table_.find(name);
        if (it == table_.end()) return false;
        table_.erase(name);
        return true;
    }

    const Entry* Get(const String& name) {
        auto it = table_.find(name);
        if (it == table_.end()) return nullptr;
        const auto* obj = (*it).second.cast<const Object*>();
        return static_cast<const Entry*>(obj);
    }

    NODISCARD Array<String> ListNames() const {
        Array<String> names;
        names.reserve(table_.size());
        for (const auto& kv: table_) {
            names.push_back(kv.first);
        }
        return names;
    }

    static GlobalFunctionTable* Global() {
        // We deliberately create a new instance via raw new
        // This is because GlobalFunctionTable can contain callbacks into
        // the host language (Python) and the resource can become invalid
        // indeterministic order of destruction and forking.
        // The resources will only be recycled during program exit.
        // static GlobalFunctionTable* inst = new GlobalFunctionTable();
        static GlobalFunctionTable inst;
        return &inst;
    }

private:
    GlobalFunctionTable() = default;

    // deliberately track the function pointer without recycling to avoid
    Map<String, Any> table_;
};

}// namespace ffi
}// namespace litetvm

int TVMFFIFunctionCreate(void* self, TVMFFISafeCallType safe_call, void (*deleter)(void* self),
                         TVMFFIObjectHandle* out) {
    TVM_FFI_SAFE_CALL_BEGIN();
    litetvm::ffi::Function func = litetvm::ffi::Function::FromExternC(self, safe_call, deleter);
    *out = litetvm::ffi::details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(func));
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFIAnyViewToOwnedAny(const TVMFFIAny* any_view, TVMFFIAny* out) {
    TVM_FFI_SAFE_CALL_BEGIN();
    litetvm::ffi::Any result(*reinterpret_cast<const litetvm::ffi::AnyView*>(any_view));
    *out = litetvm::ffi::details::AnyUnsafe::MoveAnyToTVMFFIAny(std::move(result));
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFIFunctionSetGlobal(const TVMFFIByteArray* name, TVMFFIObjectHandle f, int allow_override) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    String name_str(name->data, name->size);
    GlobalFunctionTable::Global()->Update(name_str, GetRef<Function>(static_cast<FunctionObj*>(f)), allow_override != 0);
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFIFunctionSetGlobalFromMethodInfo(const TVMFFIMethodInfo* method_info, int allow_override) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    GlobalFunctionTable::Global()->Update(method_info, allow_override != 0);
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFIFunctionGetGlobal(const TVMFFIByteArray* name, TVMFFIObjectHandle* out) {
    using namespace litetvm::ffi;
    TVM_FFI_SAFE_CALL_BEGIN();
    String name_str(name->data, name->size);
    const GlobalFunctionTable::Entry* fp = GlobalFunctionTable::Global()->Get(name_str);
    if (fp != nullptr) {
        Function func(fp->func_data);
        *out = details::ObjectUnsafe::MoveObjectRefToTVMFFIObjectPtr(std::move(func));
    } else {
        *out = nullptr;
    }
    TVM_FFI_SAFE_CALL_END();
}

int TVMFFIFunctionCall(TVMFFIObjectHandle func, TVMFFIAny* args, int32_t num_args, TVMFFIAny* result) {
    using namespace litetvm::ffi;
    // NOTE: this is a tail call
    return static_cast<FunctionObj*>(func)->safe_call(func, args, num_args, result);
}

TVM_FFI_STATIC_INIT_BLOCK({
    namespace refl = litetvm::ffi::reflection;
    refl::GlobalDef()
            .def("ffi.FunctionRemoveGlobal",
                 [](const litetvm::ffi::String& name) -> bool {
                     return litetvm::ffi::GlobalFunctionTable::Global()->Remove(name);
                 })
            .def("ffi.FunctionListGlobalNamesFunctor",
                 [] {
                     // NOTE: we return functor instead of array
                     // so list global function names do not need to depend on array
                     // this is because list global function names usually is a core api that happens
                     // before array ffi functions are available.
                     litetvm::ffi::Array<litetvm::ffi::String> names =
                             litetvm::ffi::GlobalFunctionTable::Global()->ListNames();
                     auto return_functor = [names](int64_t i) -> litetvm::ffi::Any {
                         if (i < 0) {
                             return names.size();
                         }
                         return names[i];
                     };
                     return litetvm::ffi::Function::FromTyped(return_functor);
                 })
            .def("ffi.String", [](litetvm::ffi::String val) -> litetvm::ffi::String { return val; })
            .def("ffi.Bytes", [](litetvm::ffi::Bytes val) -> litetvm::ffi::Bytes { return val; });
});
