/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*!
 * \file ffi/base_details.h
 * \brief Internal detail utils that can be used by files in tvm/ffi.
 * \note details header are for internal use only
 *       and not to be directly used by user.
 */
#ifndef LITETVM_FFI_BASE_DETAILS_H_
#define LITETVM_FFI_BASE_DETAILS_H_

#include "ffi/c_api.h"
#include "ffi/endian.h"
#include "ffi/macros.h"

#include <cstddef>
#include <type_traits>
#include <utility>

/*
 * \brief Define the default copy/move constructor and assign operator
 * \param TypeName The class typename.
 */
#define TVM_FFI_DEFINE_DEFAULT_COPY_MOVE_AND_ASSIGN(TypeName) \
    TypeName(const TypeName& other) = default;                \
    TypeName(TypeName&& other) = default;                     \
    TypeName& operator=(const TypeName& other) = default;     \
    TypeName& operator=(TypeName&& other) = default;

namespace litetvm {
namespace ffi {
namespace details {

// for each iterator
// struct for_each_dispatcher {
//     template<typename F, typename... Args, size_t... I>
//     static void run(std::index_sequence<I...>, const F& f, Args&&... args) {// NOLINT(*)
//         (f(I, std::forward<Args>(args)), ...);
//     }
// };

// template<typename F, typename... Args>
// void for_each(const F& f, Args&&... args) {// NOLINT(*)
//     for_each_dispatcher::run(std::index_sequence_for<Args...>{}, f, std::forward<Args>(args)...);
// }

template<typename F, typename... Args>
void for_each(const F& f, Args&&... args) {
    int i = 0;
    (f(i++, std::forward<Args>(args)), ...);
}

/*!
 * \brief hash an object and combines uint64_t key with previous keys
 *
 * This hash function is stable across platforms.
 *
 * \param key The left operand.
 * \param value The right operand.
 * \return the combined result.
 */
template<typename T, std::enable_if_t<std::is_convertible_v<T, uint64_t>, bool> = true>
TVM_FFI_INLINE uint64_t StableHashCombine(uint64_t key, const T& value) {
    // XXX: do not use std::hash in this function. This hash must be stable
    // across different platforms and std::hash is implementation dependent.
    return key ^ (static_cast<uint64_t>(value) + 0x9e3779b9 + (key << 6) + (key >> 2));
}

/*!
 * \brief Hash the binary bytes
 * \param data_ptr The data pointer
 * \param size The size of the bytes.
 * \return the hash value.
 */
TVM_FFI_INLINE uint64_t StableHashBytes(const void* data_ptr, size_t size) {
    const char* data = reinterpret_cast<const char*>(data_ptr);
    const constexpr uint64_t kMultiplier = 1099511628211ULL;
    const constexpr uint64_t kMod = 2147483647ULL;
    union Union {
        uint8_t a[8];
        uint64_t b;
    } u;
    static_assert(sizeof(Union) == sizeof(uint64_t), "sizeof(Union) != sizeof(uint64_t)");
    const char* it = data;
    const char* end = it + size;
    uint64_t result = 0;
    if constexpr (TVM_FFI_IO_NO_ENDIAN_SWAP) {
        // if alignment requirement is met, directly use load
        if (reinterpret_cast<uintptr_t>(it) % 8 == 0) {
            for (; it + 8 <= end; it += 8) {
                u.b = *reinterpret_cast<const uint64_t*>(it);
                result = (result * kMultiplier + u.b) % kMod;
            }
        } else {
            // unaligned version
            for (; it + 8 <= end; it += 8) {
                u.a[0] = it[0];
                u.a[1] = it[1];
                u.a[2] = it[2];
                u.a[3] = it[3];
                u.a[4] = it[4];
                u.a[5] = it[5];
                u.a[6] = it[6];
                u.a[7] = it[7];
                result = (result * kMultiplier + u.b) % kMod;
            }
        }
    } else {
        // need endian swap
        for (; it + 8 <= end; it += 8) {
            u.a[0] = it[7];
            u.a[1] = it[6];
            u.a[2] = it[5];
            u.a[3] = it[4];
            u.a[4] = it[3];
            u.a[5] = it[2];
            u.a[6] = it[1];
            u.a[7] = it[0];
            result = (result * kMultiplier + u.b) % kMod;
        }
    }

    if (it < end) {
        u.b = 0;
        uint8_t* a = u.a;
        if (it + 4 <= end) {
            a[0] = it[0];
            a[1] = it[1];
            a[2] = it[2];
            a[3] = it[3];
            it += 4;
            a += 4;
        }
        if (it + 2 <= end) {
            a[0] = it[0];
            a[1] = it[1];
            it += 2;
            a += 2;
        }
        if (it + 1 <= end) {
            a[0] = it[0];
            it += 1;
            a += 1;
        }
        if constexpr (!TVM_FFI_IO_NO_ENDIAN_SWAP) {
            std::swap(u.a[0], u.a[7]);
            std::swap(u.a[1], u.a[6]);
            std::swap(u.a[2], u.a[5]);
            std::swap(u.a[3], u.a[4]);
        }
        result = (result * kMultiplier + u.b) % kMod;
    }
    return result;
}

/*!
 *  \brief Same as StableHashBytes, but for small string data.
 *  \param data The data pointer
 *  \return the hash value.
 */
TVM_FFI_INLINE uint64_t StableHashSmallStrBytes(const TVMFFIAny* data) {
    if constexpr (TVM_FFI_IO_NO_ENDIAN_SWAP) {
        // fast path, no endian swap, simply hash as uint64_t
        constexpr uint64_t kMod = 2147483647ULL;
        return data->v_uint64 % kMod;
    }
    return StableHashBytes(reinterpret_cast<const void*>(data), sizeof(data->v_uint64));
}


}// namespace details
}// namespace ffi
}// namespace litetvm
#endif// LITETVM_FFI_BASE_DETAILS_H_
