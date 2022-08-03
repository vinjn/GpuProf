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

set(NVPERF_FILES)
# Add NvPerf API
foreach(NvPerf_INCLUDE_DIR IN LISTS NvPerf_INCLUDE_DIRS)
    file(GLOB FILES "${NvPerf_INCLUDE_DIR}/*.h")
    list(APPEND NVPERF_FILES ${FILES})
endforeach()
source_group("NvPerf" FILES ${NVPERF_FILES})

set(NVPERF_UTILITY_FILES)
# Add NvPerf Utility
file(GLOB NVPERF_UTILITY_FILES "${NvPerfUtility_INCLUDE_DIRS}/*.h")
source_group("NvPerfUtility" FILES ${NVPERF_UTILITY_FILES})

set(VULKAN_FILES)
file(GLOB VULKAN_FILES "${Vulkan_INCLUDE_DIRS}/vulkan/*.h")
source_group("Vulkan" FILES ${VULKAN_FILES})

set(SOURCES
    GpuDiag.cpp
    GpuDiagHtmlTemplate.cpp
)

add_executable(GpuDiag
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
    ${VULKAN_FILES}
)
target_include_directories(GpuDiag
    PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${NvPerfUtilityImportsDir}/json-3.9.1
)
target_link_libraries(GpuDiag
    PRIVATE
        NvPerf
        NvPerfUtility
        ${Vulkan_LIBRARY}
)

if(WIN32)
    target_link_libraries(GpuDiag
        PRIVATE
            d3d12
            dxgi
    )
else()
    target_link_libraries(GpuDiag
        PRIVATE
            dl
    )
endif()

if(MSVC)
  target_compile_options(GpuDiag PRIVATE /WX)
endif()

DeployNvPerf(GpuDiag NvPerf)