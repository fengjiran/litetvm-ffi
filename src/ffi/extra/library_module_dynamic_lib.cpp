//
// Created by 赵丹 on 2025/8/19.
//
#include "ffi/memory.h"
#include "ffi/reflection/registry.h"
#include "ffi/string.h"

#include "module_internal.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(__hexagon__)
extern "C" {
#include <HAP_farf.h>
}
#endif

namespace litetvm {
namespace ffi {

class DSOLibrary final : public Library {
public:
    explicit DSOLibrary(const String& name) { Load(name); }
    ~DSOLibrary() {
        if (lib_handle_) Unload();
    }

    void* GetSymbol(const String& name) final { return GetSymbol_(name.c_str()); }

private:
    // private system dependent implementation
    void* GetSymbol_(const char* name);
    void Load(const String& name);
    void Unload();

#if defined(_WIN32)
    //! \brief Windows library handle
    HMODULE lib_handle_{nullptr};
#else
    // \brief Linux library handle
    void* lib_handle_{nullptr};
#endif
};

#if defined(_WIN32)

void* DSOLibrary::GetSymbol_(const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(lib_handle_, (LPCSTR) name));// NOLINT(*)
}

void DSOLibrary::Load(const String& name) {
    // use wstring version that is needed by LLVM.
    std::wstring wname(name.data(), name.data() + name.size());
    lib_handle_ = LoadLibraryW(wname.c_str());
    TVM_FFI_ICHECK(lib_handle_ != nullptr) << "Failed to load dynamic shared library " << name;
}

void DSOLibrary::Unload() {
    FreeLibrary(lib_handle_);
    lib_handle_ = nullptr;
}

#else

void DSOLibrary::Load(const String& name) {
    lib_handle_ = dlopen(name.c_str(), RTLD_LAZY | RTLD_LOCAL);
    TVM_FFI_ICHECK(lib_handle_ != nullptr)
            << "Failed to load dynamic shared library " << name << " " << dlerror();
#if defined(__hexagon__)
    int p;
    int rc = dlinfo(lib_handle_, RTLD_DI_LOAD_ADDR, &p);
    if (rc)
        FARF(ERROR, "error getting model .so start address : %u", rc);
    else
        FARF(ALWAYS, "Model .so Start Address : %x", p);
#endif
}

void* DSOLibrary::GetSymbol_(const char* name) { return dlsym(lib_handle_, name); }

void DSOLibrary::Unload() {
    dlclose(lib_handle_);
    lib_handle_ = nullptr;
}
#endif

TVM_FFI_STATIC_INIT_BLOCK({
    namespace refl = litetvm::ffi::reflection;
    refl::GlobalDef().def("ffi.Module.load_from_file.so", [](String library_path, String) {
        return CreateLibraryModule(make_object<DSOLibrary>(library_path));
    });
});
}// namespace ffi
}// namespace litetvm