//
// Created by 赵丹 on 25-7-22.
//

#ifndef LITETVM_FFI_EXTRA_STRUCTURAL_EQUAL_H
#define LITETVM_FFI_EXTRA_STRUCTURAL_EQUAL_H

#include "ffi/any.h"
#include "ffi/extra/base.h"
#include "ffi/optional.h"
#include "ffi/reflection/access_path.h"

namespace litetvm {
namespace ffi {
/*
 * \brief Structural equality comparators
 */
class StructuralEqual {
public:
    /**
   * \brief Compare two Any values for structural equality.
   * \param lhs The left hand side Any object.
   * \param rhs The right hand side Any object.
   * \param map_free_vars Whether to map free variables.
   * \param skip_ndarray_content Whether to skip comparingn darray data content,
   *                             useful for cases where we don't care about parameters content
   * \return True if the two Any values are structurally equal, false otherwise.
   */
    TVM_FFI_EXTRA_CXX_API static bool Equal(const Any& lhs, const Any& rhs,
                                            bool map_free_vars = false,
                                            bool skip_ndarray_content = false);
    /**
   * \brief Get the first mismatch AccessPath pair when running
   * structural equal comparison between two Any values.
   *
   * \param lhs The left hand side Any object.
   * \param rhs The right hand side Any object.
   * \param map_free_vars Whether to map free variables.
   * \param skip_ndarray_content Whether to skip comparing ndarray data content,
   *                             useful for cases where we don't care about parameters content
   * \return If comparison fails, return the first mismatch AccessPath pair,
   *         otherwise return std::nullopt.
   */
    TVM_FFI_EXTRA_CXX_API static Optional<reflection::AccessPathPair> GetFirstMismatch(
            const Any& lhs, const Any& rhs, bool map_free_vars = false,
            bool skip_ndarray_content = false);

    /*
   * \brief Compare two Any values for structural equality.
   * \param lhs The left hand side Any object.
   * \param rhs The right hand side Any object.
   * \return True if the two Any values are structurally equal, false otherwise.
   */
    TVM_FFI_INLINE bool operator()(const Any& lhs, const Any& rhs) const {
        return Equal(lhs, rhs, false, true);
    }
};

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_EXTRA_STRUCTURAL_EQUAL_H
