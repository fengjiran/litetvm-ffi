//
// Created by richard on 5/15/25.
//

#ifndef LITETVM_CONTAINER_SHAPE_H
#define LITETVM_CONTAINER_SHAPE_H

#include "ffi/container/array.h"
#include "ffi/error.h"
#include "ffi/type_traits.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace litetvm {
namespace ffi {

/*! \brief An object representing a shape tuple. */
class ShapeObj : public Object, public TVMFFIShapeCell {
public:
    using index_type = int64_t;

    /*! \brief Get "numel", meaning the number of elements of an array if the array has this shape */
    NODISCARD int64_t Product() const {
        int64_t product = 1;
        for (size_t i = 0; i < this->size; ++i) {
            product *= this->data[i];
        }
        return product;
    }

    static constexpr uint32_t _type_index = kTVMFFIShape;
    static constexpr const char* _type_key = StaticTypeKey::kTVMFFIShape;
    TVM_FFI_DECLARE_STATIC_OBJECT_INFO(ShapeObj, Object);
};

namespace details {

class ShapeObjStdImpl : public ShapeObj {
public:
    explicit ShapeObjStdImpl(std::vector<int64_t> other) : data_{std::move(other)} {
        this->data = data_.data();
        this->size = data_.size();
    }

private:
    std::vector<int64_t> data_;
};

TVM_FFI_INLINE ObjectPtr<ShapeObj> MakeEmptyShape(size_t length, int64_t** mutable_data) {
    ObjectPtr<ShapeObj> p = make_inplace_array_object<ShapeObj, int64_t>(length);
    static_assert(alignof(ShapeObj) % alignof(int64_t) == 0);
    static_assert(sizeof(ShapeObj) % alignof(int64_t) == 0);
    auto* data = reinterpret_cast<int64_t*>(reinterpret_cast<char*>(p.get()) + sizeof(ShapeObj));
    if (mutable_data) {
        *mutable_data = data;
    }
    p->data = data;
    p->size = length;
    return p;
}

// inplace shape allocation
template<typename Iter>
TVM_FFI_INLINE ObjectPtr<ShapeObj> MakeInplaceShape(Iter begin, Iter end) {
    size_t length = std::distance(begin, end);
    int64_t* mutable_data;
    ObjectPtr<ShapeObj> p = MakeEmptyShape(length, &mutable_data);
    std::copy(begin, end, mutable_data);
    return p;
}

TVM_FFI_INLINE ObjectPtr<ShapeObj> MakeStridesFromShape(int64_t ndim, int64_t* shape) {
    int64_t* strides_data;
    ObjectPtr<ShapeObj> strides = details::MakeEmptyShape(ndim, &strides_data);
    int64_t stride = 1;
    for (int i = ndim - 1; i >= 0; --i) {
        strides_data[i] = stride;
        stride *= shape[i];
    }
    return strides;
}

}// namespace details

/*!
 * \brief Reference to shape object.
 */
class Shape : public ObjectRef {
public:
    /*! \brief The type of shape index element. */
    using index_type = ShapeObj::index_type;

    /*! \brief Default constructor */
    Shape() : ObjectRef(details::MakeEmptyShape(0, nullptr)) {}

    /*!
   * \brief Constructor from iterator
   * \param begin begin of iterator
   * \param end end of iterator
   * \tparam IterType The type of iterator
   */
    template<typename IterType>
    Shape(IterType begin, IterType end) : Shape(details::MakeInplaceShape(begin, end)) {}

    /**
   * \brief Constructor from Array<int64_t>
   * \param shape The Array<int64_t>
   *
   * \note This constructor will copy the data content.
   */
    Shape(const Array<int64_t>& shape) : Shape(shape.begin(), shape.end()) {}// NOLINT

    /*!
   * \brief constructor from initializer list
   * \param shape The initializer list
   */
    Shape(std::initializer_list<int64_t> shape) : Shape(shape.begin(), shape.end()) {}

    /*!
   * \brief constructor from int64_t [N]
   *
   * \param other a int64_t array.
   */
    Shape(std::vector<int64_t> other)// NOLINT(*)
        : ObjectRef(make_object<details::ShapeObjStdImpl>(std::move(other))) {}

    /*!
   * \brief Return the data pointer
   *
   * \return const index_type* data pointer
   */
    NODISCARD const int64_t* data() const {
        return get()->data;
    }

    /*!
   * \brief Return the size of the shape tuple
   *
   * \return size_t shape tuple size
   */
    NODISCARD size_t size() const {
        return get()->size;
    }

    /*!
   * \brief Immutably read i-th element from the shape tuple.
   * \param idx The index
   * \return the i-th element.
   */
    int64_t operator[](size_t idx) const {
        if (idx >= this->size()) {
            TVM_FFI_THROW(IndexError) << "indexing " << idx << " on a Shape of size " << this->size();
        }
        return this->data()[idx];
    }

    /*!
   * \brief Immutably read i-th element from the shape tuple.
   * \param idx The index
   * \return the i-th element.
   */
    NODISCARD int64_t at(size_t idx) const {
        return this->operator[](idx);
    }

    /*! \return Whether shape tuple is empty */
    NODISCARD bool empty() const {
        return size() == 0;
    }

    /*! \return The first element of the shape tuple */
    NODISCARD int64_t front() const {
        return this->at(0);
    }

    /*! \return The last element of the shape tuple */
    NODISCARD int64_t back() const {
        return this->at(this->size() - 1);
    }

    /*! \return begin iterator */
    NODISCARD const int64_t* begin() const {
        return get()->data;
    }

    /*! \return end iterator */
    NODISCARD const int64_t* end() const {
        return get()->data + size();
    }

    /*! \return The product of the shape tuple */
    NODISCARD int64_t Product() const {
        return get()->Product();
    }

    TVM_FFI_DEFINE_NOTNULLABLE_OBJECT_REF_METHODS(Shape, ObjectRef, ShapeObj);
};

inline std::ostream& operator<<(std::ostream& os, const Shape& shape) {
    os << '[';
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i != 0) {
            os << ", ";
        }
        os << shape[i];
    }
    os << ']';
    return os;
}

// Shape
template<>
inline constexpr bool use_default_type_traits_v<Shape> = false;

// Allow auto conversion from Array<int64_t> to Shape, but not from Shape to Array<int64_t>
template<>
struct TypeTraits<Shape> : ObjectRefWithFallbackTraitsBase<Shape, Array<int64_t>> {
    static constexpr int32_t field_static_type_index = kTVMFFIShape;
    TVM_FFI_INLINE static Shape ConvertFallbackValue(const Array<int64_t>& src) {
        return {src};
    }
};

}// namespace ffi
}// namespace litetvm

#endif//LITETVM_CONTAINER_SHAPE_H
