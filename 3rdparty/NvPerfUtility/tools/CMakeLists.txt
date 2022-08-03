#[[
* Copyright 2014-2022 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
]]

cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
cmake_policy(VERSION 3.7)

project(NvPerfTools)

find_package(NvPerf        REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
find_package(NvPerfUtility REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
find_package(Vulkan        REQUIRED) # requires cmake 3.7

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(NvPerfUtilityImportsDir ${CMAKE_CURRENT_SOURCE_DIR}/../imports)

add_subdirectory(ClockControl)
add_subdirectory(GpuDiag)