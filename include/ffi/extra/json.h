//
// Created by richard on 8/5/25.
//

#ifndef LITETVM_FFI_EXTRA_JSON_H
#define LITETVM_FFI_EXTRA_JSON_H

#include "ffi/any.h"
#include "ffi/container/array.h"
#include "ffi/container/map.h"
#include "ffi/extra/base.h"

namespace litetvm {
namespace ffi {
namespace json {

/*!
 * \brief alias Any as json Value.
 *
 * To keep things lightweight, we simply reuse the ffi::Any system.
 */
using Value = Any;

/*!
 * \brief alias Map<Any, Any> as json Object.
 * \note We use Map<Any, Any> instead of Map<String, Any> to avoid
 *      the overhead of key checking when doing as conversion,
 *      the check will be performed at runtime when we read each key
 */
using Object = ffi::Map<Any, Any>;

/*! \brief alias Array<Any> as json Array. */
using Array = ffi::Array<Any>;

/*!
 * \brief Parse a JSON string into an Any value.
 *
 * Besides the standard JSON syntax, this function also supports:
 * - Infinity/NaN as javascript syntax
 * - int64 integer value
 *
 * If error_msg is not nullptr, the error message will be written to it
 * and no exception will be thrown when parsing fails.
 *
 * \param json_str The JSON string to parse.
 * \param error_msg The output error message, can be nullptr.
 *
 * \return The parsed Any value.
 */
TVM_FFI_EXTRA_CXX_API json::Value Parse(const String& json_str, String* error_msg = nullptr);

/*!
 * \brief Serialize an Any value into a JSON string.
 *
 * \param value The Any value to serialize.
 * \param indent The number of spaces to indent the output.
 *               If not specified, the output will be compact.
 * \return The output JSON string.
 */
TVM_FFI_EXTRA_CXX_API String Stringify(const json::Value& value,
                                       Optional<int> indent = std::nullopt);

}// namespace json
}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_EXTRA_JSON_H
