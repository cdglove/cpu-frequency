# Copyright 2015 Google Inc. All Rights Reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.12)
project(cpu-frequency C CXX)

set(PROJECT_SOURCES
  main.cpp
  cpu_frequency.cpp
)

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  set(execute_exact_clocks execute_exact_clocks_32)
else()
   set(execute_exact_clocks execute_exact_clocks_64)
endif()

if(WIN32)
  set(PROJECT_PLATFORM_DIR win32)
else()
  set(PROJECT_PLATFORM_DIR linux)
endif()

if(MSVC)
  enable_language(ASM_MASM)
  set(PROJECT_SOURCES ${PROJECT_SOURCES} masm/${execute_exact_clocks}.asm)
  set(PROJECT_LIBRARIES "")
else()
  enable_language(ASM_NASM)
  set(PROJECT_SOURCES ${PROJECT_SOURCES} nasm/${PROJECT_PLATFORM_DIR}/${execute_exact_clocks}.asm)
  set(PROJECT_LIBRARIES pthread)
endif()

file(GLOB_RECURSE PROJECT_HEADERS src/*.hpp)

add_executable(cpu-frequency ${PROJECT_SOURCES} ${PROJECT_HEADERS})
target_link_libraries(cpu-frequency PUBLIC ${PROJECT_LIBRARIES})