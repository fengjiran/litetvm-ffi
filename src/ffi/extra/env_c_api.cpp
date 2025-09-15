//
// Created by 赵丹 on 2025/8/28.
//
#include "ffi/extra/c_env_api.h"
#include "ffi/function.h"


namespace litetvm {
namespace ffi {
/*!
 * \brief Execution environment specific API registry.
 *
 *  This registry stores C API function pointers about
 *  execution environment(e.g. python) specific API function that
 *  we need for specific low-level handling(e.g. signal checking).
 *
 *  We only stores the C API function when absolutely necessary (e.g. when signal handler
 *  cannot trap back into python). Always consider use the Function FFI when possible
 *  in other cases.
 */
class EnvCAPIRegistry {
public:
    /*!
   * \brief Callback to check if signals have been sent to the process and
   *        if so invoke the registered signal handler in the frontend environment.
   *
   *  When running FFI in another language (Python), the signal handler
   *  may not be immediately executed, but instead the signal is marked
   *  in the interpreter state (to ensure non-blocking of the signal handler).
   *
   * \return 0 if no error happens, -1 if error happens.
   */
    typedef int (*F_PyErr_CheckSignals)();

    /*! \brief Callback to increment/decrement the python ref count */
    typedef void (*F_Py_IncDefRef)(void*);

    /*!
   * \brief PyErr_CheckSignal function
   */
    F_PyErr_CheckSignals pyerr_check_signals = nullptr;

    /*!
    \brief PyGILState_Ensure function
   */
    void* (*py_gil_state_ensure)() = nullptr;

    /*!
    \brief PyGILState_Release function
   */
    void (*py_gil_state_release)(void*) = nullptr;

    static EnvCAPIRegistry* Global() {
        static EnvCAPIRegistry* inst = new EnvCAPIRegistry();
        return inst;
    }

    // register environment(e.g. python) specific api functions
    void Register(const String& symbol_name, void* fptr) {
        if (symbol_name == "PyErr_CheckSignals") {
            Update(symbol_name, &pyerr_check_signals, fptr);
        } else if (symbol_name == "PyGILState_Ensure") {
            Update(symbol_name, &py_gil_state_ensure, fptr);
        } else if (symbol_name == "PyGILState_Release") {
            Update(symbol_name, &py_gil_state_release, fptr);
        } else {
            TVM_FFI_THROW(ValueError) << "Unknown env API " + symbol_name;
        }
    }

    int EnvCheckSignals() {
        // check python signal to see if there are exception raised
        if (pyerr_check_signals != nullptr) {
            // The C++ env comes without gil, so we need to grab gil here
            WithGIL context(this);
            if ((*pyerr_check_signals)() != 0) {
                // The error will let FFI know that the frontend environment
                // already set an error.
                return -1;
            }
        }
        return 0;
    }

private:
    // update the internal API table
    template<typename FType>
    void Update(const String& symbol_name, FType* target, void* ptr) {
        FType ptr_casted = reinterpret_cast<FType>(ptr);
        target[0] = ptr_casted;
    }

    struct WithGIL {
        explicit WithGIL(EnvCAPIRegistry* self) : self(self) {
            TVM_FFI_ICHECK(self->py_gil_state_ensure);
            TVM_FFI_ICHECK(self->py_gil_state_release);
            gil_state = self->py_gil_state_ensure();
        }
        ~WithGIL() {
            if (self && gil_state) {
                self->py_gil_state_release(gil_state);
            }
        }
        WithGIL(const WithGIL&) = delete;
        WithGIL(WithGIL&&) = delete;
        WithGIL& operator=(const WithGIL&) = delete;
        WithGIL& operator=(WithGIL&&) = delete;

        EnvCAPIRegistry* self = nullptr;
        void* gil_state = nullptr;
    };
};
}// namespace ffi
}// namespace litetvm

int TVMFFIEnvCheckSignals() { return litetvm::ffi::EnvCAPIRegistry::Global()->EnvCheckSignals(); }

/*!
 * \brief Register a symbol into the from the surrounding env.
 * \param name The name of the symbol.
 * \param symbol The symbol to register.
 * \return 0 when success, nonzero when failure happens
 */
int TVMFFIEnvRegisterCAPI(const char* name, void* symbol) {
    TVM_FFI_SAFE_CALL_BEGIN();
    litetvm::ffi::String s_name(name);
    litetvm::ffi::EnvCAPIRegistry::Global()->Register(s_name, symbol);
    TVM_FFI_SAFE_CALL_END();
}
