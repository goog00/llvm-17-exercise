cmake_minimum_required (VERSION 3.20.0)

project ("JIT2")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
include(DetermineGCCCompatible)
# include(ChooseMSVCCRT)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

endif()


llvm_map_components_to_libnames(llvm_libs Core OrcJIT Support native)

if(LLVM_COMPILER_IS_GCC_COMPATIBLE)
  if(NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  endif()
  if(NOT LLVM_ENABLE_EH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
  endif()
endif()

add_executable(JIT JIT.cpp)
include_directories(${CMAKE_SOURCE_DIR})
target_link_libraries(JIT ${llvm_libs})


