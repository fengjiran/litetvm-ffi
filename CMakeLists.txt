# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

cmake_minimum_required(VERSION 3.18)

project(litetvm_ffi
        LANGUAGES CXX C
)

#option(TVM_FFI_BUILD_TESTS "Adding test targets." ON)
option(TVM_FFI_USE_LIBBACKTRACE "Enable libbacktrace" ON)
option(TVM_FFI_USE_EXTRA_CXX_API "Enable extra CXX API in shared lib" ON)
option(TVM_FFI_BACKTRACE_ON_SEGFAULT "Set signal handler to print traceback on segfault" ON)

#include(cmake/Utils/CxxWarning.cmake)
#include(cmake/Utils/Sanitizer.cmake)
if (TVM_FFI_USE_LIBBACKTRACE)
    include(cmake/Utils/AddLibbacktrace.cmake)
endif ()
include(cmake/Utils/Library.cmake)

########## Target: `dlpack_header` ##########

#add_library(dlpack_header INTERFACE)
#target_include_directories(dlpack_header INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/dlpack/include")

########## Target: `tvm_ffi_header` ##########
# they can be used in cases where user do not want to link into the library
# in cases like deferred linking
add_library(tvm_ffi_header INTERFACE)
target_compile_features(tvm_ffi_header INTERFACE cxx_std_20)
target_include_directories(
        tvm_ffi_header INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_include_directories(
        tvm_ffi_header INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/dlpack/include>
        $<INSTALL_INTERFACE:include>
)


########## Target: `tvm_ffi_objs` ##########
file(GLOB_RECURSE tvm_ffi_objs_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/ffi/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ffi/*.cc
)

file(GLOB_RECURSE tvm_ffi_extra_objs_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/ffi/extra/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ffi/extra/*.cc
)

if (TVM_FFI_USE_EXTRA_CXX_API)
    list(APPEND tvm_ffi_objs_sources ${tvm_ffi_extra_objs_sources})
endif ()

add_library(tvm_ffi_objs OBJECT ${tvm_ffi_objs_sources})
target_compile_features(tvm_ffi_objs PRIVATE cxx_std_20)

set_target_properties(
        tvm_ffi_objs PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        PREFIX "lib"
)

# add the include path as public so they are visible to downstreams
target_link_libraries(tvm_ffi_objs PUBLIC tvm_ffi_header)

if (TVM_FFI_USE_LIBBACKTRACE)
    message(STATUS "Setting C++ macro TVM_FFI_USE_LIBBACKTRACE - 1")
    target_compile_definitions(tvm_ffi_objs PRIVATE TVM_FFI_USE_LIBBACKTRACE=1)
else ()
    message(STATUS "Setting C++ macro TVM_FFI_USE_LIBBACKTRACE - 0")
    target_compile_definitions(tvm_ffi_objs PRIVATE TVM_FFI_USE_LIBBACKTRACE=0)
endif ()

if (TVM_FFI_BACKTRACE_ON_SEGFAULT)
    message(STATUS "Setting C++ macro TVM_FFI_BACKTRACE_ON_SEGFAULT - 1")
    target_compile_definitions(tvm_ffi_objs PRIVATE TVM_FFI_BACKTRACE_ON_SEGFAULT=1)
else ()
    message(STATUS "Setting C++ macro TVM_FFI_BACKTRACE_ON_SEGFAULT - 0")
    target_compile_definitions(tvm_ffi_objs PRIVATE TVM_FFI_BACKTRACE_ON_SEGFAULT=0)
endif ()

tvm_ffi_add_msvc_flags(tvm_ffi_objs)
tvm_ffi_add_target_from_obj(tvm_ffi tvm_ffi_objs)

if (TARGET libbacktrace)
    target_link_libraries(tvm_ffi_objs PRIVATE libbacktrace)
    target_link_libraries(tvm_ffi_shared PRIVATE libbacktrace)
    target_link_libraries(tvm_ffi_static PRIVATE libbacktrace)
endif ()

if (MSVC)
    target_link_libraries(tvm_ffi_objs PRIVATE DbgHelp.lib)
    target_link_libraries(tvm_ffi_shared PRIVATE DbgHelp.lib)
    target_link_libraries(tvm_ffi_static PRIVATE DbgHelp.lib)
    # produce pdb file
    target_link_options(tvm_ffi_shared PRIVATE /DEBUG)
endif ()

target_link_libraries(tvm_ffi_objs PUBLIC tvm_ffi_header)
target_link_libraries(tvm_ffi_shared PUBLIC tvm_ffi_header)
target_link_libraries(tvm_ffi_static PUBLIC tvm_ffi_header)

#----------------------------------------------------------------------------
# The following code section only is triggered when the project is the root
# and will be skipped when the project is a subproject.
#----------------------------------------------------------------------------
if (NOT ${PROJECT_NAME} STREQUAL ${CMAKE_PROJECT_NAME})
    return()
endif ()

option(TVM_FFI_ATTACH_DEBUG_SYMBOLS "Attach debug symbols even in release mode" OFF)
option(TVM_FFI_BUILD_TESTS "Adding test targets." ON)

if (TVM_FFI_ATTACH_DEBUG_SYMBOLS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tvm_ffi_objs PRIVATE -g1)
    endif ()
endif ()

include(cmake/Utils/CxxWarning.cmake)
include(cmake/Utils/Sanitizer.cmake)

# remap the file name to the source directory so we can see the
# exact file name in traceback relative to the project source root
tvm_ffi_add_prefix_map(tvm_ffi_objs ${CMAKE_SOURCE_DIR})

########## Adding tests ##########
if (TVM_FFI_BUILD_TESTS)
    enable_testing()
    message(STATUS "Enable Testing")
    include(cmake/Utils/AddGoogleTest.cmake)
    add_subdirectory(tests/cpp/)
    tvm_ffi_add_cxx_warning(tvm_ffi_objs)
endif ()
