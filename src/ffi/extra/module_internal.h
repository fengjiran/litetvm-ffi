//
// Created by 赵丹 on 2025/8/19.
//

#ifndef LITETVM_FFI_EXTRA_MODULE_INTERNAL_H
#define LITETVM_FFI_EXTRA_MODULE_INTERNAL_H

#include "ffi/extra/module.h"
#include "ffi/reflection/registry.h"

#include <mutex>

namespace litetvm {
namespace ffi {

/*!
 * \brief Library is the common interface
 *  for storing data in the form of shared libaries.
 *
 * \sa src/ffi/extra/dso_library.cc
 * \sa src/ffi/extra/system_library.cc
 */
class Library : public Object {
public:
    // destructor.
    virtual ~Library() {}
    /*!
   * \brief Get the symbol address for a given name.
   * \param name The name of the symbol.
   * \return The symbol.
   */
    virtual void* GetSymbol(const String& name) = 0;
    /*!
   * \brief Get the symbol address for a given name with the tvm ffi symbol prefix.
   * \param name The name of the symbol.
   * \return The symbol.
   * \note This function will be overloaded by systemlib implementation.
   */
    virtual void* GetSymbolWithSymbolPrefix(const String& name) {
        String name_with_prefix = symbol::tvm_ffi_symbol_prefix + name;
        return GetSymbol(name_with_prefix);
    }

    // NOTE: we do not explicitly create an type index and type_key here for libary.
    // This is because we do not need dynamic type downcasting and only need to use the refcounting
};

struct ModuleObj::InternalUnsafe {
    static Array<Any>* GetImports(ModuleObj* module) { return &(module->imports_); }

    static void* GetFunctionFromImports(ModuleObj* module, const char* name) {
        // backend implementation for TVMFFIEnvModLookupFromImports
        static std::mutex mutex_;
        std::lock_guard<std::mutex> lock(mutex_);
        String s_name(name);
        auto it = module->import_lookup_cache_.find(s_name);
        if (it != module->import_lookup_cache_.end()) {
            return const_cast<FunctionObj*>((*it).second.operator->());
        }

        auto opt_func = [&]() -> std::optional<Function> {
            for (const Any& import: module->imports_) {
                if (auto opt_func = import.cast<Module>()->GetFunction(s_name, true)) {
                    return *opt_func;
                }
            }
            // try global at last
            return litetvm::ffi::Function::GetGlobal(s_name);
        }();
        if (!opt_func.has_value()) {
            TVM_FFI_THROW(RuntimeError) << "Cannot find function " << name
                                        << " in the imported modules or global registry.";
        }
        module->import_lookup_cache_.Set(s_name, *opt_func);
        return const_cast<FunctionObj*>((*opt_func).operator->());
    }

    static void RegisterReflection() {
        namespace refl = litetvm::ffi::reflection;
        refl::ObjectDef<ModuleObj>().def_ro("imports_", &ModuleObj::imports_);
    }
};

/*!
 * \brief Create a library module from a given library.
 *
 * \param lib The library.
 *
 * \return The corresponding loaded module.
 */
Module CreateLibraryModule(ObjectPtr<Library> lib);

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_EXTRA_MODULE_INTERNAL_H
