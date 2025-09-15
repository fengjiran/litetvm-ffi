//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_FFI_TRACEBACK_H
#define LITETVM_FFI_TRACEBACK_H

#include "macros.h"

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace litetvm {
namespace ffi {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)// std::getenv is unsafe
#endif

inline int32_t GetTracebackLimit() {
    if (const char* env = std::getenv("TVM_TRACEBACK_LIMIT")) {
        return std::stoi(env);
    }
    return 512;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/*!
 * \brief List frame patterns that should be excluded as they contain less information
 */
inline bool ShouldExcludeFrame(const char* filename, const char* symbol) {
    if (filename) {
        // Stack frames for TVM FFI
        if (strstr(filename, "include/tvm/ffi/error.h")) {
            return true;
        }
        if (strstr(filename, "include/tvm/ffi/function_details.h")) {
            return true;
        }
        if (strstr(filename, "include/tvm/ffi/function.h")) {
            return true;
        }
        if (strstr(filename, "include/tvm/ffi/any.h")) {
            return true;
        }
        if (strstr(filename, "include/tvm/runtime/logging.h")) {
            return true;
        }
        if (strstr(filename, "src/ffi/traceback.cc")) {
            return true;
        }
        // C++ stdlib frames
        if (strstr(filename, "include/c++/")) {
            return true;
        }
    }

    if (symbol) {
        // C++ stdlib frames
        if (strstr(symbol, "__libc_")) {
            return true;
        }
    }
    if (strncmp(symbol, "TVMFFIErrorSetRaisedFromCStr", 28) == 0) {
        return true;
    }
    // libffi.so stack frames.  These may also show up as numeric
    // addresses with no symbol name.  This could be improved in the
    // future by using dladdr() to check whether an address is contained
    // in libffi.so
    if (strstr(symbol, "ffi_call_")) {
        return true;
    }
    return false;
}

/**
 * \brief List frames that should stop the traceback.
 * \param filename The filename of the frame.
 * \param symbol The symbol name of the frame.
 * \return true if the frame should stop the traceback.
 * \note We stop traceback at the FFI boundary.
 */
inline bool ShouldStopTraceback(const char* filename, const char* symbol) {
    if (symbol != nullptr) {
        if (strncmp(symbol, "TVMFFIFunctionCall", 14) == 0) {
            return true;
        }
        // Python interpreter stack frames
        // we stop traceback at the Python interpreter stack frames
        // since these frame will be handled from by the python side.
        if (strncmp(symbol, "_Py", 3) == 0 || strncmp(symbol, "PyObject", 9) == 0) {
            return true;
        }
    }
    return false;
}

/*!
 * \brief storage to store traceback
 */
struct TracebackStorage {
    std::vector<std::string> lines;
    /*! \brief Maximum size of the traceback. */
    size_t max_frame_size = GetTracebackLimit();

    void Append(const char* filename, const char* func, int lineno) {
        // skip frames with empty filename
        if (filename == nullptr) {
            if (func != nullptr) {
                if (strncmp(func, "0x0", 3) == 0) {
                    return;
                }
                filename = "<unknown>";
            } else {
                return;
            }
        }
        std::ostringstream trackeback_stream;
        trackeback_stream << "  File \"" << filename << "\"";
        if (lineno != 0) {
            trackeback_stream << ", line " << lineno;
        }
        trackeback_stream << ", in " << func << '\n';
        lines.push_back(trackeback_stream.str());
    }

    NODISCARD bool ExceedTracebackLimit() const {
        return lines.size() >= max_frame_size;
    }

    // get traceback in the order of most recent call last
    NODISCARD std::string GetTraceback() const {
        std::string traceback;
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            // traceback.insert(traceback.end(), it->begin(), it->end());
            traceback += *it;
        }
        return traceback;
    }
};

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_TRACEBACK_H
