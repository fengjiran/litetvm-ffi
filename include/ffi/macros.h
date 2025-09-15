//
// Created by 赵丹 on 25-7-22.
//

#ifndef LITETVM_FFI_MACROS_H
#define LITETVM_FFI_MACROS_H

#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif
#endif

/*!
 * \brief Macro helper for exception throwing.
 */
#define TVM_THROW_EXCEPTION noexcept(false)

#if defined(_MSC_VER)
#define TVM_FFI_INLINE [[msvc::forceinline]] inline
#else
#define TVM_FFI_INLINE [[gnu::always_inline]] inline
#endif

/*!
 * \brief Macro helper to force a function to be inlined.
 * It is only used in places that we know inline is important,
 * e.g. some template expansion cases.
 */
#ifdef _MSC_VER
#define TVM_ALWAYS_INLINE __forceinline
#else
#define TVM_ALWAYS_INLINE inline __attribute__((always_inline))
#endif

/*!
 * \brief Macro helper to force a function not to be inlined.
 * It is only used in places that we know not inlining is good,
 * e.g. some logging functions.
 */
#if defined(_MSC_VER)
#define TVM_NO_INLINE __declspec(noinline)
#else
#define TVM_NO_INLINE __attribute__((noinline))
#endif


/*!
 * \brief Macro helper to force a function not to be inlined.
 * It is only used in places that we know not inlining is good,
 * e.g. some logging functions.
 */
#if defined(_MSC_VER)
#define TVM_FFI_NO_INLINE [[msvc::noinline]]
#else
#define TVM_FFI_NO_INLINE [[gnu::noinline]]
#endif

#if defined(_MSC_VER)
#define TVM_FFI_UNREACHABLE() __assume(false)
#else
#define TVM_FFI_UNREACHABLE() __builtin_unreachable()
#endif

// Macros to do weak linking
#ifdef _MSC_VER
#define TVM_FFI_WEAK __declspec(selectany)
#else
#define TVM_FFI_WEAK __attribute__((weak))
#endif

#if !defined(TVM_FFI_DLL) && defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define TVM_FFI_DLL EMSCRIPTEN_KEEPALIVE
#define TVM_FFI_DLL_EXPORT EMSCRIPTEN_KEEPALIVE
#endif
#if !defined(TVM_FFI_DLL) && defined(_MSC_VER)
#ifdef TVM_FFI_EXPORTS
#define TVM_FFI_DLL __declspec(dllexport)
#else
#define TVM_FFI_DLL __declspec(dllimport)
#endif
#define TVM_FFI_DLL_EXPORT __declspec(dllexport)
#endif
#ifndef TVM_FFI_DLL
#define TVM_FFI_DLL __attribute__((visibility("default")))
#define TVM_FFI_DLL_EXPORT __attribute__((visibility("default")))
#endif


/*! \brief helper macro to suppress unused warning */
#define TVM_FFI_ATTRIBUTE_UNUSED [[maybe_unused]]

#define TVM_FFI_STR_CONCAT_(__x, __y) __x##__y
#define TVM_FFI_STR_CONCAT(__x, __y) TVM_FFI_STR_CONCAT_(__x, __y)

#if defined(__GNUC__) || defined(__clang__)
#define TVM_FFI_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define TVM_FFI_FUNC_SIG __FUNCSIG__
#else
#define TVM_FFI_FUNC_SIG __func__
#endif

#define TVM_FFI_STATIC_INIT_BLOCK_VAR_DEF \
    TVM_FFI_ATTRIBUTE_UNUSED static inline int __##TVMFFIStaticInitReg

/*! \brief helper macro to run code once during initialization */
#define TVM_FFI_STATIC_INIT_BLOCK(Body) \
    TVM_FFI_STR_CONCAT(TVM_FFI_STATIC_INIT_BLOCK_VAR_DEF, __COUNTER__) = []() { Body return 0; }()


#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define NODISCARD [[nodiscard]]
#else
#define NODISCARD
#endif

#if __has_cpp_attribute(maybe_unused)
#define MAYBE_UNUSED [[maybe_unused]]
#else
#define MAYBE_UNUSED
#endif
#endif

#define UNUSED(expr)   \
    do {               \
        (void) (expr); \
    } while (false)

/**
 * \brief marks the begining of a C call that logs exception
 */
#define TVM_FFI_LOG_EXCEPTION_CALL_BEGIN() \
    try {                                  \
    (void) 0

/*!
 * \brief Marks the end of a C call that logs exception
 */
#define TVM_FFI_LOG_EXCEPTION_CALL_END(Name)                      \
    }                                                             \
    catch (const std::exception& err) {                           \
        std::cerr << "Exception caught during " << #Name << ":\n" \
                  << err.what() << std::endl;                     \
        exit(-1);                                                 \
    }

/*!
 * \brief Clear the padding parts so we can safely use v_int64 for hash
 *        and equality check even when the value stored is a pointer.
 *
 * This macro is used to clear the padding parts for hash and equality check
 * in 32bit platform.
 */
#define TVM_FFI_CLEAR_PTR_PADDING_IN_FFI_ANY(result)                      \
    if constexpr (sizeof((result)->v_obj) != sizeof((result)->v_int64)) { \
        (result)->v_int64 = 0;                                            \
    }


#endif//LITETVM_FFI_MACROS_H
