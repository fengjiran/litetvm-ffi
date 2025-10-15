//
// Created by richard on 5/15/25.
//
#include "ffi/container/array.h"
#include "ffi/container/map.h"
#include "ffi/container/shape.h"
#include "ffi/function.h"
#include "ffi/reflection/registry.h"

namespace litetvm {
namespace ffi {

// Favor struct outside function scope as MSVC may have bug for in fn scope struct.
class MapForwardIterFunctor {
public:
    MapForwardIterFunctor(MapObj::iterator iter, MapObj::iterator end)
        : iter_(iter), end_(end) {}
    // 0 get current key
    // 1 get current value
    // 2 move to next: return true if success, false if end
    Any operator()(int command) const {
        if (command == 0) {
            return (*iter_).first;
        }

        if (command == 1) {
            return (*iter_).second;
        }

        ++iter_;
        if (iter_ == end_) {
            return false;
        }
        return true;
    }

private:
    mutable MapObj::iterator iter_;
    MapObj::iterator end_;
};

TVM_FFI_STATIC_INIT_BLOCK() {
    namespace refl = reflection;
    refl::GlobalDef()
            .def_packed("ffi.Array",
                        [](PackedArgs args, Any* ret) {
                            *ret = Array<Any>(args.data(), args.data() + args.size());
                        })
            .def("ffi.ArrayGetItem", [](const ArrayObj* n, int64_t i) -> Any { return n->at(i); })
            .def("ffi.ArraySize",
                 [](const ArrayObj* n) -> int64_t { return static_cast<int64_t>(n->size()); })
            .def_packed("ffi.Map",
                        [](PackedArgs args, Any* ret) {
                            TVM_FFI_ICHECK_EQ(args.size() % 2, 0);
                            Map<Any, Any> data;
                            for (int i = 0; i < args.size(); i += 2) {
                                data.Set(args[i], args[i + 1]);
                            }
                            *ret = data;
                        })
            .def("ffi.MapSize",
                 [](const MapObj* n) -> int64_t { return static_cast<int64_t>(n->size()); })
            .def("ffi.MapGetItem", [](const MapObj* n, const Any& k) -> Any { return n->at(k); })
            .def("ffi.MapCount",
                 [](const MapObj* n, const Any& k) -> int64_t {
                     return static_cast<int64_t>(n->count(k));
                 })
            .def("ffi.MapForwardIterFunctor", [](const MapObj* n) -> Function {
                return ffi::Function::FromTyped(MapForwardIterFunctor(n->begin(), n->end()));
            });
}

}// namespace ffi
}// namespace litetvm