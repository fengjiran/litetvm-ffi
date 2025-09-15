//
// Created by 赵丹 on 2025/9/8.
//

#include "ffi/any.h"
#include "ffi/container/array.h"
#include "ffi/container/map.h"
#include "ffi/container/tensor.h"
#include "ffi/container/tuple.h"
#include "ffi/container/variant.h"
#include "ffi/function.h"
#include "ffi/reflection/registry.h"

#include <gtest/gtest.h>

// test-cases used in example code
namespace {

void ExampleAny() {
  namespace ffi = litetvm::ffi;
  // Create an Any from various types
  ffi::Any int_value = 42;
  ffi::Any float_value = 3.14;
  ffi::Any string_value = "hello world";

  // AnyView provides a lightweight view without ownership
  ffi::AnyView view = int_value;
  // we can cast Any/AnyView to a specific type
  int extracted = view.cast<int>();
  EXPECT_EQ(extracted, 42);

  // If we are not sure about the type
  // we can use as to get an optional value
  std::optional<int> maybe_int = view.as<int>();
  if (maybe_int.has_value()) {
    EXPECT_EQ(maybe_int.value(), 42);
  }
  // Try cast is another version that will try to run the type
  // conversion even if the type does not exactly match
  std::optional<int> maybe_int_try = view.try_cast<int>();
  if (maybe_int_try.has_value()) {
    EXPECT_EQ(maybe_int_try.value(), 42);
  }
}

TEST(Example, Any) { ExampleAny(); }

void ExampleFunctionFromPacked() {
  namespace ffi = litetvm::ffi;
  // Create a function from a typed lambda
  ffi::Function fadd1 =
      ffi::Function::FromPacked([](const ffi::AnyView* args, int32_t num_args, ffi::Any* rv) {
        TVM_FFI_ICHECK_EQ(num_args, 1);
        int a = args[0].cast<int>();
        *rv = a + 1;
      });
  int b = fadd1(1).cast<int>();
  EXPECT_EQ(b, 2);
}

void ExampleFunctionFromTyped() {
  namespace ffi = litetvm::ffi;
  // Create a function from a typed lambda
  ffi::Function fadd1 = ffi::Function::FromTyped([](const int a) -> int { return a + 1; });
  int b = fadd1(1).cast<int>();
  EXPECT_EQ(b, 2);
}

void ExampleFunctionPassFunction() {
  namespace ffi = litetvm::ffi;
  // Create a function from a typed lambda
  ffi::Function fapply = ffi::Function::FromTyped(
      [](const ffi::Function f, ffi::Any param) { return f(param.cast<int>()); });
  ffi::Function fadd1 = ffi::Function::FromTyped(  //
      [](const int a) -> int { return a + 1; });
  int b = fapply(fadd1, 2).cast<int>();
  EXPECT_EQ(b, 3);
}

void ExamplegGlobalFunctionRegistry() {
  namespace ffi = litetvm::ffi;
  ffi::reflection::GlobalDef().def("xyz.add1", [](const int a) -> int { return a + 1; });
  ffi::Function fadd1 = ffi::Function::GetGlobalRequired("xyz.add1");
  int b = fadd1(1).cast<int>();
  EXPECT_EQ(b, 2);
}

void FuncThrowError() {
  namespace ffi = litetvm::ffi;
  TVM_FFI_THROW(TypeError) << "test0";
}

void ExampleErrorHandling() {
  namespace ffi = litetvm::ffi;
  try {
    FuncThrowError();
  } catch (const ffi::Error& e) {
    EXPECT_EQ(e.kind(), "TypeError");
    EXPECT_EQ(e.message(), "test0");
    std::cout << e.traceback() << std::endl;
  }
}

TEST(Example, Function) {
  ExampleFunctionFromPacked();
  ExampleFunctionFromTyped();
  ExampleFunctionPassFunction();
  ExamplegGlobalFunctionRegistry();
  ExampleErrorHandling();
}

struct CPUNDAlloc {
  void AllocData(DLTensor* tensor) { tensor->data = malloc(litetvm::ffi::GetDataSize(*tensor)); }
  void FreeData(DLTensor* tensor) { free(tensor->data); }
};

void ExampleTensor() {
  namespace ffi = litetvm::ffi;
  ffi::Shape shape = {1, 2, 3};
  DLDataType dtype = {kDLFloat, 32, 1};
  DLDevice device = {kDLCPU, 0};
  ffi::Tensor tensor = ffi::Tensor::FromNDAlloc(CPUNDAlloc(), shape, dtype, device);
}

void ExampleTensorDLPack() {
  namespace ffi = litetvm::ffi;
  ffi::Shape shape = {1, 2, 3};
  DLDataType dtype = {kDLFloat, 32, 1};
  DLDevice device = {kDLCPU, 0};
  ffi::Tensor tensor = ffi::Tensor::FromNDAlloc(CPUNDAlloc(), shape, dtype, device);
  // convert to DLManagedTensorVersioned
  DLManagedTensorVersioned* dlpack = tensor.ToDLPackVersioned();
  // load back from DLManagedTensorVersioned
  ffi::Tensor tensor2 = ffi::Tensor::FromDLPackVersioned(dlpack);
}

TEST(Example, Tensor) {
  ExampleTensor();
  ExampleTensorDLPack();
}

void ExampleString() {
  namespace ffi = litetvm::ffi;
  ffi::String str = "hello world";
  EXPECT_EQ(str.size(), 11);
  std::string std_str = str;
  EXPECT_EQ(std_str, "hello world");
}

TEST(Example, String) { ExampleString(); }

void ExampleArray() {
  namespace ffi = litetvm::ffi;
  ffi::Array<int> numbers = {1, 2, 3};
  EXPECT_EQ(numbers.size(), 3);
  EXPECT_EQ(numbers[0], 1);

  ffi::Function head = ffi::Function::FromTyped([](const ffi::Array<int> a) { return a[0]; });
  EXPECT_EQ(head(numbers).cast<int>(), 1);

  try {
    // throw an error because 2.2 is not int
    head(ffi::Array<ffi::Any>({1, 2.2}));
  } catch (const ffi::Error& e) {
    EXPECT_EQ(e.kind(), "TypeError");
  }
}

void ExampleTuple() {
  namespace ffi = litetvm::ffi;
  ffi::Tuple<int, ffi::String, bool> tup(42, "hello", true);

  EXPECT_EQ(tup.get<0>(), 42);
  EXPECT_EQ(tup.get<1>(), "hello");
  EXPECT_EQ(tup.get<2>(), true);
}

TEST(Example, Array) {
  ExampleArray();
  ExampleTuple();
}

void ExampleMap() {
  namespace ffi = litetvm::ffi;

  ffi::Map<ffi::String, int> map0 = {{"Alice", 100}, {"Bob", 95}};

  EXPECT_EQ(map0.size(), 2);
  EXPECT_EQ(map0.at("Alice"), 100);
  EXPECT_EQ(map0.count("Alice"), 1);
}

TEST(Example, Map) { ExampleMap(); }

void ExampleOptional() {
  namespace ffi = litetvm::ffi;
  ffi::Optional<int> opt0 = 100;
  EXPECT_EQ(opt0.has_value(), true);
  EXPECT_EQ(opt0.value(), 100);

  ffi::Optional<ffi::String> opt1;
  EXPECT_EQ(opt1.has_value(), false);
  EXPECT_EQ(opt1.value_or("default"), "default");
}

TEST(Example, Optional) { ExampleOptional(); }

void ExampleVariant() {
  namespace ffi = litetvm::ffi;
  ffi::Variant<int, ffi::String> var0 = 100;
  EXPECT_EQ(var0.get<int>(), 100);

  var0 = ffi::String("hello");
  std::optional<ffi::String> maybe_str = var0.as<ffi::String>();
  EXPECT_EQ(maybe_str.value(), "hello");

  std::optional<int> maybe_int2 = var0.as<int>();
  EXPECT_EQ(maybe_int2.has_value(), false);
}

TEST(Example, Variant) { ExampleVariant(); }

// Step 1: Define the object class (stores the actual data)
class MyIntPairObj : public litetvm::ffi::Object {
 public:
  int64_t a;
  int64_t b;

