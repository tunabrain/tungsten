## ======================================================================== ##
## Copyright 2009-2016 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

SET(FLAGS_SSE2  "-msse2")
SET(FLAGS_SSE3  "-msse3")
SET(FLAGS_SSSE3 "-mssse3")
SET(FLAGS_SSE41 "-msse4.1")
SET(FLAGS_SSE42 "-msse4.2")
SET(FLAGS_AVX   "-mavx")
SET(FLAGS_AVX2  "-mf16c -mavx2 -mfma -mlzcnt -mbmi -mbmi2")
SET(FLAGS_AVX512KNL "-mavx512f -mavx512pf -mavx512er -mavx512cd")
SET(FLAGS_AVX512SKX "-mavx512f -mavx512dq -mavx512cd -mavx512bw -mavx512vl")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -std=c++11 -fvisibility-inlines-hidden -fvisibility=hidden -fno-strict-aliasing -Wno-misleading-indentation -Wno-parentheses -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-maybe-uninitialized -Wno-terminate -Wno-attributes")
SET(CMAKE_CXX_FLAGS_DEBUG          "-DDEBUG  -DTBB_USE_DEBUG -g -O0")
SET(CMAKE_CXX_FLAGS_RELEASE        "-DNDEBUG                    -O3")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-DDEBUG  -DTBB_USE_DEBUG -g -O3")
SET(CMAKE_EXE_LINKER_FLAGS "")

IF (APPLE)
  SET (CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS_INIT} -dynamiclib)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.7 -stdlib=libc++")
ENDIF (APPLE)

