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

#include <string>

namespace nv { namespace perf { namespace tool {

    extern const std::string HtmlTemplate = R"(
<html>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>

  <head>
    <title>GpuDiagnostics</title>
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

      summary {
        display: block;
        padding: 2px 6px;
        background-color: #fff;
        border-radius: 15px;
        box-shadow: 1px 1px 2px black;
        cursor: pointer;
      }

      details {
        display: block;
      }

      details > summary:only-child::-webkit-details-marker {
        display: none;
      }

      details > details {
        margin-left: 22px;
      }

      .value {
        color: #228b22;
        text-align: right;
      }
    </style>

    <script type="text/JavaScript">
      function appendNodeRecursively(key, obj, domNode) {
        let summary = document.createElement('summary');
        summary.innerText = key;
        // exclude dummy root
        if (key !== "") {
          domNode.appendChild(summary);
        }

        // if it's a leaf node
        if (typeof(obj) != 'object') {
          if (obj != null) {
            let span = document.createElement('span'); // wrap the value in a span so we can customize its style
            span.className = 'value';
            span.innerText = obj.toString();
            summary.innerText = summary.innerText + ': ';
            summary.appendChild(span);
          }
          return;
        }

        // for non-leaf nodes
        for (var child in obj) {
          let childNode = document.createElement('details');
          childNode.open = true;
          appendNodeRecursively(child, obj[child], childNode);
          domNode.appendChild(childNode);
        }
      }

      function onBodyLoaded() {
        let main = document.getElementById('main');
        appendNodeRecursively('', g_json, main);
      }
    </script>
  </head>


  <body onload="onBodyLoaded() " style="background-color:#202020;">
    <noscript>
      <p>Enable javascript to see report contents</span>
    </noscript>

    <div>
      <div class="titlearea">
        <div class="titlebar">
          <img src="https://developer.nvidia.com/sites/all/themes/devzone_new/nvidia_logo.png"/>
          <span class="title" id="titlebar_text">Nsight Perf SDK GPU Diagnostics Report</span>
        </div>
      </div>
    </div>

    <div class="section", id="main">
    </div>

    <script>
      g_json = /***JSON_DATA_HERE***/;
    </script>
  </body>

</html>
)";

}}} // nv::perf::tool