  MyIntPairObj() = default;
  MyIntPairObj(int64_t a, int64_t b) : a(a), b(b) {}

  // Required: declare type information
  static constexpr const char* _type_key = "example.MyIntPair";
  TVM_FFI_DECLARE_FINAL_OBJECT_INFO(MyIntPairObj, litetvm::ffi::Object);
};

// Step 2: Define the reference wrapper (user-facing interface)
class MyIntPair : public litetvm::ffi::ObjectRef {
 public:
  // Constructor
  explicit MyIntPair(int64_t a, int64_t b) { data_ = litetvm::ffi::make_object<MyIntPairObj>(a, b); }

  // Required: define object reference methods
  TVM_FFI_DEFINE_OBJECT_REF_METHODS(MyIntPair, litetvm::ffi::ObjectRef, MyIntPairObj);
};

void ExampleObjectPtr() {
  namespace ffi = litetvm::ffi;
  ffi::ObjectPtr<MyIntPairObj> obj = ffi::make_object<MyIntPairObj>(100, 200);
  EXPECT_EQ(obj->a, 100);
  EXPECT_EQ(obj->b, 200);
}

void ExampleObjectRef() {
  namespace ffi = litetvm::ffi;
  MyIntPair pair(100, 200);
  EXPECT_EQ(pair->a, 100);
  EXPECT_EQ(pair->b, 200);
}

void ExampleObjectRefAny() {
  namespace ffi = litetvm::ffi;
  MyIntPair pair(100, 200);
  ffi::Any any = pair;
  MyIntPair pair2 = any.cast<MyIntPair>();
  EXPECT_EQ(pair2->a, 100);
  EXPECT_EQ(pair2->b, 200);
}

TEST(Example, ObjectPtr) {
  ExampleObjectPtr();
  ExampleObjectRef();
  ExampleObjectRefAny();
}

}  // namespace