/*
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
*/
#pragma once

#include <string>

namespace nv { namespace perf {

    inline std::string GetReadMeHtml()
    {
        return R"(
<html>

  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>

  <head>
    <title>NvPerfSDK</title>
    <style id="ReportStyle">
      .titlearea {
        display: flex;
        align-items: center;
        color: white;
        font-family: verdana;
      }

      .titlebar {
        margin-left: 0;
        margin-right: auto;
      }

      .title {
        font-size: 28px;
        margin-left: 10px;
      }

      .section {
        border-radius: 15px;
        padding: 10px;
        background: #FFFFFF;
        margin: 10px;
      }

      .section_title {
        font-family: verdana;
        font-weight: bold;
        color: black;
      }

      li {
        white-space: normal;;
      }
    </style>
  </head>

  <body style="background-color:#202020;">
    <div>
      <div class="titlearea">
        <div class="titlebar">
          <img src="https://developer.nvidia.com/sites/all/themes/devzone_new/nvidia_logo.png"/>
          <span class="title" id="titlebar_text">Nsight Perf SDK</span>
        </div>
      </div>
    </div>

    <div class="section" id="intro">
      <h2>Nsight Perf SDK HTML Report</h2>
      <p>Navigate to the <a href="summary.html">summary.html</a> to begin.
    </div>

    <div class="section" id="unintended_use">
      <h2>Unintended Use of Product</h2>
      <p>The Nsight Perf SDK should not be used for benchmarking absolute performance, nor for comparing results between GPUs, due to the following factors:</p>
      <ul>
        <li>To ensure stable measurements, the Perf SDK encourages users to lock the GPU to its <a href=https://en.wikipedia.org/wiki/Thermal_design_power target="_blank">rated TDP (thermal design power)</a>. This forces thermally stable clock rates and disables boost clocks, ensuring consistent performance, but preventing the GPU from reaching its absolute peak performance.</li>
        <li>The Perf SDK disables certain GPU power management settings during profiling, to meet hardware requirements.</li>
        <li>Not all metrics are comparable between GPUs or architectures. For example, a more powerful GPU may complete a workload in less time while showing lower %%-of-peak throughput values, when compared against a less powerful GPU.</li>
      </ul>
      <p>The Nsight Perf SDK is intended to be used for performance profiling, to assist developers and artists in improving the performance of their code, art assets, and GPU shaders.</p>
      <p>See the bundled NVIDIA Developer Tools License document for additional details.</p>
    </div>


  </body>

</html>
)";
    }

}}
