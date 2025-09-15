//
// Created by richard on 8/2/25.
//

#ifndef LITETVM_FFI_EXTRA_BASE_H
#define LITETVM_FFI_EXTRA_BASE_H

#include "ffi/c_api.h"

/*!
 * \brief Marks the API as extra c++ api that is defined in cc files.
 *
 * They are implemented in cc files to reduce compile-time overhead.
 * The input/output only uses POD/Any/ObjectRef for ABI stability.
 * However, these extra APIs may have an issue across MSVC/Itanium ABI,
 *
 * Related features are also available through reflection based function
 * that is fully based on C API
 *
 * The project aims to minimize the number of extra C++ APIs to keep things
 * lightweight and restrict the use to non-core functionalities.
 */
#ifndef TVM_FFI_EXTRA_CXX_API
#define TVM_FFI_EXTRA_CXX_API TVM_FFI_DLL
#endif

#endif//LITETVM_FFI_EXTRA_BASE_H
