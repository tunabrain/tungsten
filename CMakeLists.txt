cmake_minimum_required(VERSION 2.8)
project(Tungsten)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Enable C++11
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++11" COMPILER_SUPPORTS_CXX11)
check_cxx_compiler_flag("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif (COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

include(OptimizeForArchitecture)
OptimizeForArchitecture()
# Used by Embree
set(__AVX__ 0)
if (USE_AVX)
	message(STATUS "Compiling with AVX support")
    set(__AVX__ 1)
    set(SIMD_FLAGS " -mavx")
elseif (USE_SSE4_2)
	message(STATUS "Compiling with SSE4.2 support")
    set(SIMD_FLAGS " -msse4.2")
elseif (USE_SSSE3)
	message(STATUS "Compiling with SSE3 support")
    set(SIMD_FLAGS " -msse3")
else()
    message(FATAL_ERROR "The target machine does not support SSE3. At least SSE3 is required")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SIMD_FLAGS}")

add_subdirectory(src/extern/embree)
add_definitions(-DLODEPNG_NO_COMPILE_DECODER=1)
add_library(extern STATIC
		src/extern/lodepng/lodepng.cpp
		src/extern/sobol/sobol.cpp
		src/extern/stbi/stb_image.c
		src/extern/SFMT/SFMT.c)

set(CXX_WARNINGS "-Wall -Wextra -Wpointer-arith -Wcast-align -fstrict-aliasing -Wno-unused-local-typedefs")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_WARNINGS} -fvisibility-inlines-hidden")
set(core_libs core extern embree tbb)

include_directories(src/core src/extern src/extern/embree src)

file(GLOB_RECURSE Core_SOURCES "src/core/*.cpp")
add_library(core STATIC ${Core_SOURCES})

add_executable(obj2json src/obj2json/obj2json.cpp)
target_link_libraries(obj2json ${core_libs})

add_executable(json2xml src/json2xml/json2xml.cpp)
target_link_libraries(json2xml ${core_libs})

add_executable(tungsten src/standalone/main.cpp)
target_link_libraries(tungsten ${core_libs})

find_package(Qt4)
if (QT4_FOUND)
    set(QT_USE_QTOPENGL TRUE)
    include(${QT_USE_FILE})
    add_definitions(${QT_DEFINITIONS})
    
    file(GLOB_RECURSE Editor_SOURCES "src/editor/*.cpp")
    file(GLOB_RECURSE Editor_SHADERS "src/editor/shaders/*")
    file(COPY ${Editor_SHADERS} DESTINATION "src/editor/shaders")
    add_executable(editor ${Editor_SOURCES} src/editor/resources/Tungsten.rc)
    set_target_properties(editor PROPERTIES AUTOMOC TRUE)
    target_link_libraries(editor ${core_libs} glew32 glu32 opengl32 ${QT_LIBRARIES})
else()
    message(STATUS "Qt4 not found. Editor will not be built")
endif()