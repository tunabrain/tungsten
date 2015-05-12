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

# AVX does not do much benefit at the moment, but breaks compilation on some platforms.
# Disabled for now until AVX becomes important enough to reconsider.
set(USE_AVX FALSE CACHE BOOL "Use AVX.")
set(USE_AVX2 FALSE CACHE BOOL "Use AVX2.")

include(OptimizeForArchitecture)
OptimizeForArchitecture()

if (MSVC)
    # Needed by MSVC, but not added by OptimizeForArchitexture()
    add_definitions(-D__SSE__)
endif()

if (USE_AVX)
    message(STATUS "Compiling with AVX support")
    set(__AVX__ 1)
elseif (USE_SSE4_2)
    message(STATUS "Compiling with SSE4.2 support")
elseif (USE_SSSE3)
    message(STATUS "Compiling with SSE3 support")
else()
    message(FATAL_ERROR "The target machine does not support SSE3. At least SSE3 is required")
endif()

if (MSVC)
    add_definitions(-DCONSTEXPR=const -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
else()
    add_definitions(-DCONSTEXPR=constexpr)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Vc_ARCHITECTURE_FLAGS}")

add_subdirectory(src/thirdparty/embree)
add_definitions(-DSTBI_NO_STDIO=1)
add_definitions(-DLODEPNG_NO_COMPILE_DISK=1)
add_definitions(-DUSE_IPV6=1)
add_library(thirdparty STATIC
		src/thirdparty/civetweb/civetweb.c
		src/thirdparty/lodepng/lodepng.cpp
		src/thirdparty/sobol/sobol.cpp
		src/thirdparty/stbi/stb_image.c
		src/thirdparty/miniz/miniz.c
		src/thirdparty/skylight/ArHosekSkyModel.cpp)

if (CMAKE_COMPILER_IS_GNUCXX)
    set(CXX_WARNINGS "-Wall -Wextra -Wpointer-arith -Wcast-align -fstrict-aliasing -Wno-unused-local-typedefs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_WARNINGS} -fvisibility-inlines-hidden")
endif()
set(core_libs core thirdparty embree)

include_directories(src/core src/thirdparty src/thirdparty/embree src)

find_package(OpenEXR)
if (OPENEXR_FOUND)
    message(STATUS "OpenEXR detected. Building with .exr support")
    add_definitions(-DOPENEXR_AVAILABLE)
    include_directories(${OPENEXR_INCLUDE_DIR})
    set(core_libs ${core_libs} ${OPENEXR_LIBRARIES})
else()
    message(STATUS "No OpenEXR detected. Building without .exr support")
endif()

file(GLOB_RECURSE Core_SOURCES "src/core/*.cpp")
add_library(core STATIC ${Core_SOURCES})

add_executable(obj2json src/obj2json/obj2json.cpp)
target_link_libraries(obj2json ${core_libs})

add_executable(json2xml src/json2xml/json2xml.cpp)
target_link_libraries(json2xml ${core_libs})

add_executable(scenemanip src/scenemanip/scenemanip.cpp)
target_link_libraries(scenemanip ${core_libs})

add_executable(hdrmanip src/hdrmanip/hdrmanip.cpp)
target_link_libraries(hdrmanip ${core_libs})

add_executable(tungsten src/tungsten/tungsten.cpp)
target_link_libraries(tungsten ${core_libs})

if (WIN32)
    set(socket_libs wsock32 Ws2_32)
else()
    set(socket_libs "")
endif()

add_executable(tungsten_server src/tungsten-server/tungsten-server.cpp)
target_link_libraries(tungsten_server ${core_libs} ${socket_libs})

find_package(OpenGL)
find_package(GLEW)
find_package(Qt4)
if (OPENGL_FOUND AND GLEW_FOUND AND QT4_FOUND)
    include_directories(${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})
    set(opengl_libs ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})
    
    set(QT_USE_QTOPENGL TRUE)
    include(${QT_USE_FILE})
    add_definitions(${QT_DEFINITIONS})
    
    file(GLOB_RECURSE Editor_SOURCES "src/editor/*.cpp")
    if (APPLE)
        list(APPEND Editor_SOURCES "src/editor/CoreProfileAttributes.mm")
    endif()
    file(GLOB_RECURSE Data_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "data/*")
    foreach (data ${Data_FILES})
        configure_file(${data} ${CMAKE_CURRENT_BINARY_DIR}/${data} COPYONLY)
    endforeach(data)
    add_executable(editor ${Editor_SOURCES} src/editor/resources/Tungsten.rc)
    set_target_properties(editor PROPERTIES AUTOMOC TRUE)
    target_link_libraries(editor ${core_libs} ${opengl_libs} ${QT_LIBRARIES})
    if (APPLE)
        find_library(APPKIT NAMES AppKit) 
        target_link_libraries(editor ${APPKIT})
    endif()
else()
    if (NOT QT4_FOUND)
        message(STATUS "Qt4 not found. Editor will not be built")
    endif()
    if (NOT (GLEW_FOUND AND OPENGL_FOUND))
        message(STATUS "OpenGL not found. Editor will not be built")
    endif()
endif()

