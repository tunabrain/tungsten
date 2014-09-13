cmake_minimum_required(VERSION 2.8)
project(Tungsten)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(CXX11)
check_for_cxx11_compiler(CXX11_COMPILER)

if(CXX11_COMPILER)
    enable_cxx11()
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} seems to have no C++11 support. Please try again with a more recent compiler version.")
endif()

include(OptimizeForArchitecture)
OptimizeForArchitecture()
# Used by Embree
set(__AVX__ 0)

if (USE_AVX)
    message(STATUS "Compiling with AVX support")
    set(__AVX__ 1)
    if (CMAKE_COMPILER_IS_GNUCXX)
        set(SIMD_FLAGS " -mavx")
    elseif (MSVC)
        add_definitions(-D__AVX__ -D__SSE4_2__ -D__SSE4_1__ -D__SSE3__ -D__SSE__)
    endif()
elseif (USE_SSE4_2)
    message(STATUS "Compiling with SSE4.2 support")
    if (CMAKE_COMPILER_IS_GNUCXX)
        set(SIMD_FLAGS " -msse4.2")
    elseif (MSVC)
        add_definitions(-D__SSE4_2__ -D__SSE4_1__ -D__SSE3__ -D__SSE__)
    endif()
elseif (USE_SSSE3)
    message(STATUS "Compiling with SSE3 support")
    if (CMAKE_COMPILER_IS_GNUCXX)
        set(SIMD_FLAGS " -msse3")
    elseif (MSVC)
        add_definitions(-D__SSE3__ -D__SSE__)
    endif()
else()
    message(FATAL_ERROR "The target machine does not support SSE3. At least SSE3 is required")
endif()

if (MSVC)
    add_definitions(-DCONSTEXPR=const -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
else()
    add_definitions(-DCONSTEXPR=constexpr)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SIMD_FLAGS}")

add_subdirectory(src/thirdparty/embree)
add_definitions(-DLODEPNG_NO_COMPILE_DECODER=1)
add_library(thirdparty STATIC
		src/thirdparty/lodepng/lodepng.cpp
		src/thirdparty/sobol/sobol.cpp
		src/thirdparty/stbi/stb_image.c
		src/thirdparty/SFMT/SFMT.c)

if (CMAKE_COMPILER_IS_GNUCXX)
    set(CXX_WARNINGS "-Wall -Wextra -Wpointer-arith -Wcast-align -fstrict-aliasing -Wno-unused-local-typedefs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_WARNINGS} -fvisibility-inlines-hidden")
endif()
set(core_libs core thirdparty embree)

include_directories(src/core src/thirdparty src/thirdparty/embree src)

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
    file(GLOB_RECURSE Data_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "data/*")
    foreach (data ${Data_FILES})
        configure_file(${data} ${CMAKE_CURRENT_BINARY_DIR}/${data} COPYONLY)
    endforeach(data)
    add_executable(editor ${Editor_SOURCES} src/editor/resources/Tungsten.rc)
    set_target_properties(editor PROPERTIES AUTOMOC TRUE)
    target_link_libraries(editor ${core_libs} glew32 glu32 opengl32 ${QT_LIBRARIES})
else()
    message(STATUS "Qt4 not found. Editor will not be built")
endif()