//
// Created by 赵丹 on 25-7-22.
//

#ifndef LITETVM_FFI_EXTRA_STRUCTURAL_HASH_H
#define LITETVM_FFI_EXTRA_STRUCTURAL_HASH_H

#include "ffi/any.h"
#include "ffi/extra/base.h"

namespace litetvm {
namespace ffi {

/*
 * \brief Structural hash
 */
class StructuralHash {
public:
    /*!
     * \brief Hash an Any value.
     * \param value The Any value to hash.
     * \param map_free_vars Whether to map free variables.
     * \param skip_ndarray_content Whether to skip comparingn darray data content,
     *                             useful for cases where we don't care about parameters content.
     * \return The hash value.
     */
    TVM_FFI_EXTRA_CXX_API static uint64_t Hash(const Any& value, bool map_free_vars = false,
                                               bool skip_ndarray_content = false);
    /*!
     * \brief Hash an Any value.
     * \param value The Any value to hash.
     * \return The hash value.
     */
    TVM_FFI_INLINE uint64_t operator()(const Any& value) const { return Hash(value); }
};
}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_EXTRA_STRUCTURAL_HASH_H
