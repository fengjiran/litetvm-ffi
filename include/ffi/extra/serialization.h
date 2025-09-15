//
// Created by 赵丹 on 2025/8/8.
//

#ifndef LITETVM_FFI_EXTRA_SERIALIZATION_H
#define LITETVM_FFI_EXTRA_SERIALIZATION_H

#include "ffi/extra/base.h"
#include "ffi/extra/json.h"

namespace litetvm {
namespace ffi {

/**
 * \brief Serialize ffi::Any to a JSON that stores the object graph.
 *
 * The JSON graph structure is stored as follows:
 *
 * ```json
 * {
 *   "root_index": <int>,        // Index of root node in nodes array
 *   "nodes": [<node>, ...],     // Array of serialized nodes
 *   "metadata": <object>        // Optional metadata
 * }
 * ```
 *
 * Each node has the format: `{"type": "<type_key>", "data": <type_data>}`
 * For object types and strings, the data may contain indices to other nodes.
 * For object fields whose static type is known as a primitive type, it is stored directly,
 * otherwise, it is stored as a reference to the nodes array by an index.
 *
 * This function preserves the type and multiple references to the same object,
 * which is useful for debugging and serialization.
 *
 * \param value The ffi::Any value to serialize.
 * \param metadata Extra metadata attached to "metadata" field of the JSON object.
 * \return The serialized JSON value.
 */
TVM_FFI_EXTRA_CXX_API json::Value ToJSONGraph(const Any& value, const Any& metadata = Any(nullptr));

/**
 * \brief Deserialize a JSON that stores the object graph to an ffi::Any value.
 *
 * This function can be used to implement deserialization
 * and debugging.
 *
 * \param value The JSON value to deserialize.
 * \return The deserialized object graph.
 */
TVM_FFI_EXTRA_CXX_API Any FromJSONGraph(const json::Value& value);

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_FFI_EXTRA_SERIALIZATION_H
