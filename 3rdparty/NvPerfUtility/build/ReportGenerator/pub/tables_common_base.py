# Copyright 2014-2022 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from profiler_report_types import *

def get_common_css():
    return r'''
      table {
        font-size: 14px;
        margin: 2 auto;
        border-collapse: collapse;
        border: 1px solid ;
      }

      table th {
        margin: 0 auto;
        border-collapse: collapse;
        border: 1px solid ;
        background: #F8F8F8;
      }

      table td {
        margin: 0 auto;
        border-collapse: collapse;
        border: 1px solid ;
      }

      table tbody tr td {
        margin: 0 auto;
        border-collapse: collapse;
        border: 1px solid ;
      }

      .tablename {
        color: DarkGreen;
        border-color: Black;
        background: #F8F8F8;
        font-weight: bold;
      }

      .subhdr {
        background: #F8F8F8;
      }

      .ca {
        text-align: center;
      }

      .la {
        text-align: left;
      }

      .ra {
        text-align: right;
      }

      .ww {
        word-wrap: break-word;
      }

      .full_name {
        min-width: 150px;
        width: 33vw;
        max-width: calc(92vw - 920px);
      }

      .base {
        font-size: 8px;
        color: #606060;
        border-color: Black;
      }

      .comp {
        font-size: 8px;
        color: steelblue;
        border-color: Black;
      }

      .not_applicable {
        color: #CCCCCC;
      }

      .not_available {
        color: #888888;
      }

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

      .global_settings {
        margin-left: auto;
        margin-right: 0;
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
        min-width: calc(100% - 40px);
        width: max-content;
      }

      .section_title {
        font-family: verdana;
        font-weight: bold;
        color: black;
      }

      .workflow {
        width: 960px;
        max-width: 90vw;
      }

      .debug_section {
        border-radius: 15px;
        padding: 10px;
        background: #DDDDDD;
        margin: 10px;
        min-width: calc(100% - 40px);
        width: max-content;
      }

      #footer {
        position: fixed;
        left: 0;
        bottom: 0;
        width: 100%;
        background: rgba(225, 225, 225, 0.5);
        color: darkred;
        text-align: center;
        height: 20px;
        line-height: 20px;
        font-weight: 700;
      }
'''

def get_js_common_functions():
    return r'''
      class NotApplicable {
        valueOf() {
          return undefined;
        }
        toString() {
          return 'NotApplicable';
        }
      }
      class NotAvailable {
        valueOf() {
          return undefined;
        }
        toString() {
          return 'NotAvailable';
        }
      }

      function timeToStr(secondsSinceEpoch) {
        var date = new Date(secondsSinceEpoch * 1000); // convert to milliseconds
        var year = date.getFullYear();
        var Months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
        var month = Months[date.getMonth()];
        var day = date.getDate();
        var hours = date.getHours();
        var minutes = "0" + date.getMinutes();
        var seconds = "0" + date.getSeconds();
        var formattedTime = month + ' ' + day + ', ' + year + ' ' + hours + ':' + minutes.substr(-2) + ':' + seconds.substr(-2);
        return formattedTime;
      }

      function escapeHtml(input) {
        if (typeof input == 'string' || input instanceof String) {
          return input
            .replace(/&/g, "&amp;") // make it first
            .replace(/ /g, "&nbsp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&apos;");
        }
        return input;
      }

      function safeDiv(dividend, divisor) {
        if (dividend == 0 && divisor == 0) {
          return 0;
        }
        return dividend / divisor;
      }

      // NaN < -Infinity < Infinity < finite numbers
      function compareNumbers(lhs, rhs) {
        if (isFinite(lhs) && isFinite(rhs)) {
          return lhs - rhs;
        } else if (isFinite(lhs)) {
          return 1;
        } else if (isFinite(rhs)) {
          return -1;
        } else {
          if (isNaN(lhs) && isNaN(rhs)) {
            return 0;
          } else if (isNaN(lhs)) {
            return -1;
          } else if (isNaN(rhs)) {
            return 1;
          } else {
            return (lhs == rhs) ? 0 : ((lhs > rhs) ? 1 : -1);
          }
        }
      }

      function getCounterValue(base, submetric) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        console.assert(typeof base == 'string' || base instanceof String);
        console.assert(typeof submetric == 'string' || submetric instanceof String);

        if (g_debug) {
          g_counters_referenced.add(base);
        }

        counter = g_counters[base];
        if (counter === undefined) {
          counter = {};
          g_counters[base] = counter;
        }
        value = counter[submetric];
        if (value === undefined) {
          if (g_populateDummyValues) {
            value = Math.random() * 100;
          } else {
            value = NaN;
          }
          counter[submetric] = value;
        }
        return value;
      }

      function getCounterPct(base) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        return getCounterValue(base, 'avg.pct_of_peak_sustained_elapsed');
      }

      function getCounterDimUnits(base) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        console.assert(typeof base == 'string' || base instanceof String);

        if (g_debug) {
          g_counters_referenced.add(base);
        }

        counter = g_counters[base];
        if (counter === undefined) {
          counter = {};
          g_counters[base] = counter;
        }
        dim_units = counter['dim_units'];
        if (dim_units === undefined) {
          dim_units = '';
          counter['dim_units'] = dim_units;
        }
        return dim_units;
      }

      function getRatioValue(base, submetric) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        if (!base) {
          return NaN;
        }
        console.assert(typeof base == 'string' || base instanceof String);
        console.assert(typeof submetric == 'string' || submetric instanceof String);

        if (g_debug) {
          g_ratios_referenced.add(base);
        }

        ratio = g_ratios[base];
        if (ratio === undefined) {
          ratio = {};
          g_ratios[base] = ratio;
        }
        value = ratio[submetric];
        if (value === undefined) {
          if (g_populateDummyValues) {
            value = Math.random() * 100;
          } else {
            value = NaN;
          }
          ratio[submetric] = value;
        }
        return value;
      }

      function getRatioPct(base) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        return getRatioValue(base, 'pct');
      }

      function getThroughputValue(base, submetric) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        if (!base) {
          return NaN;
        }
        console.assert(typeof base == 'string' || base instanceof String);
        console.assert(typeof submetric == 'string' || submetric instanceof String);

        if (g_debug) {
          g_throughputs_referenced.add(base);
        }

        throughput = g_throughputs[base];
        if (throughput === undefined) {
          throughput = {};
          g_throughputs[base] = throughput;
        }
        value = throughput[submetric];
        if (value === undefined) {
          if (g_populateDummyValues) {
            value = Math.random() * 100;
          } else {
            value = NaN;
          }
          throughput[submetric] = value;
        }
        return value;
      }

      function getThroughputPct(base) {
        if (base instanceof NotApplicable || base instanceof NotAvailable) {
          return base;
        }
        return getThroughputValue(base, 'avg.pct_of_peak_sustained_elapsed');
      }

      function toBarChart(pct, ch) {
        if (pct instanceof NotApplicable || pct instanceof NotAvailable) {
          return '';
        }
        const clampedPct = Math.min(Math.max(pct, 0), 100); // clamp to range
        const width = 20;
        const barwidth = Math.ceil((clampedPct / 100) * width);
        bar = '';
        for (let ii = 1; ii <= barwidth; ii++) {
          bar += ch;
        }
        return bar;
      }

      function roundUp(num, precision) {
        precision = Math.pow(10, precision);
        return Math.ceil(num * precision) / precision;
      }

      function format_pct(pct, precision=1) {
        if (pct instanceof NotApplicable || pct instanceof NotAvailable) {
          return pct;
        }
        if (isNaN(pct)) {
          return '⚠';
        }
        if (!isFinite(pct)) {
          return (Math.sign(pct) == 1) ? '∞' : '-∞';
        }
        return roundUp(pct, precision).toLocaleString(undefined, { minimumFractionDigits: precision, maximumFractionDigits: precision });
      }

      function format_avg(avg, precision=1) {
        if (avg instanceof NotApplicable || avg instanceof NotAvailable) {
          return avg;
        }
        if (isNaN(avg)) {
          return '⚠';
        }
        if (!isFinite(avg)) {
          return (Math.sign(avg) == 1) ? '∞' : '-∞';
        }
        return avg.toLocaleString(undefined, { minimumFractionDigits: precision, maximumFractionDigits: precision });
      }

      function format_sum(sum, precision=1) {
        if (sum instanceof NotApplicable || sum instanceof NotAvailable) {
          return sum;
        }
        if (isNaN(sum)) {
          return '⚠';
        }
        if (!isFinite(sum)) {
          return (Math.sign(sum) == 1) ? '∞' : '-∞';
        }
        return sum.toLocaleString(undefined, { minimumFractionDigits: precision, maximumFractionDigits: precision });
      }

      function addCellAttr(trow, attributes, text, passThrough = false) {
        let td = document.createElement('td');
        for (const attr in attributes) {
          td.setAttribute(attr, attributes[attr]);
        }
        if (text instanceof NotApplicable) {
          let span = document.createElement('span');
          span.setAttribute("class", "not_applicable");
          span.innerHTML = '-';
          td.appendChild(span);
        } else if (text instanceof NotAvailable) {
          let span = document.createElement('span');
          span.setAttribute("class", "not_available");
          span.innerHTML = '-';
          td.appendChild(span);
        } else {
          if (passThrough) {
            td.innerHTML = text;
          } else {
            td.innerHTML = escapeHtml(text);
          }
        }
        trow.appendChild(td);
      }

      function addCellSimple(trow, classes, text, passThrough = false) {
        addCellAttr(trow, { 'class': classes }, text, passThrough);
      }

      // for debugging utility
      function clearReferencedMetrics() {
        g_counters_referenced.clear();
        g_ratios_referenced.clear();
        g_throughputs_referenced.clear();
      }

      function tbody_RequiredCounters(tbody) {
        let metricNames = [];
        for(const counter of g_counters_referenced) {
          metricNames.push(counter);
        }
        metricNames.sort();

        for (const metricName of metricNames) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la", "'" + metricName + "',");
          tbody.appendChild(trow);
        }
        return metricNames;
      }

      function tbody_RequiredRatios(tbody) {
        let metricNames = [];
        for(const ratio of g_ratios_referenced) {
          metricNames.push(ratio);
        }
        metricNames.sort();

        for (const metricName of metricNames) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la", "'" + metricName + "',");
          tbody.appendChild(trow);
        }
        return metricNames;
      }

      function tbody_RequiredThroughputs(tbody) {
        let metricNames = [];
        for(const throughput of g_throughputs_referenced) {
          metricNames.push(throughput);
        }
        metricNames.sort();

        for (const metricName of metricNames) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la", "'" + metricName + "',");
          tbody.appendChild(trow);
        }
        return metricNames;
      }

      // calculate the row span based on grouping rows that have the same attribute retrieved via getRowAttribute(),
      // return 0 if the current row has the same attribute as its previous row
      function calcRowSpan(rows, rowIdx, getRowAttribute) {
        let rowspan = 0;
        if (!rowIdx || getRowAttribute(rows[rowIdx]) != getRowAttribute(rows[rowIdx - 1])) {
          for (let ii = rowIdx; ii < rows.length; ++ii) {
            if (getRowAttribute(rows[rowIdx]) != getRowAttribute(rows[ii])) {
              break;
            }
            ++rowspan;
          }
        }
        return rowspan;
      }
'''

class DevicePropertiesGenerator:
    class Row:
        def __init__(row, name, attr, value):
            row.name = name
            row.attr = attr
            row.value = value

    def __init__(gen):
        gen.name = 'DeviceProperties'
        gen.jsfunc_logic = '''
        const smCount = Math.round(safeDiv(getCounterValue('sm__cycles_elapsed', 'sum'), getCounterValue('sm__cycles_elapsed', 'avg')));
        const ltsCount = Math.round(safeDiv(getCounterValue('lts__cycles_elapsed', 'sum'), getCounterValue('lts__cycles_elapsed', 'avg')));
        const l2cacheSizeKiB = ltsCount * {gen.l2cacheSizePerLts};
'''
        gen.rows = [
            gen.Row('Date & Time'           , "ra", "timeToStr(g_time)"),
            gen.Row('GPU Name'              , "ra", "g_device.gpuName"),
            gen.Row('Chip Name'             , "ra", "g_device.chipName"),
            gen.Row('Clock Locking Status'  , "ra", "g_device.clockLockingStatus"),
            gen.Row('# SMs'                 , "ra", "smCount"),
            gen.Row('L2 Cache Size (KiB)'   , "ra", "l2cacheSizeKiB"),
        ]
        # the list of required metrics can be pasted from the debug model of the HTML, it will also compare this list to the actual used metric list
        # and trigger a warning if they're not identical
        gen.required_counters = [
            'lts__cycles_elapsed',
            'sm__cycles_elapsed',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None # self explanatory; do not clutter

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Device-Properties">
          <thead>
            <tr>
              <th colspan="2" class="ca tablename">Device Properties</th>
            </tr>
            <tr>
              <th class="la">Name</th>
              <th class="la">Value</th>
            </tr>
          </thead>
          <tbody id="tbody_device_properties">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_DeviceIds(tbody) {
        class Row {
          constructor(name, valueclass, value) {
            this.name = name;
            this.valueclass = valueclass;
            this.value = value;
          }
        }

''']
        jsfunc_lines += [gen.jsfunc_logic.format(gen=gen)]
        jsfunc_lines += ['''
        let rows = [
''']
        for row in gen.rows:
            qname = "'{row.name}'".format(row=row)
            qattr = '"{row.attr}"'.format(row=row)
            jsfunc_lines += ["          new Row({qname:<40}, {qattr:<10}, {row.value:<60}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, row.valueclass, row.value);
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_DeviceIds(document.getElementById('tbody_device_properties'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class ClocksGeneratorBase:
    class Row:
        def __init__(row, name, time, time_units, freq, freq_units):
            row.name = name
            row.time = time
            row.time_units = time_units
            row.freq = freq
            row.freq_units = freq_units

    def __init__(gen):
        gen.name = 'Clocks'
        gen.rows = [
            gen.Row('Time Duration'     , "getCounterValue('gpu__time_duration', 'sum')"    , 'ns'          , 'new NotApplicable'                                                       , ''),
            gen.Row('SYS Clock'         , "getCounterValue('sys__cycles_elapsed', 'avg')"   , 'sys_clks'    , "getCounterValue('sys__cycles_elapsed', 'avg.per_second') * 1e-6"         , 'MHz'),
            gen.Row('GPC Clock'         , "getCounterValue('gpc__cycles_elapsed', 'avg')"   , 'gpc_clks'    , "getCounterValue('gpc__cycles_elapsed', 'avg.per_second') * 1e-6"         , 'MHz'),
            gen.Row('L2 Clock'          , "getCounterValue('lts__cycles_elapsed', 'avg')"   , 'lts_clks'    , "getCounterValue('lts__cycles_elapsed', 'avg.per_second') * 1e-6"         , 'MHz'),
        ]
        gen.required_counters = [
            'gpc__cycles_elapsed',
            'gpu__time_duration',
            'lts__cycles_elapsed',
            'sys__cycles_elapsed',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None # self explanatory; do not clutter

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Device-Clocks">
          <thead>
            <tr>
              <th colspan="5" class="ca tablename">Device Clocks Measured</th>
            </tr>
            <tr>
              <th class="la">Name</th>
              <th class="ra">Elapsed Time</th>
              <th class="ra">Time Units</th>
              <th class="ra">Avg. Frequency</th>
              <th class="ra">Freq Units</th>
            </tr>
          </thead>
          <tbody id="tbody_device_clocks">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_DeviceClocks(tbody) {
        class Row {
          constructor(name, time, timeUnits, freq, freqUnits) {
            this.name = name;
            this.time = time;
            this.timeUnits = timeUnits;
            this.freq = freq;
            this.freqUnits = freqUnits;
          }
        }

''']
        jsfunc_lines += ['''
        let rows = [
''']
        for row in gen.rows:
            qname = "'{row.name}'".format(row=row)
            qtimeUnits = "'{row.time_units}'".format(row=row)
            qfreqUnits = "'{row.freq_units}'".format(row=row)
            jsfunc_lines += ["          new Row({qname:<20}, {row.time:<70}, {qtimeUnits:<13}, {row.freq:<70}, {qfreqUnits:<13}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, "ra", format_sum(row.time));
          addCellSimple(trow, "ra", row.timeUnits);
          addCellSimple(trow, "ra", format_sum(row.freq));
          addCellSimple(trow, "ra", row.freqUnits);
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_DeviceClocks(document.getElementById('tbody_device_clocks'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class TopLevelStatsGenerator:
    class Row:
        def __init__(row, category, name, value, pct, description):
            row.category = category
            row.name = name
            row.value = value
            row.pct = pct
            row.description = description

    def __init__(gen):
        gen.name = 'TopLevelStats'
        gen.rows = [
            gen.Row('3D+Compute'    , 'GR Engine Active'            , "getCounterValue('gr__cycles_active', 'avg')"                         , "getCounterPct('gr__cycles_active')"              , "The GR Engine executes all 3D and Compute workloads."),
            gen.Row('3D'            , 'Hardware Draw Calls'         , "getCounterValue('fe__draw_count', 'sum')"                            , "new NotApplicable"                               , "HW draw count may exceed API draw calls, and may include clears."),
            gen.Row('Compute'       , 'Hardware Compute Dispatches' , "getCounterValue('gr__dispatch_count', 'sum')"                        , "new NotApplicable"                               , "HW dispatch count may exceed API dispatches."),
            gen.Row('Stalls'        , 'Wait For Idle Commands'      , "getCounterValue('fe__output_ops_type_bundle_cmd_go_idle', 'sum')"    , "new NotApplicable"                               , "Wait-for-idle commands stall the GPU Front End between commands."),
            gen.Row('Stalls'        , 'Pixel Shader Barriers'       , "getCounterValue('fe__pixel_shader_barriers', 'sum')"                 , "new NotApplicable"                               , "Pixel shader barriers stall the PROP unit between draw calls."),
            gen.Row('Shader'        , 'SM Active Cycles'            , "getCounterValue('sm__cycles_active', 'avg')"                         , "getCounterPct('sm__cycles_active')"              , "Indicates when shaders were running."),
            gen.Row('Shader'        , 'SM Active Cycles - 3D'       , "getCounterValue('tpc__cycles_active_shader_3d', 'avg')"              , "getCounterPct('tpc__cycles_active_shader_3d')"   , "Indicates when 3D shaders were running.  May overlap with compute."),
            gen.Row('Shader'        , 'SM Active Cycles - Compute'  , "getCounterValue('sm__cycles_active_shader_cs', 'avg')"               , "getCounterPct('sm__cycles_active_shader_cs')"    , "Indicates when compute shaders were running.  May overlap with 3D."),
            gen.Row('Shader'        , 'SM Instruction Issue Cycles' , "getCounterValue('sm__issue_active', 'avg')"                          , "getCounterPct('sm__issue_active')"               , "Indicates how often an SM issued instructions, on average."),
            gen.Row('Shader'        , 'Warp Occupancy (per SM)'     , "getCounterValue('sm__warps_active', 'avg.per_cycle_elapsed')"        , "getCounterPct('sm__warps_active')"               , "Resident warps per SM, on average.  Low occupancy is only a problem when Issue Active% is low."),
        ]
        gen.required_counters = [
            'fe__draw_count',
            'fe__output_ops_type_bundle_cmd_go_idle',
            'fe__pixel_shader_barriers',
            'gr__cycles_active',
            'gr__dispatch_count',
            'sm__cycles_active',
            'sm__cycles_active_shader_cs',
            'sm__issue_active',
            'sm__warps_active',
            'tpc__cycles_active_shader_3d',
        ]
        gen.required_ratios = [
        ]
        gen.required_throughputs = [
        ]
        gen.workflow = '''Top-Level Stats:
This table and Top Throughputs provide an overview of the type of workload executed.
If GR Engine Active% is not close to 100%, the range is likely starved by the CPU; use a trace tool to improve that, before returning to low-level GPU profiling.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Top-Level-Stats">
          <thead>
            <tr>
              <th colspan="3" class="ca tablename">Top-Level Stats</th>
              <th colspan="2" class="ca">%-of-Peak</th>
              <th colspan="1" class="ca"></th>
            </tr>
            <tr>
              <th class="la">Category</th>
              <th class="la">Name</th>
              <th class="ra">Value</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="la">Description</th>
            </tr>
          </thead>
          <tbody id="tbody_top_level_stats">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_TopLevelStats(tbody) {
        class Row {
          constructor(category, name, value, pct, description) {
            this.category = category;
            this.name = name;
            this.value = value;
            this.pct = pct;
            this.description = description;
          }
        }
''']
        jsfunc_lines += ['''
        let rows = [
''']
        for row in gen.rows:
            qcategory = "'{row.category}'".format(row=row)
            qname = "'{row.name}'".format(row=row)
            jsfunc_lines += ["          new Row({qcategory:<20}, {qname:<20}, {row.value:<50}, {row.pct}, \"{row.description}\"),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (let rowIdx = 0; rowIdx < rows.length; ++rowIdx) {
          const row = rows[rowIdx];
          var trow = document.createElement('tr');
          const rowspan = calcRowSpan(rows, rowIdx, (row) => row.category);
          if (rowspan) {
            addCellAttr(trow, {'rowspan':rowspan, 'class':"la subhdr"}, row.category);
          }
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, "ra", format_avg(row.value));
          addCellSimple(trow, "ra", format_pct(row.pct));
          addCellSimple(trow, "la comp", toBarChart(row.pct, '█'));
          addCellSimple(trow, "la subhdr", row.description, passThrough=true);
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_TopLevelStats(document.getElementById('tbody_top_level_stats'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class TopThroughputsGenerator:
    class Row:
        def __init__(row, category, name, value):
            row.category = category
            row.name = name
            row.value = value

    def __init__(gen):
        gen.name = 'TopThroughputs'
        gen.rows = []
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''Top Throughputs:
Observe the most utilized hardware units, and navigate to their corresponding sections for more details.  The rows are sorted; the first row always has the highest utilization.
If all unit throughputs are less than 60%, check whether the range is starvation-limited (low <a href="#Top-Level-Stats">GR Engine Active%</a>).
If not starvation-limited, conclude that the workload is latency-limited.  Investigate <a href="#L2-Sector-Traffic">L2 Sector Traffic</a> and <a href="#SM-Warp-Issue-Stall-Reasons">SM Warp Issue Stall Reasons</a> for additional clues.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Top-Throughputs">
          <thead>
            <tr>
              <th colspan="2" class="la tablename">Top Throughputs</th>
              <th colspan="2" class="ca">%-of-Peak</th>
            </tr>
            <tr>
              <th class="la">Category</th>
              <th class="la">Throughput Name</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_top_throughputs">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_TopThroughputs(tbody) {
        class Row {
          constructor(category, name, value) {
            this.category = category;
            this.name = name;
            this.value = value;
          }
        }
''']
        jsfunc_lines += ['''
        let throughputs = [
''']
        for row in gen.rows:
            qcategory = "'{row.category}'".format(row=row)
            qname = "'{row.name}'".format(row=row)
            jsfunc_lines += ["          new Row({qcategory:<20}, {qname:<20}, {row.value:<50}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];
        throughputs.sort((lhsrow, rhsrow) => compareNumbers(rhsrow.value, lhsrow.value)); // sort in descending order

        for (const row of throughputs) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.category);
          addCellSimple(trow, "la subhdr", row.name, passThrough=true);
          addCellSimple(trow, "ra", format_pct(row.value));
          addCellSimple(trow, "la comp", toBarChart(row.value, '█'));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_TopThroughputs(document.getElementById('tbody_top_throughputs'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class CacheHitRates:
    class Row:
        def __init__(row, category, name, hit_rate_total):
            row.category = category
            row.name = name
            row.hit_rate_total = hit_rate_total

    def __init__(gen):
        gen.name = 'CacheHitRates'
        gen.rows = [
            gen.Row('Indexed Constants'                 , 'IDC Cache Hit-Rate %'        , "getRatioPct('idc__request_hit_rate')"    ),
            gen.Row('Local, Global, Texture, Surface'   , 'L1TEX Cache Hit-Rate %'      , "getRatioPct('l1tex__t_sector_hit_rate')" ),
            gen.Row('All Cached Memory'                 , 'L2 Cache Hit-Rate %'         , "getRatioPct('lts__t_sector_hit_rate')"   ),
        ]
        gen.required_counters = [
        ]
        gen.required_ratios = [
            'idc__request_hit_rate',
            'l1tex__t_sector_hit_rate',
            'lts__t_sector_hit_rate',
        ]
        gen.required_throughputs = [
        ]
        gen.workflow = '''Cache Hit-Rates:
Before considering cache hit-rates to be a problem, first determine if the corresponding unit throughput is high, or if the cache is a source of <a href="#SM-Warp-Issue-Stall-Reasons">warp stall cycles<a>.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Cache-Hit-Rates">
          <thead>
            <tr>
              <th colspan="2" class="ca tablename">Cache Hit-Rates</th>
              <th colspan="2" class="ca">Hit-Rates%</th>
            </tr>
            <tr>
              <th class="la">Memory Spaces</th>
              <th class="la">Name</th>
              <th class="ra">All Ops%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_cache_hit_rates">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_CacheHitRates(tbody) {
        class Row {
          constructor(category, name, hitRateTotal) {
            this.category = category;
            this.name = name;
            this.hitRateTotal = hitRateTotal;
          }
        }
''']
        jsfunc_lines += ['''
        let rows = [
''']
        for row in gen.rows:
            qcategory = "'{row.category}'".format(row=row)
            qname = "'{row.name}'".format(row=row)
            jsfunc_lines += ["          new Row({qcategory:<20}, {qname:<20}, {row.hit_rate_total:<50}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.category);
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, "ra", format_pct(row.hitRateTotal));
          addCellSimple(trow, "la comp", toBarChart(row.hitRateTotal, '█'));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_CacheHitRates(document.getElementById('tbody_cache_hit_rates'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class SmThroughputsGenerator:
    class Pipe:
        def __init__(pipe, name, description, activity='', hasInstExecuted=True):
            pipe.name = name
            pipe.description = description
            pipe.instExecuted = 'sm__inst_executed_pipe_' + name if hasInstExecuted else ''
            pipe.activity = activity

        def get_counter_names(pipe):
            counter_names = []
            if pipe.instExecuted:
                counter_names += [pipe.instExecuted]
            if pipe.activity:
                counter_names += [pipe.activity]
            return counter_names

    def __init__(gen):
        gen.name = 'SmThroughputs'
        gen.pipes = []
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SM Instruction Throughput:
Determine which shader pipelines are limiting performance, and their corresponding shader instructions.
Since all pipelines execute in parallel, it is possible for multiple pipelines to reach 100% simultaneously.
'''

    @property
    def html(gen):
        return r'''
        <table style="display: inline-block; border: 1px solid;" id="SM-Instruction-Throughput">
          <thead>
            <tr>
              <th colspan="2" class="la tablename">SM Instruction Throughput</th>
              <th colspan="1" class="ca">GPU Total</th>
              <th colspan="2" class="ca">per-SM</th>
              <th colspan="2" class="ca">%-of-Peak</th>
            </tr>
            <tr>
              <th class="la">Description</th>
              <th class="la">Pipeline</th>
              <th class="la">Warp Inst</th>
              <th class="ra">IPC</th>
              <th class="ra">&#x25C2;Peak</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_sm_throughput">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_SmThroughput(tbody) {
        class Row {
          constructor(name, instExecuted, activity, description) {
            this.name = name;
            this.instExecuted = instExecuted;
            this.activity = activity;
            this.description = description;
          }
        }

        let pipes = [
''']
        for pipe in gen.pipes:
            qname = "'{pipe.name}'".format(pipe=pipe)
            qdesc = "'{pipe.description}'".format(pipe=pipe)
            qinstExec = "'{pipe.instExecuted}'".format(pipe=pipe)
            qactivity = "'{pipe.activity}'".format(pipe=pipe)
            jsfunc_lines += ["          new Row({qname:<20}, {qinstExec:<30}, {qactivity:<30}, {qdesc:<60}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const pipe of pipes) {
          const instExecuted = pipe.instExecuted;
          const sum = instExecuted ? getCounterValue(instExecuted, 'sum') : new NotAvailable;
          const ipc = instExecuted ? getCounterValue(instExecuted, 'avg.per_cycle_elapsed') : new NotAvailable;
          const ppc = instExecuted ? getCounterValue(instExecuted, 'avg.peak_sustained') : new NotAvailable;
          const pct = pipe.activity ? getCounterPct(pipe.activity) : getCounterPct(instExecuted);

          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", pipe.description);
          addCellSimple(trow, "la subhdr", pipe.name);
          addCellSimple(trow, "ra", format_sum(sum));
          addCellSimple(trow, "ra", format_avg(ipc, 2));
          addCellSimple(trow, "ra", format_avg(ppc, 2));
          addCellSimple(trow, "ra", format_pct(pct));
          addCellSimple(trow, "la comp", toBarChart(pct, '█'));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_SmThroughput(document.getElementById('tbody_sm_throughput'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class L1TexThroughputsGenerator:
    class PipeStage:
        def __init__(ps, name, lsuChartAlign, lsuPct, texPct):
            ps.name = name
            ps.lsuChartAlign = lsuChartAlign
            ps.lsuPct = lsuPct
            ps.texPct = texPct

    def __init__(gen):
        gen.name = 'L1TexThroughputs'
        gen.pipe_stages = [
            gen.PipeStage('SM Instructions'     , 'la', "getCounterPct('sm__inst_executed_pipe_lsu')"                   ,   "getCounterPct('sm__inst_executed_pipe_tex')"),
            gen.PipeStage('MIO Parameter Queue' , 'ra', "getCounterPct('sm__mio_pq_read_cycles_active_pipe_lsu')"       ,   "getCounterPct('sm__mio_pq_read_cycles_active_pipe_tex')"),
            gen.PipeStage('Input Cycles'        , 'la', "getCounterPct('l1tex__lsuin_requests')"                        ,   "getCounterPct('l1tex__texin_sm2tex_req_cycles_active')"),
            gen.PipeStage('M-Stage to XBAR'     , 'ra', "getThroughputPct('l1tex__m_l1tex2xbar_throughput_pipe_lsu')"   ,   "getThroughputPct('l1tex__m_l1tex2xbar_throughput_pipe_tex')"),
            gen.PipeStage('XBAR to M-Stage'     , 'ra', "getThroughputPct('l1tex__m_xbar2l1tex_throughput_pipe_lsu')"   ,   "getThroughputPct('l1tex__m_xbar2l1tex_throughput_pipe_tex')"),
            gen.PipeStage('Data Cycles'         , 'la', "getCounterPct('l1tex__data_pipe_lsu_wavefronts')"              ,   "getCounterPct('l1tex__data_pipe_tex_wavefronts')"),
            gen.PipeStage('TEX Filter Cycles'   , 'la', "new NotApplicable"                                             ,   "getCounterPct('l1tex__data_pipe_tex_wavefronts')"),
            gen.PipeStage('Writeback Cycles'    , 'la', "getCounterPct('l1tex__lsu_writeback_active')"                  ,   "getCounterPct('l1tex__tex_writeback_active')"),
        ]
        gen.required_counters = [
            'l1tex__data_pipe_lsu_wavefronts',
            'l1tex__data_pipe_tex_wavefronts',
            'l1tex__lsu_writeback_active',
            'l1tex__lsuin_requests',
            'l1tex__tex_writeback_active',
            'l1tex__texin_sm2tex_req_cycles_active',
            'sm__inst_executed_pipe_lsu',
            'sm__inst_executed_pipe_tex',
            'sm__mio_pq_read_cycles_active_pipe_lsu',
            'sm__mio_pq_read_cycles_active_pipe_tex',
        ]
        gen.required_ratios = []
        gen.required_throughputs = [
            'l1tex__m_l1tex2xbar_throughput_pipe_lsu',
            'l1tex__m_l1tex2xbar_throughput_pipe_tex',
            'l1tex__m_xbar2l1tex_throughput_pipe_lsu',
            'l1tex__m_xbar2l1tex_throughput_pipe_tex',
        ]
        gen.workflow = '''L1TEX Throughput:
Determine which stage of the L1TEX Cache's pipeline is limiting performance.
The LSU and TEX pipes execute in parallel.  LSU handles local, global, surface stores, and shared memory; and miscellaneous instructions.  TEX handles texture and surface reads.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="L1TEX-Throughput">
          <thead>
            <tr>
              <th class="la tablename" colspan="1">L1TEX Throughput</th>
              <th class="ca" colspan="2">LSU %-of-Peak</th>
              <th class="ca" colspan="2">TEX %-of-Peak</th>
            </tr>
            <tr>
              <th class="la">Pipe Stage</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">%</th>
            </tr>
          </thead>
          <tbody id="tbody_l1tex_throughput">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_L1TexThroughput(tbody) {
        class Row {
          constructor(name, lsuChartAlign, lsuPct, texPct) {
            this.name = name;
            this.lsuChartAlign = lsuChartAlign;
            this.lsuPct = lsuPct;
            this.texChartAlign = "la";
            this.texPct = texPct;
          }
        }
        let rows = [];
''']
        for pipe_stage in gen.pipe_stages:
            qname = "'{pipe_stage.name}'".format(pipe_stage=pipe_stage)
            qalign = "'{pipe_stage.lsuChartAlign}'".format(pipe_stage=pipe_stage)
            jsfunc_lines += ["        rows.push(new Row({qname:<30}, {qalign:<10}, {pipe_stage.lsuPct:<60}, {pipe_stage.texPct:<60}));\n".format(**locals())]
        jsfunc_lines += ['''
        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, "ra", format_pct(row.lsuPct));
          addCellSimple(trow, row.lsuChartAlign + " comp", toBarChart(row.lsuPct, '█'));
          addCellSimple(trow, row.texChartAlign + " comp", toBarChart(row.texPct, '█'));
          addCellSimple(trow, "ra", format_pct(row.texPct));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_L1TexThroughput(document.getElementById('tbody_l1tex_throughput'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class L1TexTrafficBreakdownGenerator:
    # The traffic breakdown forms a tree structure; model it directly.
    # If ratio_addends is empty, it will automatically use the sum of its children.
    class Node:
        def __init__(node, label, ratio_addends, children):
            node.label = label
            node.ratio_addends = ratio_addends
            if len(node.ratio_addends) == 0:
                for child in children:
                    node.ratio_addends.extend(child.ratio_addends)
            node.children = children

        def to_javascript(node, base_indent):
            def to_js_recursive(cur_node, indent):
                str = ''
                str += '  ' * indent
                str += "new Node('{cur_node.label}', [".format(cur_node=cur_node)
                for ratio_addend in cur_node.ratio_addends:
                    str += "'" + ratio_addend + "', "
                if len(cur_node.children):
                    str += '], [\n'
                    for child in cur_node.children:
                        str += to_js_recursive(child, indent + 1)
                    str += '  ' * indent + ']),\n'
                else:
                    str += '], []),\n'
                return str
            return to_js_recursive(node, base_indent)

    def get_required_ratios(gen):
        # note: this function is factored out, in case we need to split the table, or define per-architecture metrics
        def _get_required_ratios_recursive(nodes):
            required_ratios = []
            for node in nodes:
                required_ratios += node.ratio_addends
                required_ratios += [ratio_addend + '_lookup_hit' for ratio_addend in node.ratio_addends]
                required_ratios += _get_required_ratios_recursive(node.children)
            return required_ratios
        required_ratios = _get_required_ratios_recursive(gen.nodes)
        return required_ratios

    def __init__(gen):
        gen.name = 'L1TexTrafficBreakdown'
        gen.nodes = [
            gen.Node('LSU by Mem', ['l1tex__average_t_sector_pipe_lsu'], [
                gen.Node('Global', [], [
                    gen.Node('Global Load' , ['l1tex__average_t_sector_pipe_lsu_mem_global_op_ld'], []),
                    gen.Node('Global Store', ['l1tex__average_t_sector_pipe_lsu_mem_global_op_st'], []),
                    gen.Node('Global Atom' , ['l1tex__average_t_sector_pipe_lsu_mem_global_op_atom'], []),
                    gen.Node('Global Red'  , ['l1tex__average_t_sector_pipe_lsu_mem_global_op_red'], []),
                ]),
                gen.Node('Local', [], [
                    gen.Node('Local Load' , ['l1tex__average_t_sector_pipe_lsu_mem_local_op_ld'], []),
                    gen.Node('Local Store', ['l1tex__average_t_sector_pipe_lsu_mem_local_op_st'], []),
                ]),
            ]),
            gen.Node('TEX by Mem', ['l1tex__average_t_sector_pipe_tex'], [
               gen.Node('Texture', ['l1tex__average_t_sector_pipe_tex_mem_texture'], [
                    gen.Node('Texture Fetch', ['l1tex__average_t_sector_pipe_tex_mem_texture_op_tex'], []),
                    gen.Node('Texture Load' , ['l1tex__average_t_sector_pipe_tex_mem_texture_op_ld'], []),
                ]),
                gen.Node('Surface', ['l1tex__average_t_sector_pipe_tex_mem_surface'], [
                    gen.Node('Surface Load' , ['l1tex__average_t_sector_pipe_tex_mem_surface_op_ld'], []),
                    gen.Node('Surface Store', ['l1tex__average_t_sector_pipe_tex_mem_surface_op_st'], []),
                    gen.Node('Surface Atom' , ['l1tex__average_t_sector_pipe_tex_mem_surface_op_atom'], []),
                    gen.Node('Surface Red'  , ['l1tex__average_t_sector_pipe_tex_mem_surface_op_red'], []),
                ]),
             ]),
            gen.Node('TEX by Format', ['l1tex__average_t_sector_pipe_tex'], [
               gen.Node('1D Buffer'                 , ['l1tex__average_t_sector_pipe_tex_format_1d_buffer'], []),
               gen.Node('1D or 2D Tex/Surf'         , ['l1tex__average_t_sector_pipe_tex_format_1d_2d'], []),
               gen.Node('2D Tex/Surf, no Mipmaps'   , ['l1tex__average_t_sector_pipe_tex_format_2d_nomipmap'], []),
               gen.Node('3D Tex/Surf'               , ['l1tex__average_t_sector_pipe_tex_format_3d'], []),
               gen.Node('Cubemap'                   , ['l1tex__average_t_sector_pipe_tex_format_cubemap'], []),
             ]),
        ]

        gen.required_counters = []
        gen.required_ratios = gen.get_required_ratios()
        gen.required_throughputs = []
        gen.workflow = '''L1TEX Sector Traffic:
Determine which shader memory spaces and operations are incurring the most L1TEX sector bandwidth.
Hit-rates are calculated per pipe/memory/op combination (both numerator and denominator are specific to the pipe/memory/op).  Low hit-rates are only a problem if the corresponding %-of-Sectors is high.
Texture traffic is decomposed in two ways: by memory, and by surface format.
All %-of-Sectors values are relative to the total sectors processed by the L1TEX cache.  The sum of "LSU by Mem" and "TEX by Mem" will add up to 100% in each column.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="L1TEX-Sector-Traffic">
          <thead>
            <tr>
              <th colspan="4" class="la tablename">L1TEX Sector Traffic</th>
              <th colspan="4" class="ca">Per-Memory Space %-of-Total</th>
              <th colspan="4" class="ca">Per-Op %-of-total</th>
            </tr>
            <tr>
              <th class="la">Pipe Breakdown</th>
              <th class="ra">Hit-Rate%</th>
              <th class="la">%-of-Sectors</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="la">Memory Space</th>
              <th class="ra">Hit-Rate%</th>
              <th class="la">%-of-Sectors</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="la">Op</th>
              <th class="ra">Hit-Rate%</th>
              <th class="la">%-of-Sectors</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_l1tex_traffic">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_L1TexTrafficBreakdown(tbody) {
        class Node {
          constructor(label, ratioAddends, children) {
            this.label = label;
            this.ratioAddends = ratioAddends;
            this.children = children;
            this.rowspan = Math.max(1, children.reduce((acc, child) => acc + child.rowspan, 0));
          }
        }
        let topLevelNodes = [
''']
        for node in gen.nodes:
            jsfunc_lines += [node.to_javascript(5)]
        jsfunc_lines += ['''
        ];
        function generateTableRecursive(trow_init, nodes) {
          let trow = trow_init;
          for (let ii = 0; ii < nodes.length; ++ii) {
            if (ii > 0) {
              trow = document.createElement('tr');
              tbody.appendChild(trow);
            }
            let node = nodes[ii];
            let sectorPct    = node.ratioAddends.reduce((acc, ratioAddend) => acc + getRatioPct(ratioAddend), 0);
            let sectorHitPct = node.ratioAddends.reduce((acc, ratioAddend) => acc + getRatioPct(ratioAddend + '_lookup_hit'), 0);
            let hitRate = safeDiv(sectorHitPct, sectorPct) * 100;
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"la subhdr"}, node.label, passThrough=true);
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"ra"}, format_pct(hitRate));
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"ra"}, format_pct(sectorPct));
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"la comp"}, toBarChart(sectorPct, '█'));

            generateTableRecursive(trow, node.children);
          }
        }
        let trowFirst = document.createElement('tr');
        tbody.appendChild(trowFirst);
        generateTableRecursive(trowFirst, topLevelNodes);
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_L1TexTrafficBreakdown(document.getElementById('tbody_l1tex_traffic'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable

class L2TrafficBreakdownGenerator:
    class Node:
        def __init__(node, label, ratio_addends, children):
            node.label = label
            node.ratio_addends = ratio_addends
            if len(node.ratio_addends) == 0:
                for child in children:
                    node.ratio_addends.extend(child.ratio_addends)
            node.children = children

        def to_javascript(node, base_indent):
            def to_js_recursive(cur_node, indent):
                str = ''
                str += '  ' * indent
                str += "new Node('{cur_node.label}', [".format(cur_node=cur_node)
                for ratio_addend in cur_node.ratio_addends:
                    str += "'" + ratio_addend + "', "
                if len(cur_node.children):
                    str += '], [\n'
                    for child in cur_node.children:
                        str += to_js_recursive(child, indent + 1)
                    str += '  ' * indent + ']),\n'
                else:
                    str += '], []),\n'
                return str
            return to_js_recursive(node, base_indent)

    def get_required_ratios(gen):
        # note: this function is factored out, in case we need to split the table, or define per-architecture metrics
        def _get_required_ratios_recursive(nodes):
            required_ratios = []
            for node in nodes:
                required_ratios += node.ratio_addends
                required_ratios += [ratio_addend + '_lookup_hit' for ratio_addend in node.ratio_addends]
                required_ratios += _get_required_ratios_recursive(node.children)
            return required_ratios
        required_ratios = _get_required_ratios_recursive(gen.nodes)
        return required_ratios


    def __init__(gen, show_generic_workflow=False):
        gen.name = ''
        gen.table_id = ''
        gen.column_names = []
        gen.nodes = []
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        # only first table should have the workflow
        gen.workflow = ''
        if show_generic_workflow:
            gen.workflow = '''L2 Sector Traffic:
Determine which units and operations are incurring the most L2 bandwidth, and which destinations are being addressed.
All %-of-Sectors values are relative to the total sectors processed by the L2 cache.
The term "sector" refers to a 32 byte portion of a cacheline.
'''

    @property
    def html(gen):
        html_lines = ['''
        <table style="display: inline-block; border: 1px solid;" id="{table_id}">
          <thead>
            <tr>'''.format(table_id=gen.table_id)]
        for index, column in enumerate(gen.column_names):
          if not index:
            html_lines += ['''
              <th colspan="2" class="la tablename">L2 Sector Traffic by {column[0]}</th>
              <th colspan="2" class="ca">%-of-Sectors</th>'''.format(column=column)]
          else:
            html_lines += ['''
              <th colspan="2" class="ca">{column[0]}</th>
              <th colspan="2" class="ca">%-of-Sectors</th>
'''.format(column=column)]
        html_lines += ['''
            </tr>
            <tr>''']
        for index, column in enumerate(gen.column_names):
          html_lines += ['''
              <th class="la">{column[1]}</th>
              <th class="ra">Hit-Rate%</th>
              <th class="ca">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>'''.format(column=column)]
        html_lines += ['''
            </tr>
          </thead>
          <tbody id="tbody_{table_id_us}">
          </tbody>
        </table>
'''.format(table_id_us=gen.table_id.replace('-', '_').lower())]
        return ''.join(html_lines)

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_{name}(tbody) {{
        class Node {{
          constructor(label, ratioAddends, children) {{
            this.label = label;
            this.ratioAddends = ratioAddends;
            this.children = children;
            this.sectorPct    = this.ratioAddends.reduce((acc, ratioAddend) => acc + getRatioPct(ratioAddend), 0);
            this.sectorHitPct = this.ratioAddends.reduce((acc, ratioAddend) => acc + getRatioPct(ratioAddend + '_lookup_hit'), 0);
            this.rowspan = Math.max(1, (this.sectorPct == 0.0 ? 1 : children.reduce((acc, child) => acc + child.rowspan, 0)));
            this.depth = children.reduce((acc, child) => Math.max(acc, child.depth), 0) + 1;
          }}
        }}
        let topLevelNodes = [
'''.format(name=gen.name)]
        for node in gen.nodes:
            jsfunc_lines += [node.to_javascript(5)]
        jsfunc_lines += ['''
        ];
        function generateTableRecursive(trow_init, nodes) {
          let trow = trow_init;
          for (let ii = 0; ii < nodes.length; ++ii) {
            if (ii > 0) {
              trow = document.createElement('tr');
              tbody.appendChild(trow);
            }
            let node = nodes[ii];
            let hitRate = safeDiv(node.sectorHitPct, node.sectorPct) * 100;
  
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"la subhdr"}, node.label, passThrough=true);
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"ra"}, format_pct(hitRate));
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"ra"}, format_pct(node.sectorPct));
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"la comp"}, toBarChart(node.sectorPct, '█'));

            if (node.sectorPct > 0.0) {
              generateTableRecursive(trow, node.children);
            }
            else {
              for (let jj = 1; jj < node.depth; ++jj) {
                addCellAttr(trow, {'class':"la subhdr"}, '');
                addCellAttr(trow, {'class':"ra"}, '');
                addCellAttr(trow, {'class':"ra"}, '');
                addCellAttr(trow, {'class':"la comp"}, '');
              }
            }
          }
        }
        let trowFirst = document.createElement('tr');
        tbody.appendChild(trowFirst);
        generateTableRecursive(trowFirst, topLevelNodes);
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_{name}(document.getElementById('tbody_{table_id}'));'''.format(name=gen.name, table_id=gen.table_id.replace('-', '_').lower())

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class PrimitiveDataflowGenerator:
    class Row:
        def __init__(row, stage, input_prims, input_verts, input_attrs, culled_prims, output_prims, output_verts, output_attrs):
            row.stage = stage
            row.input_prims = input_prims
            row.input_verts = input_verts
            row.input_attrs = input_attrs
            row.culled_prims = culled_prims
            row.output_prims = output_prims
            row.output_verts = output_verts
            row.output_attrs = output_attrs

    def __init__(gen):
        gen.name = 'PrimitiveDataflow'
        gen.rows = [
            #        Pipeline Stage                     , Input Primitives          , Input Vertices                        , Input Attributes          , Culled Primitives                     , Output Primitives                 , Output Vertices               , Output Attributes
            gen.Row('Primitive Distributor'             , 'pda__input_prims'        , 'pda__input_verts'                    , 'NotAvailable'            , 'NotApplicable'                       , 'NotApplicable'                   , 'NotApplicable'               , 'NotApplicable'               ),
            gen.Row('Vertex Shader'                     , 'NotApplicable'           , 'sm__threads_launched_shader_vs'      , 'NotAvailable'            , 'NotApplicable'                       , 'NotApplicable'                   , 'NotApplicable'               , 'NotApplicable'               ),
            gen.Row('Tess. Control Shader'              , 'NotApplicable'           , 'sm__threads_launched_shader_tcs'     , 'NotAvailable'            , 'NotApplicable'                       , 'NotAvailable'                    , 'NotAvailable'                , 'NotAvailable'                ),
            gen.Row('Tess. Eval Shader'                 , 'NotAvailable'            , 'sm__threads_launched_shader_tes'     , 'NotAvailable'            , 'NotApplicable'                       , 'NotAvailable'                    , 'NotAvailable'                , 'NotAvailable'                ),
            gen.Row('Geometry Shader'                   , 'NotAvailable'            , 'sm__threads_launched_shader_gs'      , 'NotAvailable'            , 'NotApplicable'                       , 'NotAvailable'                    , 'NotAvailable'                , 'NotAvailable'                ),
            gen.Row('Stream (Transform Feedback)'       , 'NotApplicable'           , 'NotApplicable'                       , 'NotApplicable'           , 'NotApplicable'                       , 'pes__stream_output_prims'        , 'pes__stream_output_verts'    , 'pes__stream_output_attrs'    ),
            gen.Row('Primitive Assembly Clip'           , 'vpc__clip_input_prims'   , 'NotAvailable'                        , 'NotAvailable'            , 'vpc__clip_input_prims_op_clipped'    , 'vpc__clip_output_prims'          , 'NotApplicable'               , 'NotApplicable'               ), # vpc__clip_output_verts is INT-only
            gen.Row('Primitive Assembly Cull'           , 'vpc__cull_input_prims'   , 'NotAvailable'                        , 'NotAvailable'            , 'vpc__cull_input_prims_op_culled'     , 'vpc__cull_input_prims_op_passed' , 'NotApplicable'               , 'NotApplicable'               ), # add vpc__cull_input_verts & vpc__cull_input_attrs once they're available
            gen.Row('Primitive Assembly (All Stages)'   , 'vpc__input_prims'        , 'NotApplicable'                       , 'NotApplicable'           , 'NotApplicable'                       , 'vpc__output_prims'               , 'NotApplicable'               , 'vpc__output_attrs'           ), # vpc__output_verts is INT-only
        ]
        gen.required_counters = [
            'pda__input_prims',
            'pda__input_verts',
            'pes__stream_output_attrs',
            'pes__stream_output_prims',
            'pes__stream_output_verts',
            'sm__threads_launched_shader_gs',
            'sm__threads_launched_shader_tcs',
            'sm__threads_launched_shader_tes',
            'sm__threads_launched_shader_vs',
            'vpc__clip_input_prims',
            'vpc__clip_input_prims_op_clipped',
            'vpc__clip_output_prims',
            'vpc__cull_input_prims',
            'vpc__cull_input_prims_op_culled',
            'vpc__cull_input_prims_op_passed',
            'vpc__input_prims',
            'vpc__output_attrs',
            'vpc__output_prims',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''Primitive Data Flow:
This table shows the creation, destruction, and processing of geometry data through the 3D graphics pipeline, before reaching the rasterizer.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Primitive-Data-Flow">
          <thead>
            <tr>
              <th colspan="10" class="ca tablename">Primitive Data Flow</th>
            </tr>
            <tr>
              <th class="la">Pipeline Stage</th>
              <th class="ra">Input Primitives</th>
              <th class="ra">Input Vertices</th>
              <th class="ra">Input Attributes</th>
              <th class="ra">Culled Primitives</th>
              <th class="ra">Output Primitives</th>
              <th class="ra">Output Vertices</th>
              <th class="ra">Output Attributes</th>
            </tr>
          </thead>
          <tbody id="tbody_primitive_data_flow">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_PrimitiveDataflow(tbody) {
        class Row {
          constructor(stage, inputPrims, inputVerts, inputAttrs, culledPrims, outputPrims, outputVerts, outputAttrs) {
            let getCounterValueIfDefined = function(counterName){
              if (counterName == 'NotApplicable') {
                return new NotApplicable;
              } else if (counterName == 'NotAvailable') {
                return new NotAvailable;
              } else {
                return getCounterValue(counterName, 'sum');
              }
            };
            this.stage = stage;
            this.inputPrims  = getCounterValueIfDefined(inputPrims);
            this.inputVerts  = getCounterValueIfDefined(inputVerts);
            this.inputAttrs  = getCounterValueIfDefined(inputAttrs);
            this.culledPrims = getCounterValueIfDefined(culledPrims);
            this.outputPrims = getCounterValueIfDefined(outputPrims);
            this.outputVerts = getCounterValueIfDefined(outputVerts);
            this.outputAttrs = getCounterValueIfDefined(outputAttrs);
          }
        }
        let rows = [
''']
        for row in gen.rows:
            qstage = "'{row.stage}'".format(row=row)
            qinput_prims = "'{row.input_prims}'".format(row=row)
            qinput_verts = "'{row.input_verts}'".format(row=row)
            qinput_attrs = "'{row.input_attrs}'".format(row=row)
            qculled_prims = "'{row.culled_prims}'".format(row=row)
            qoutput_prims = "'{row.output_prims}'".format(row=row)
            qoutput_verts = "'{row.output_verts}'".format(row=row)
            qoutput_attrs = "'{row.output_attrs}'".format(row=row)
            jsfunc_lines += ["          new Row({qstage:<40}, {qinput_prims:<30}, {qinput_verts:<40}, {qinput_attrs:<30}, {qculled_prims:<40}, {qoutput_prims:<40}, {qoutput_verts:<30}, {qoutput_attrs:<30}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.stage);
          addCellSimple(trow, "ra", format_sum(row.inputPrims));
          addCellSimple(trow, "ra", format_sum(row.inputVerts));
          addCellSimple(trow, "ra", format_sum(row.inputAttrs));
          addCellSimple(trow, "ra", format_sum(row.culledPrims));
          addCellSimple(trow, "ra", format_sum(row.outputPrims));
          addCellSimple(trow, "ra", format_sum(row.outputVerts));
          addCellSimple(trow, "ra", format_sum(row.outputAttrs));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_PrimitiveDataflow(document.getElementById('tbody_primitive_data_flow'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class RasterDataflowGenerator:
    class Row:
        def __init__(row, stage, pixelsInput, pixelsKilled, pixelsOutput, samplesInput, samplesKilled, samplesOutput):
            row.stage = stage
            # pixels
            row.pixelsInput = pixelsInput
            row.pixelsKilled = pixelsKilled
            row.pixelsOutput = pixelsOutput
            # samples
            row.samplesInput = samplesInput
            row.samplesKilled = samplesKilled
            row.samplesOutput = samplesOutput

    def __init__(gen):
        gen.name = 'RasterDataflow'
        gen.counter_def = r'''
        let notApplicable = new NotApplicable;
        let notAvailable = new NotAvailable;
        // zcull
        const zcull_pixels_input = getCounterValue('raster__zcull_input_samples', 'sum');
        const zcull_pixels_killed = getCounterValue('raster__zcull_input_samples_op_rejected', 'sum');
        const zcull_pixels_output = getCounterValue('raster__zcull_input_samples_op_accepted', 'sum');

        // prop input
        const prop_input_pixels_input = getCounterValue('prop__input_pixels_type_3d_realtime', 'sum');
        const prop_input_samples_input = notAvailable;

        // prop earlyz
        const prop_earlyz_pixels_input = notAvailable;
        const prop_earlyz_pixels_killed = getCounterValue('prop__earlyz_killed_pixels_realtime', 'sum');
        const prop_earlyz_pixels_output = notAvailable;
        const prop_earlyz_samples_input = getCounterValue('prop__earlyz_input_samples', 'sum');
        const prop_earlyz_samples_killed = getCounterValue('prop__earlyz_killed_samples', 'sum');
        const prop_earlyz_samples_output = getCounterValue('prop__earlyz_output_samples', 'sum');

        // pixel shader
        const pixel_shader_pixels_input = getCounterValue('sm__threads_launched_shader_ps_killmask_off', 'sum');
        const pixel_shader_pixels_killed = notAvailable;
        const pixel_shader_pixels_output = notAvailable;

        // prop latez
        const prop_latez_pixels_input = notAvailable;
        const prop_latez_pixels_killed = notAvailable;
        const prop_latez_pixels_output = notAvailable;
        const prop_latez_samples_input = notAvailable;
        const prop_latez_samples_killed = getCounterValue('prop__latez_killed_samples', 'sum');
        const prop_latez_samples_output = getCounterValue('prop__latez_output_samples', 'sum');

        // zrop
        const zrop_pixels_input = {gen.zrop_pixels_input};
        const zrop_samples_input = notAvailable;
        const zrop_samples_output = notAvailable;

        // prop color

        // crop
        const crop_pixels_input = {gen.crop_pixels_input};
'''
        gen.zrop_pixels_input = 'notAvailable'
        gen.crop_pixels_input = 'notAvailable'
        gen.rows = [
            #       stage                           , pixelsInput                   , pixelsKilled                  , pixelsOutput                  , samplesInput                  , samplesKilled                 , samplesOutput
            gen.Row('ZCULL'                         , "zcull_pixels_input"          , "zcull_pixels_killed"         , "zcull_pixels_output"         , "notApplicable"               , "notApplicable"               , "notApplicable"               ),
            gen.Row('PROP Input'                    , "prop_input_pixels_input"     , "notApplicable"               , "notApplicable"               , "prop_input_samples_input"    , "notApplicable"               , "notApplicable"               ),
            gen.Row('PROP EarlyZ'                   , "prop_earlyz_pixels_input"    , "prop_earlyz_pixels_killed"   , "prop_earlyz_pixels_output"   , "prop_earlyz_samples_input"   , "prop_earlyz_samples_killed"  , "prop_earlyz_samples_output"  ),
            gen.Row('Pixel Shader(EarlyZ + LateZ)'  , "pixel_shader_pixels_input"   , "notAvailable"                , "notAvailable"                , "notApplicable"               , "notApplicable"               , "notApplicable"               ),
            gen.Row('PROP LateZ'                    , "notAvailable"                , "prop_latez_pixels_killed"    , "prop_latez_pixels_output"    , "prop_latez_samples_input"    , "prop_latez_samples_killed"   , "prop_latez_samples_output"   ),
            gen.Row('ZROP'                          , "zrop_pixels_input"           , "notApplicable"               , "notApplicable"               , "zrop_samples_input"          , "notAvailable"                , "zrop_samples_output"         ),
            gen.Row('PROP Color'                    , "notAvailable"                , "notAvailable"                , "notAvailable"                , "notAvailable"                , "notAvailable"                , "notAvailable"                ),
            gen.Row('CROP'                          , "crop_pixels_input"           , "notAvailable"                , "notAvailable"                , "notAvailable"                , "notAvailable"                , "notAvailable"                ),
        ]
        gen.required_counters = [
            'prop__earlyz_input_samples',
            'prop__earlyz_killed_pixels_realtime',
            'prop__earlyz_killed_samples',
            'prop__earlyz_output_samples',
            'prop__input_pixels_type_3d_realtime',
            'prop__latez_killed_samples',
            'prop__latez_output_samples',
            'raster__zcull_input_samples',
            'raster__zcull_input_samples_op_accepted',
            'raster__zcull_input_samples_op_rejected',
            'sm__threads_launched_shader_ps_killmask_off',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''Raster Data Flow:
This table shows the creation, destruction, and processing of pixels and samples (MSAA) through the 3D graphics pipeline.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Raster-Data-Flow">
          <thead>
            <tr>
              <th colspan="1" class="ca tablename">Raster Data Flow</th>
              <th colspan="3" class="ca">Pixels</th>
              <th colspan="3" class="ca">Samples</th>
            </tr>
            <tr>
              <th class="la">Pipeline Stage</th>
              <th class="ra">Pixels In</th>
              <th class="ra">Pixels Killed</th>
              <th class="ra">Pixels Out</th>
              <th class="ra">Samples In</th>
              <th class="ra">Samples Killed</th>
              <th class="ra">Samples Out</th>
            </tr>
          </thead>
          <tbody id="tbody_raster_data_flow">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_RasterDataflow(tbody) {
        class Row {
          constructor(stage, pixelsInput, pixelsKilled, pixelsOutput, samplesInput, samplesKilled, samplesOutput) {
            this.stage = stage;
            this.pixelsInput = pixelsInput;
            this.pixelsKilled = pixelsKilled;
            this.pixelsOutput = pixelsOutput;
            this.samplesInput = samplesInput;
            this.samplesKilled = samplesKilled;
            this.samplesOutput = samplesOutput;
          }
        }
''']
        jsfunc_lines += [gen.counter_def.format(gen=gen)]
        jsfunc_lines += ['''
        let rows = [
''']
        for row in gen.rows:
            qstage = "'{row.stage}'".format(row=row)
            jsfunc_lines += ["          new Row({qstage:<30}, {row.pixelsInput:<40}, {row.pixelsKilled:<40}, {row.pixelsOutput:<40}, {row.samplesInput:<40}, {row.samplesKilled:<40}, {row.samplesOutput:<40},),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.stage);
          addCellSimple(trow, "ra", format_sum(row.pixelsInput));
          addCellSimple(trow, "ra", format_sum(row.pixelsKilled));
          addCellSimple(trow, "ra", format_sum(row.pixelsOutput));
          addCellSimple(trow, "ra", format_sum(row.samplesInput));
          addCellSimple(trow, "ra", format_sum(row.samplesKilled));
          addCellSimple(trow, "ra", format_sum(row.samplesOutput));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_RasterDataflow(document.getElementById('tbody_raster_data_flow'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class SmInstExecutedGenerator:
    class Pipe:
        def __init__(pipe, name, description, hasThreadInstExecuted=False):
            pipe.name = name
            pipe.description = description

            if name == 'total':
                pipe.instExecuted = 'sm__inst_executed'
                pipe.threadInstExecuted = 'smsp__thread_inst_executed_pred_on'
            else:
                pipe.instExecuted = 'sm__inst_executed_pipe_' + name
                pipe.threadInstExecuted = 'sm__thread_inst_executed_pipe_{}_pred_on'.format(name) if hasThreadInstExecuted else ''

        def get_counter_names(pipe):
            counter_names = []
            if pipe.instExecuted:
                counter_names += [pipe.instExecuted]
            if pipe.threadInstExecuted:
                counter_names += [pipe.threadInstExecuted]
            return counter_names

    def __init__(gen):
        gen.name = 'SmInstExecuted'
        gen.pipes = []
        gen.required_counters = [
            'sm__warps_launched',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SM Instruction Execution:
This table displays shader instruction counts (total and per-pipeline).  Low Active Thread% implies either heavy branch divergence (threads taking separate if/else code paths), or low numbers of launched threads per warp.
Cross-check thread launch statistics in the <a href="#SM-Shader-Execution">SM Shader Execution</a> table.
'''

    @property
    def html(gen):
        return r'''
        <table style="display: inline-block; border: 1px solid;" id="SM-Instruction-Execution">
          <thead>
            <tr>
              <th colspan="2" class="la tablename">SM Instruction Execution</th>
              <th colspan="2" class="ca">GPU Total</th>
              <th colspan="2" class="ca">Threads Per Warp (Max 32)</th>
              <th colspan="2" class="ca">Avg Executed Per Warp</th>
            </tr>
            <tr>
              <th class="la">Description</th>
              <th class="la">Pipeline</th>
              <th class="la">Warp Inst</th>
              <th class="la">Thread Inst</th>
              <th class="la">Avg Active Threads</th>
              <th class="la">Active Thread%</th>
              <th class="la">Warp Inst</th>
              <th class="la">Thread Inst</th>
            </tr>
          </thead>
          <tbody id="tbody_sm_inst_executed">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_SmInstExecuted(tbody) {
        class Pipe {
          constructor(name, description, instExecuted, threadInstExecuted) {
            this.name = name;
            this.description = description;
            this.instExecuted = instExecuted
            this.threadInstExecuted = threadInstExecuted
          }
        }

        let pipes = [
''']
        for pipe in gen.pipes:
            qname = "'{pipe.name}'".format(pipe=pipe)
            qdesc = "'{pipe.description}'".format(pipe=pipe)
            qinstExecuted = "'{pipe.instExecuted}'".format(pipe=pipe)
            qthreadInstExecuted = "'{pipe.threadInstExecuted}'".format(pipe=pipe)
            jsfunc_lines += ["          new Pipe({qname:<20}, {qdesc:<60}, {qinstExecuted:<40}, {qthreadInstExecuted:<50}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        const warpsLaunched = getCounterValue('sm__warps_launched', 'sum');

        for (const pipe of pipes) {
          const warpCounter   = pipe.instExecuted;
          const threadCounter = pipe.threadInstExecuted;

          const warpInst = getCounterValue(warpCounter, 'sum');
          const warpInstPerWarpLaunched = safeDiv(warpInst, warpsLaunched);
          threadInst = new NotAvailable;
          threadsPerWarp = new NotAvailable;
          threadsPerWarpPct = new NotAvailable;
          threadInstPerWarpLaunched = new NotAvailable;
          if (threadCounter != '') {
            threadInst = getCounterValue(threadCounter, 'sum');
            threadsPerWarp = safeDiv(threadInst, warpInst);
            threadsPerWarpPct = 100 * threadsPerWarp / 32;
            threadInstPerWarpLaunched = safeDiv(threadInst, warpsLaunched);
          }

          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", pipe.description);
          addCellSimple(trow, "la subhdr", pipe.name);
          addCellSimple(trow, "ra", format_sum(warpInst));
          addCellSimple(trow, "ra", format_sum(threadInst));
          addCellSimple(trow, "ra", format_avg(threadsPerWarp));
          addCellSimple(trow, "ra", format_pct(threadsPerWarpPct));
          addCellSimple(trow, "ra", format_avg(warpInstPerWarpLaunched));
          addCellSimple(trow, "ra", format_avg(threadInstPerWarpLaunched));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_SmInstExecuted(document.getElementById('tbody_sm_inst_executed'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class WarpIssueStallsGenerator:
    class Row:
        def __init__(row, desc, name, counterName):
            row.desc = desc
            row.name = name
            row.counterName = counterName

    def __init__(gen):
        gen.name = 'WarpIssueStalls'
        gen.rows = [
            gen.Row('All; sum of all other reasons (waiting + issuing)'                                             , 'Total'                 , 'smsp__warps_active'                          ),
            gen.Row('Waiting for a synchronization barrier'                                                         , 'Barrier'               , 'smsp__warps_issue_stalled_barrier'           ),
            gen.Row('Waiting for dynamic branch target computation and warp PC update'                              , 'Branch Resolving'      , 'smsp__warps_issue_stalled_branch_resolving'  ),
            gen.Row('Waiting for a busy pipeline at the dispatch stage; wasted scheduler cycle'                     , 'Dispatch Stall'        , 'smsp__warps_issue_stalled_dispatch_stall'    ),
            gen.Row('Waiting for memory instructions or pixout to complete after warp EXIT'                         , 'Drain after Exit'      , 'smsp__warps_issue_stalled_drain'             ),
            gen.Row('Waiting for IMC (immediate constant cache) - for static constant addresses'                    , 'IMC Miss'              , 'smsp__warps_issue_stalled_imc_miss'          ),
            gen.Row('Waiting for free space in the LG Input FIFO: local/global instruction issue to LSU'            , 'LG Throttle'           , 'smsp__warps_issue_stalled_lg_throttle'       ),
            gen.Row('Waiting on variable latency dependency: LSU (local/global), TEX (texture, surface)'            , 'Long Scoreboard'       , 'smsp__warps_issue_stalled_long_scoreboard'   ),
            gen.Row('Waiting for a math pipe to become available'                                                   , 'Math Pipe Throttle'    , 'smsp__warps_issue_stalled_math_pipe_throttle'),
            gen.Row('Waiting for a memory barrier'                                                                  , 'Memory Barrier'        , 'smsp__warps_issue_stalled_membar'            ),
            gen.Row('Waiting for free space in the MIO Input FIFO: ADU, CBU, FP64, LSU (not local/global), IPA'     , 'MIO Throttle'          , 'smsp__warps_issue_stalled_mio_throttle'      ),
            gen.Row('Waiting for a miscellaneous reason (should be rare)'                                           , 'Misc'                  , 'smsp__warps_issue_stalled_misc'              ),
            gen.Row('Waiting for instruction fetch, or instruction cache miss'                                      , 'No Instruction'        , 'smsp__warps_issue_stalled_no_instruction'    ),
            gen.Row('Waiting for a scheduler cycle, from an otherwise ready-to-issue warp'                          , 'Not Selected'          , 'smsp__warps_issue_stalled_not_selected'      ),
            gen.Row('Issuing an instruction'                                                                        , 'Selected'              , 'smsp__warps_issue_stalled_selected'          ),
            gen.Row('Waiting on variable latency dependency: XU, FP64, LSU (not local/global), ADU, CBU, LDC/IDC'   , 'Short Scoreboard'      , 'smsp__warps_issue_stalled_short_scoreboard'  ),
            gen.Row('Waiting for a nanosleep timer to expire'                                                       , 'Sleeping'              , 'smsp__warps_issue_stalled_sleeping'          ),
            gen.Row('Waiting for free space in the TEX Input FIFO: TEX pipe texture or surface instructions'        , 'TEX Throttle'          , 'smsp__warps_issue_stalled_tex_throttle'      ),
            gen.Row('Waiting on a fixed latency dependency'                                                         , 'Shadow Pipe Throttle'  , 'smsp__warps_issue_stalled_wait'              ),
        ]
        gen.required_counters = [
            'smsp__inst_executed',
            'smsp__warps_launched',
            'smsp__warps_active',
            'smsp__warps_issue_stalled_barrier',
            'smsp__warps_issue_stalled_branch_resolving',
            'smsp__warps_issue_stalled_dispatch_stall',
            'smsp__warps_issue_stalled_drain',
            'smsp__warps_issue_stalled_imc_miss',
            'smsp__warps_issue_stalled_lg_throttle',
            'smsp__warps_issue_stalled_long_scoreboard',
            'smsp__warps_issue_stalled_math_pipe_throttle',
            'smsp__warps_issue_stalled_membar',
            'smsp__warps_issue_stalled_mio_throttle',
            'smsp__warps_issue_stalled_misc',
            'smsp__warps_issue_stalled_no_instruction',
            'smsp__warps_issue_stalled_not_selected',
            'smsp__warps_issue_stalled_selected',
            'smsp__warps_issue_stalled_short_scoreboard',
            'smsp__warps_issue_stalled_sleeping',
            'smsp__warps_issue_stalled_tex_throttle',
            'smsp__warps_issue_stalled_wait',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SM Warp Issue Stall Reasons:
This table shows which types of instructions contribute most to shader warp execution time.
For large workloads where the <a href="#SM-Shader-Execution">total number of warps launched</a> can fill every SM's warp slots, reducing the average warp latency will usually improve wall clock time.
The table is sorted; the first row always has the highest contribution.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="SM-Warp-Issue-Stall-Reasons">
          <thead>
            <tr>
              <th colspan="2" class="la tablename">SM Warp Issue Stall Reasons</th>
              <th colspan="3" class="ca">Avg Warp Latency</th>
              <th colspan="1" class="ca">Avg Inst Latency</th>
            </tr>
            <tr>
              <th class="la">Latency Reason</th>
              <th class="la">Name</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">Cycles</th>
              <th class="ra">Cycles</th>
            </tr>
          </thead>
          <tbody id="tbody_warp_issue_stalls">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_WarpIssueStalls(tbody) {
        const activeWarps = getCounterValue('smsp__warps_active', 'avg');
        const warpsLaunched = getCounterValue('smsp__warps_launched', 'avg');
        const instExecuted = getCounterValue('smsp__inst_executed', 'avg');

        class Row {
          constructor(desc, name, counterName) {
            const counterValue = getCounterValue(counterName, 'avg');
            this.desc = desc;
            this.name = name;
            this.pct = 100 * safeDiv(counterValue, activeWarps);
            this.warpLatency = safeDiv(counterValue, warpsLaunched);
            this.instLatency = safeDiv(counterValue, instExecuted);
          }
        }

        let rows = [];
''']
        for row in gen.rows:
            qdesc = "'{row.desc}'".format(row=row)
            qname = "'{row.name}'".format(row=row)
            qcounter_name = "'{row.counterName}'".format(row=row)
            jsfunc_lines += ["        rows.push(new Row({qdesc:<80}, {qname:<30}, {qcounter_name:<50}));\n".format(**locals())]
        jsfunc_lines += ['''
        rows.sort((lhsrow, rhsrow) => compareNumbers(rhsrow.pct, lhsrow.pct)); // sort in descending order

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.desc, passThrough=true);
          addCellSimple(trow, "la subhdr", row.name, passThrough=true);
          addCellSimple(trow, "ra", format_pct(row.pct));
          addCellSimple(trow, "la comp", toBarChart(row.pct, '█'));
          addCellSimple(trow, "ra", format_avg(row.warpLatency));
          addCellSimple(trow, "ra", format_avg(row.instLatency));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_WarpIssueStalls(document.getElementById('tbody_warp_issue_stalls'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class SmShaderExecutionGenerator:
    class Row:
        def __init__(row, stageName, counterSuffix):
            row.stageName = stageName
            row.counterSuffix = counterSuffix

    def __init__(gen):
        gen.name = 'SmShaderExecution'
        gen.rows = [
            gen.Row('Total'                 , ''            ),
            gen.Row('Vertex Shader'         , '_shader_vs'  ),
            gen.Row('Tess Control Shader'   , '_shader_tcs' ),
            gen.Row('Tess Eval Shader'      , '_shader_tes' ),
            gen.Row('Geometry Shader'       , '_shader_gs'  ),
            gen.Row('Pixel Shader'          , '_shader_ps'  ),
            gen.Row('Compute Shader'        , '_shader_cs'  ),
        ]
        gen.required_counters = []
        for row in gen.rows:
            gen.required_counters.append('smsp__inst_executed' + row.counterSuffix)
            gen.required_counters.append('sm__threads_launched' + row.counterSuffix)
            gen.required_counters.append('sm__warps_launched' + row.counterSuffix)
        gen.required_counters += [
            'sm__inst_executed',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SM Shader Execution:
Determine which shader stages executed the most instructions, and the launched thread efficiency (Threads/Warp).
Threads Launched values are equivalent to "shader invocations" in the D3D/GL/Vulkan specifications, except for Pixel Shaders where helper threads are also counted.
All values are presented as GPU totals.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="SM-Shader-Execution">
          <thead>
            <tr>
              <th colspan="1" class="la tablename">SM Shader Execution</th>
              <th colspan="3" class="ca">Shader Launch (GPU Total)</th>
              <th colspan="2" class="ca">Warp Instructions (GPU Total)</th>
              <th colspan="2" class="ca">%-of-Total-Warp-Inst</th>
            </tr>
            <tr>
              <th class="la">Shader Stage</th>
              <th class="la">Warps Launched</th>
              <th class="la">Threads Launched</th>
              <th class="la">Threads/Warp</th>
              <th class="ra">Warp Inst Executed</th>
              <th class="ra">Inst/Warp</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_sm_shader_execution">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_SmShaderExecution(tbody) {
        const warpsLaunched = getCounterValue('sm__warps_launched', 'sum');
        const warpInstExecuted = getCounterValue('sm__inst_executed', 'sum');

        class Row {
          constructor(stageName, counterSuffix) {
            this.stageName = stageName;
            this.launchedWarps = getCounterValue('sm__warps_launched' + counterSuffix, 'sum');
            this.launchedThreads = getCounterValue('sm__threads_launched' + counterSuffix, 'sum');
            this.launchedThreadsPerWarp = safeDiv(this.launchedThreads, this.launchedWarps);
            this.instWarp = getCounterValue('smsp__inst_executed' + counterSuffix, 'sum');
            this.instPerWarp = safeDiv(this.instWarp, this.launchedWarps);
            this.pct = safeDiv(this.instWarp, warpInstExecuted) * 100;
          }
        }

        let rows = [];
''']
        for row in gen.rows:
            qstage_name = "'{row.stageName}'".format(row=row)
            qcounter_suffix = "'{row.counterSuffix}'".format(row=row)
            jsfunc_lines += ["        rows.push(new Row({qstage_name:<30}, {qcounter_suffix:<20}));\n".format(**locals())]
        jsfunc_lines += ['''
        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.stageName);
          addCellSimple(trow, "ra", format_sum(row.launchedWarps));
          addCellSimple(trow, "ra", format_sum(row.launchedThreads));
          addCellSimple(trow, "ra", format_avg(row.launchedThreadsPerWarp));
          addCellSimple(trow, "ra", format_sum(row.instWarp));
          addCellSimple(trow, "ra", format_avg(row.instPerWarp));
          addCellSimple(trow, "ra", format_pct(row.pct));
          addCellSimple(trow, "la comp", toBarChart(row.pct, '█'));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_SmShaderExecution(document.getElementById('tbody_sm_shader_execution'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class SmWarpLaunchStallsGenerator:
    class Row:
        def __init__(row, stageName, anyReason, warpAlloc, regAlloc, shmAlloc, otherReasonName, otherReasonMetric):
            row.stageName = stageName
            row.anyReason = anyReason
            row.warpAlloc = warpAlloc
            row.regAlloc = regAlloc
            row.shmAlloc = shmAlloc
            row.otherReasonName = otherReasonName
            row.otherReasonMetric = otherReasonMetric

    def __init__(gen):
        gen.name = 'SmWarpLaunchStalls'
        gen.rows = [
            gen.Row('Vertex Shader'         , 'tpc__warp_launch_cycles_stalled_shader_vs'   , 'NotApplicable'                                                       , 'NotApplicable'                                                           , 'NotApplicable'                                                       , 'NotApplicable'       , 'NotApplicable'                                                           ),
            gen.Row('Tess Control Shader'   , 'tpc__warp_launch_cycles_stalled_shader_tcs'  , 'NotApplicable'                                                       , 'NotApplicable'                                                           , 'NotApplicable'                                                       , 'NotApplicable'       , 'NotApplicable'                                                           ),
            gen.Row('Tess Eval Shader'      , 'tpc__warp_launch_cycles_stalled_shader_tes'  , 'NotApplicable'                                                       , 'NotApplicable'                                                           , 'NotApplicable'                                                       , 'NotApplicable'       , 'NotApplicable'                                                           ),
            gen.Row('Geometry Shader'       , 'tpc__warp_launch_cycles_stalled_shader_gs'   , 'NotApplicable'                                                       , 'NotApplicable'                                                           , 'NotApplicable'                                                       , 'NotApplicable'       , 'NotApplicable'                                                           ),
            gen.Row('VTG Shader'            , 'tpc__warp_launch_cycles_stalled_shader_vtg'  , 'NotApplicable'                                                       , 'NotApplicable'                                                           , 'tpc__pe2sm_vtg_isbe_allocation_cycles_stalled'                       , 'NotApplicable'       , 'NotApplicable'                                                           ), # tpc__warp_launch_cycles_stalled_shader_vtg_reason_warp_allocation & tpc__warp_launch_cycles_stalled_shader_vtg_reason_register_allocation not found
            gen.Row('Pixel Shader'          , 'tpc__warp_launch_cycles_stalled_shader_ps'   , 'tpc__warp_launch_cycles_stalled_shader_ps_reason_warp_allocation'    , 'tpc__warp_launch_cycles_stalled_shader_ps_reason_register_allocation'    , 'tpc__pe2sm_ps_tram_allocation_cycles_stalled'                        , 'Out-of-Order Exit'   , 'tpc__warp_launch_cycles_stalled_shader_ps_reason_ooo_warp_completion'    ),
            gen.Row('Compute Shader'        , 'NotApplicable'                               , 'tpc__warp_launch_cycles_stalled_shader_cs_reason_warp_allocation'    , 'tpc__warp_launch_cycles_stalled_shader_cs_reason_register_allocation'    , 'tpc__warp_launch_cycles_stalled_shader_cs_reason_shmem_allocation'   , 'CTA Alloc'           , 'tpc__warp_launch_cycles_stalled_shader_cs_reason_cta_allocation'         ),
        ]
        gen.required_counters = [
            'tpc__pe2sm_ps_tram_allocation_cycles_stalled',
            'tpc__pe2sm_vtg_isbe_allocation_cycles_stalled',
            'tpc__warp_launch_cycles_stalled_shader_cs_reason_cta_allocation',
            'tpc__warp_launch_cycles_stalled_shader_cs_reason_register_allocation',
            'tpc__warp_launch_cycles_stalled_shader_cs_reason_shmem_allocation',
            'tpc__warp_launch_cycles_stalled_shader_cs_reason_warp_allocation',
            'tpc__warp_launch_cycles_stalled_shader_gs',
            'tpc__warp_launch_cycles_stalled_shader_ps',
            'tpc__warp_launch_cycles_stalled_shader_ps_reason_ooo_warp_completion',
            'tpc__warp_launch_cycles_stalled_shader_ps_reason_register_allocation',
            'tpc__warp_launch_cycles_stalled_shader_ps_reason_warp_allocation',
            'tpc__warp_launch_cycles_stalled_shader_tcs',
            'tpc__warp_launch_cycles_stalled_shader_tes',
            'tpc__warp_launch_cycles_stalled_shader_vs',
            'tpc__warp_launch_cycles_stalled_shader_vtg',
        ]
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SM Warp Can't Launch Reasons:
This table reveals the most heavily used shader resource, and can help to explain low warp occupancy.
If all values are zero, the workload is too small to fill every hardware warp slot, which is a form of starvation.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="SM-Warp-Cant-Launch">
          <thead>
            <tr>
              <th colspan="1" class="la tablename">SM Warp Can't Launch Reasons</th>
              <th colspan="2" class="ca">Any Reason</th>
              <th colspan="2" class="ca">Warp Alloc</th>
              <th colspan="2" class="ca">Register Alloc</th>
              <th colspan="2" class="ca">Attr/ShMem Alloc</th>
              <th colspan="3" class="ca">Other</th>
            </tr>
            <tr>
              <th class="la">Stage</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">Reason</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_warp_launch_stalls">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_SmWarpLaunchStalls(tbody) {
        class Row {
          constructor(stageName, any, warp, reg, shm, otherName, otherMetric) {
            this.stageName = stageName;
            this.warpPct = getCounterPct(warp);
            this.regPct = getCounterPct(reg);
            this.shmPct = getCounterPct(shm);
            this.otherName = otherName;
            this.otherPct = getCounterPct(otherMetric);
            this.anyPct = any ? getCounterPct(any) : Math.max(this.warpPct, this.regPct, this.shmPct, this.otherPct);
          }
        }

        let rows = [];
''']
        for row in gen.rows:
            qstage_name     = "'{row.stageName}'".format(row=row)
            def string_transform(string):
                if string == 'NotApplicable':
                    return 'new NotApplicable'
                elif string == 'NotAvailable':
                    return 'new NotAvailable'
                else:
                    return "'{}'".format(string)
            qany = string_transform(row.anyReason)
            qwarp = string_transform(row.warpAlloc)
            qreg = string_transform(row.regAlloc)
            qshm = string_transform(row.shmAlloc)
            qotherName = string_transform(row.otherReasonName)
            qotherMetric = string_transform(row.otherReasonMetric)
            jsfunc_lines += ["        rows.push(new Row({qstage_name:<30}, {qany:<60}, {qwarp:<80}, {qreg:<80}, {qshm:<80}, {qotherName:<80}, {qotherMetric:<80}));\n".format(**locals())]
        jsfunc_lines += ['''
        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.stageName);
          addCellSimple(trow, "ra", format_pct(row.anyPct));
          addCellSimple(trow, "la comp", toBarChart(row.anyPct, '█'));
          addCellSimple(trow, "ra", format_pct(row.warpPct));
          addCellSimple(trow, "la comp", toBarChart(row.warpPct, '█'));
          addCellSimple(trow, "ra", format_pct(row.regPct));
          addCellSimple(trow, "la comp", toBarChart(row.regPct, '█'));
          addCellSimple(trow, "ra", format_pct(row.shmPct));
          addCellSimple(trow, "la comp", toBarChart(row.shmPct, '█'));
          addCellSimple(trow, "la", row.otherName);
          addCellSimple(trow, "ra", format_pct(row.otherPct));
          addCellSimple(trow, "la comp", toBarChart(row.otherPct, '█'));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_SmWarpLaunchStalls(document.getElementById('tbody_warp_launch_stalls'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class SmResourceUsageGenerator:
    class Row:
        def __init__(row, resource, tot, gfx, vtg, ps, cs):
            row.resource = resource
            row.tot = tot
            row.gfx = gfx
            row.vtg = vtg
            row.ps = ps
            row.cs = cs

    def __init__(gen):
        gen.name = 'SmResourceUsage'
        gen.rows = []
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SM Resource Usage:
This table reveals the quantity of each shader resource used on average, and can help to explain low warp occupancy.  All values are presented per-SM.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="SM-Resource-Usage">
          <thead>
            <tr>
              <th colspan="1" class="la tablename">SM Resource Usage</th>
              <th colspan="3" class="ca">SM Total</th>
              <th colspan="3" class="ca">VTG Shader</th>
              <th colspan="3" class="ca">Pixel Shader</th>
              <th colspan="3" class="ca">Compute Shader</th>
            </tr>
            <tr>
              <th class="la">Stage</th>
              <th class="ra">per-cycle</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">per-cycle</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">per-cycle</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">per-cycle</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_sm_resource_usage">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_SmResourceUsage(tbody) {
        let isValid = function(attr) {
          return !(attr instanceof NotApplicable || attr instanceof NotAvailable);
        };
        class Row {
          constructor(resource, tot, gfx, vtg, ps, cs) {
            this.resource = resource;
            this.tot = tot;
            this.gfx = gfx;
            this.vtg = vtg;
            this.ps = ps;
            this.cs = cs;

            if (isValid(tot)) {
              const totSmFactor = tot.includes('tpc__') ? 0.5 : 1;
              this.totValue = getCounterValue(tot, 'avg.per_cycle_elapsed') * totSmFactor;
              this.totPct = getCounterPct(tot);
            }

            if (isValid(gfx)) {
              const gfxSmFactor = gfx.includes('tpc__') ? 0.5 : 1;
              this.gfxValue = getCounterValue(gfx, 'avg.per_cycle_elapsed') * gfxSmFactor;
              this.gfxPct = getCounterPct(gfx);
            } else if (isValid(vtg) && isValid(ps)) {
              const vtgSmFactor = vtg.includes('tpc__') ? 0.5 : 1;
              this.vtgValue = getCounterValue(vtg, 'avg.per_cycle_elapsed') * vtgSmFactor;
              this.vtgPct = getCounterPct(vtg);
              const psSmFactor = vtg.includes('tpc__') ? 0.5 : 1;
              this.psValue = getCounterValue(ps, 'avg.per_cycle_elapsed') * psSmFactor;
              this.psPct = getCounterPct(ps);
            }

            if (isValid(cs)) {
              const csSmFactor = cs.includes('tpc__') ? 0.5 : 1;
              this.csValue = getCounterValue(cs, 'avg.per_cycle_elapsed') * csSmFactor;
              this.csPct = getCounterPct(cs);
            }
          }
        }

        let rows = [
''']
        for row in gen.rows:
            qresource = "'{row.resource}'".format(row=row)
            def string_transform(string):
                if string == 'NotApplicable':
                    return 'new NotApplicable'
                elif string == 'NotAvailable':
                    return 'new NotAvailable'
                else:
                    return "'{}'".format(string)
            qtot = string_transform(row.tot)
            qgfx = string_transform(row.gfx)
            qvtg = string_transform(row.vtg)
            qps  = string_transform(row.ps)
            qcs  = string_transform(row.cs)
            jsfunc_lines += ["        new Row({qresource:<20}, {qtot:<60}, {qgfx:<60}, {qvtg:<60}, {qps:<60}, {qcs:<60}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, 'la', row.resource);

          if (isValid(row.tot)) {
            addCellSimple(trow, "ra", format_avg(row.totValue));
            addCellSimple(trow, "ra", format_pct(row.totPct));
            addCellSimple(trow, "la comp", toBarChart(row.totPct, '█'));
          } else {
            addCellAttr(trow, {'colspan':3, 'class':"ra"}, row.tot);
          }

          if (isValid(row.gfx)) {
            addCellAttr(trow, { colspan:3, class:"ra"}, '<em>VTG+PS Combined</em> ⮕', true);
            addCellSimple(trow, "ra", format_avg(row.gfxValue));
            addCellSimple(trow, "ra", format_pct(row.gfxPct));
            addCellSimple(trow, "la comp", toBarChart(row.gfxPct, '█'));
          } else if (isValid(row.vtg) && isValid(row.ps)) {
            addCellSimple(trow, "ra", format_avg(row.vtgValue));
            addCellSimple(trow, "ra", format_pct(row.vtgPct));
            addCellSimple(trow, "la comp", toBarChart(row.vtgPct, '█'));
            addCellSimple(trow, "ra", format_avg(row.psValue));
            addCellSimple(trow, "ra", format_pct(row.psPct));
            addCellSimple(trow, "la comp", toBarChart(row.psPct, '█'));
          } else {
            if (row.gfx instanceof NotAvailable) {
              addCellAttr(trow, {'colspan':6, 'class':"ra"}, row.gfx);
            } else if (row.vtg instanceof NotAvailable && row.ps instanceof NotAvailable) {
              addCellAttr(trow, {'colspan':3, 'class':"ra"}, row.vtg);
              addCellAttr(trow, {'colspan':3, 'class':"ra"}, row.ps);
            } else {
              addCellAttr(trow, {'colspan':6, 'class':"ra"}, new NotApplicable);
            }
          }

          if (isValid(row.cs)) {
            addCellSimple(trow, "ra", format_avg(row.csValue));
            addCellSimple(trow, "ra", format_pct(row.csPct));
            addCellSimple(trow, "la comp", toBarChart(row.csPct, '█'));
          } else {
            addCellAttr(trow, {'colspan':3, 'class':"ra"}, row.cs);
          }
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_SmResourceUsage(document.getElementById('tbody_sm_resource_usage'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


# A hidden table that adopts all orphan metrics that don't yet have a home in an official table
class AdditionalMetricsGenerator:
    def __init__(gen):
        gen.name = 'AdditionalMetrics'
        gen.required_counters = [
            'idc__requests',
            'idc__requests_lookup_hit',
            'idc__requests_lookup_miss',
            'l1tex__data_pipe_lsu_wavefronts_mem_lg',
            'l1tex__data_pipe_lsu_wavefronts_mem_shared',
            'l1tex__data_pipe_lsu_wavefronts_mem_surface',
            'l1tex__data_pipe_tex_wavefronts_mem_surface',
            'l1tex__data_pipe_tex_wavefronts_mem_texture',
            'l1tex__lsu_writeback_active',
            'l1tex__lsuin_requests',
            'l1tex__tex_writeback_active',
            'l1tex__texin_cycles_stalled_on_tsl1_miss',
            'l1tex__texin_requests',
            'l1tex__texin_sm2tex_req_cycles_active',
            'sm__mio_pq_read_cycles_active',
            'sm__mio_pq_write_cycles_active',
            'sm__mio2rf_writeback_active',
            'sm__ps_quads_launched',
            'smsp__thread_inst_executed',
            'smsp__thread_inst_executed_pred_off',
            'smsp__thread_inst_executed_pred_on',
            'smsp__warps_eligible',
        ]
        gen.required_ratios = [
            'smsp__amortized_warp_latency',
        ]
        gen.required_throughputs = [
            'l1tex__m_l1tex2xbar_throughput',
            'l1tex__m_xbar2l1tex_throughput',
        ]
        gen.workflow = None

    @property
    def html(gen):
        return ''

    @property
    def jsfunc(gen):
        jsfunc = []
        jsfunc += [r'''
      function AdditionalMetrics() {
''']
        for counter in gen.required_counters:
          jsfunc += ['        getCounterPct("{}");\n'.format(counter)]
        for ratio in gen.required_ratios:
          jsfunc += ['        getRatioPct("{}");\n'.format(ratio)]
        for throughput in gen.required_throughputs:
          jsfunc += ['        getThroughputPct("{}");\n'.format(throughput)]
        jsfunc += [r'''
      }
''']
        return ''.join(jsfunc)

    @property
    def jscall(gen):
        return '''AdditionalMetrics();'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class AllCountersGenerator:
    def __init__(gen):
        gen.name = 'AllCounters'
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="All-Counters">
          <thead>
            <tr>
              <th colspan="3" class="la tablename">All Counters</th>
              <th colspan="3" class="ca">Per Unit Instance (avg)</th>
              <th colspan="2" class="ca">Total (sum)</th>
              <th rowspan="2" class="ca"><a href="https://en.wikipedia.org/wiki/Dimensional_analysis" target="_blank">Dimensional Units</a></th>
            </tr>
            <tr>
              <th class="la">Counter Name</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">value</th>
              <th class="ra">per-cycle</th>
              <th class="ra">peak per-cycle</th>
              <th class="ra">value</th>
              <th class="ra">per-second</th>
            </tr>
          </thead>
          <tbody id="tbody_all_counters">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        return r'''
      function tbody_AllCounters(tbody) {
        let counterNames = [];
        for (const counter in g_counters) {
          counterNames.push(counter);
        }
        counterNames.sort();

        for (const counter of counterNames) {
          const sum = getCounterValue(counter, 'sum');
          const sps = getCounterValue(counter, 'sum.per_second');
          const avg = getCounterValue(counter, 'avg');
          const pct = getCounterValue(counter, 'avg.pct_of_peak_sustained_elapsed');
          const apc = getCounterValue(counter, 'avg.per_cycle_elapsed');
          const aps = getCounterValue(counter, 'avg.peak_sustained');
          const dim_units = getCounterDimUnits(counter);

          const submetrics = g_counters[counter];
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", counter);
          addCellSimple(trow, "ra", format_pct(pct));
          addCellSimple(trow, "la comp", toBarChart(pct, '█'));
          addCellSimple(trow, "ra", format_avg(avg));
          addCellSimple(trow, "ra", format_avg(apc, 2));
          addCellSimple(trow, "ra", format_avg(aps, 2));
          addCellSimple(trow, "ra", format_sum(sum));
          addCellSimple(trow, "ra", format_sum(sps));
          addCellSimple(trow, "ra", dim_units);
          tbody.appendChild(trow);
        }
      }
'''

    @property
    def jscall(gen):
        return '''tbody_AllCounters(document.getElementById('tbody_all_counters'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class AllRatiosGenerator:
    def __init__(gen):
        gen.name = 'AllRatios'
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="All-Ratios">
          <thead>
            <tr>
              <th colspan="3" class="la tablename">All Ratio Metrics</th>
              <th colspan="2" class="ca">Data</th>
            </tr>
            <tr>
              <th class="la">Ratio Name</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
              <th class="ra">ratio</th>
              <th class="ra">Max Rate</th>
            </tr>
          </thead>
          <tbody id="tbody_all_ratios">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        return r'''
      function tbody_AllRatios(tbody) {
        let ratioNames = [];
        for (const ratio in g_ratios) {
          ratioNames.push(ratio);
        }
        ratioNames.sort();

        for (const ratio of ratioNames) {
          const pct = getRatioValue(ratio, 'pct');
          const rio = getRatioValue(ratio, 'ratio');
          const max_rate = getRatioValue(ratio, 'max_rate');

          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", ratio);
          addCellSimple(trow, "ra", format_pct(pct, 2));
          addCellSimple(trow, "la comp", toBarChart(pct, '█'));
          addCellSimple(trow, "ra", format_avg(rio));
          addCellSimple(trow, "ra", format_avg(max_rate, 4));
          tbody.appendChild(trow);
        }
      }
'''

    @property
    def jscall(gen):
        return '''tbody_AllRatios(document.getElementById('tbody_all_ratios'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class AllThroughputsGenerator:
    def __init__(gen):
        gen.name = 'AllThroughputs'
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="All-Throughputs">
          <thead>
            <tr>
              <th colspan="3" class="la tablename">All Throughput Metrics</th>
            </tr>
            <tr>
              <th class="la">Throughput Name</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_all_throughputs">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        return r'''
      function tbody_AllThroughputs(tbody) {
        let throughputNames = [];
        for (const throughput in g_throughputs) {
          throughputNames.push(throughput);
        }
        throughputNames.sort();

        for (const throughput of throughputNames) {
          const pct = getThroughputValue(throughput, 'avg.pct_of_peak_sustained_elapsed');

          const submetrics = g_throughputs[throughput];
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", throughput);
          addCellSimple(trow, "ra", format_pct(pct));
          addCellSimple(trow, "la comp", toBarChart(pct, '█'));
          tbody.appendChild(trow);
        }
      }
'''

    @property
    def jscall(gen):
        return '''tbody_AllThroughputs(document.getElementById('tbody_all_throughputs'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class RequiredCountersGenerator:
    def __init__(gen, table_name):
        gen.table_name = table_name
        gen.name = 'RequiredCounters_' + table_name
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;">
          <thead>
            <tr>
              <th class="la">{gen.table_name}: Required Counters</th>
            </tr>
          </thead>
          <tbody id="tbody_required_counters_{gen.table_name}">
          </tbody>
        </table>
'''.format(**locals())

    @property
    def jsfunc(gen):
        return ''

    @property
    def jscall(gen):
        return '''tbody_RequiredCounters(document.getElementById('tbody_required_counters_{gen.table_name}'));'''.format(**locals())

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class RequiredRatiosGenerator:
    def __init__(gen, table_name):
        gen.table_name = table_name
        gen.name = 'RequiredRatios_' + table_name
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;">
          <thead>
            <tr>
              <th class="la">{gen.table_name}: Required Ratios</th>
            </tr>
          </thead>
          <tbody id="tbody_required_ratios_{gen.table_name}">
          </tbody>
        </table>
'''.format(**locals())

    @property
    def jsfunc(gen):
        return ''

    @property
    def jscall(gen):
        return '''tbody_RequiredRatios(document.getElementById('tbody_required_ratios_{gen.table_name}'));'''.format(**locals())

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class RequiredThroughputsGenerator:
    def __init__(gen, table_name):
        gen.table_name = table_name
        gen.name = 'RequiredThroughputs_' + table_name
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;">
          <thead>
            <tr>
              <th class="la">{gen.table_name}: Required Throughputs</th>
            </tr>
          </thead>
          <tbody id="tbody_required_throughputs_{gen.table_name}">
          </tbody>
        </table>
'''.format(**locals())

    @property
    def jsfunc(gen):
        return ''

    @property
    def jscall(gen):
        return '''tbody_RequiredThroughputs(document.getElementById('tbody_required_throughputs_{gen.table_name}'));'''.format(**locals())

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable


class RangesSummaryGenerator:
    class Col:
        def __init__(col, desc, value, format, align):
            col.desc = desc
            col.value = value
            col.format = format
            col.align = align

    def __init__(gen):
        gen.name = 'RangesSummary'
        gen.cols = []
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''
To find the biggest time consumers, sort by duration by clicking on the column header.<br>
To find cold spots, sort by GR Active%.<br>
To find regions with excessive synchronization, sort by #WFI (the number of Wait-for-Idle commands).<br>
Then follow the links to per-range reports in the Full Name column for more detail.
'''

    @property
    def html(gen):
        h = r'''
        <table style="border: 1px solid; table-layout: fixed;" id="table_summary">
'''
        h += r'''
          <thead>
            <tr>
              <th class="ra" onclick="summary_sortTable(0, parseInt, (lhs, rhs) => lhs - rhs)" style="cursor:pointer;" title="Click to sort by range index">#</th>
              <th class="la" onclick="summary_sortTable(1, s => s, (lhs, rhs) => lhs.localeCompare(rhs))" style="cursor:pointer;" title="Click to sort by full name">Full Name</th>
'''.format(num_stats=len(gen.cols))
        num_range_name_cols = 2
        for idx, col in enumerate(gen.cols):
            h += '''              <th class="{}" onclick="summary_sortTable({}, s => parseFloat(s.replaceAll(',', '')), compareNumbers)" style="cursor:pointer;" title="Click to sort by {}">{}</th>\n'''.format(col.align + " ww", num_range_name_cols + idx, col.desc, col.desc)
        h += r'''
            </tr>
          </thead>
          <tbody id="tbody_summary">
          </tbody>
        </table>
'''.format(**locals())
        return h

    @property
    def jsfunc(gen):
        js = '\n'

        js += '    const IS_COLUMN_ASCENDING_BY_DEFAULT = [true, true' + ', false' * (len(gen.cols) - 2) + '];\n'
        js += '    const isColumnAscending = [true' + ', null' * (len(gen.cols) - 1) + '];\n'
        js += '    let sortedColumnIndex = 0;\n'

        js += r'''
    function summary_sortTable(selectedColumnIndex, extractValue, compare) {
      var table = document.getElementById("table_summary");
      var rows = table.rows;
      const numHeaderRows = 1;
      if (rows.length < numHeaderRows + 2) {
        return;
      }

      // determine the order that we want to sort
      if (isColumnAscending[selectedColumnIndex] == null) {
        isColumnAscending[selectedColumnIndex] = IS_COLUMN_ASCENDING_BY_DEFAULT[selectedColumnIndex];
      } else if (selectedColumnIndex == sortedColumnIndex) {
        isColumnAscending[selectedColumnIndex] = !isColumnAscending[selectedColumnIndex];
      } else {
        // keep the previous order
      }

      sortedColumnIndex = selectedColumnIndex;

      var store = [];
      for (var ii = numHeaderRows; ii < rows.length; ii++) {
        var row = rows[ii];
        var value = extractValue(row.getElementsByTagName("TD")[selectedColumnIndex].innerText);
        store.push([value, row]);
      }
      store.sort(function(lhs_pair, rhs_pair) {
        const ret = compare(lhs_pair[0], rhs_pair[0]);
        if (isColumnAscending[selectedColumnIndex]) {
          return ret;
        } else {
          return -ret;
        }
      });
      // note nodes are moved: https://stackoverflow.com/questions/12146888/why-does-appendchild-moves-a-node
      for (var ii = 0; ii < store.length; ii++) {
        table.appendChild(store[ii][1]);
      }
    }

    function tbody_Summary(tbody) {
      let addRow = function(rangeIndex, rangeName, perRangeReportLink) {
        let fullName = rangeName;
        let hierarchy = rangeName.split('/');
        let leafName = hierarchy[hierarchy.length - 1];
        var trow = document.createElement('tr');
        addCellSimple(trow, 'ra', rangeIndex, passthrough=true);
        addCellSimple(trow, 'la ww full_name', escapeHtml(fullName).link(perRangeReportLink), true);
'''
        for col in gen.cols:
            classes = "'{}'".format(col.align)
            js += '        addCellSimple(trow, {}, {}({}), passthrough=true);\n'.format(classes, col.format, col.value)
        js += r'''
        tbody.appendChild(trow);
      };

      g_ranges.forEach(function (rangeName, rangeIndex) {
        g_counters = g_ranges_counters[rangeName] || {};
        g_ratios = g_ranges_ratios[rangeName] || {};
        g_throughputs = g_ranges_throughputs[rangeName] || {};
        perRangeReportLink = g_range_file_names[rangeIndex];
        addRow(rangeIndex, rangeName, perRangeReportLink);
      });
    }
'''
        return js

    @property
    def jscall(gen):
        return '''tbody_Summary(document.getElementById('tbody_summary'));'''.format(**locals())

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable

class CollectionInfoGenerator:
    class Row:
        def __init__(row, name, attr, value):
            row.name = name
            row.attr = attr
            row.value = value

    def __init__(gen):
        gen.name = 'CollectionInfo'
        gen.rows = [
            gen.Row('Collection Time'   , "ra", "timeToStr(g_time)"),
            gen.Row('GPU Name'          , "ra", "g_device.gpuName"),
            gen.Row('Chip Name'         , "ra", "g_device.chipName"),
            gen.Row('#Ranges'           , "ra", "g_ranges.length.toString()"),
        ]
        gen.required_counters = []
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = None

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;">
          <thead>
            <tr>
              <th colspan="2" class="ca tablename">Collection Information</th>
            </tr>
            <tr>
              <th class="la">Name</th>
              <th class="la">Value</th>
            </tr>
          </thead>
          <tbody id="tbody_collection_info">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_CollectionInfo(tbody) {
        class Row {
          constructor(name, valueclass, value) {
            this.name = name;
            this.valueclass = valueclass;
            this.value = value;
          }
        }

        let rows = [
''']
        for row in gen.rows:
            qname = "'{row.name}'".format(row=row)
            qattr = '"{row.attr}"'.format(row=row)
            jsfunc_lines += ["          new Row({qname:<40}, {qattr:<10}, {row.value:<60}),\n".format(**locals())]
        jsfunc_lines += ['''
        ];

        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, row.valueclass, row.value);
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_CollectionInfo(document.getElementById('tbody_collection_info'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable

def get_range_common_on_body_loaded(dtables):
    js = ''
    # per-table data population function invocations
    for dtable in dtables:
        js += r'''
        if (g_debug) {{
          clearReferencedMetrics();
        }}
        {dtable.jscall}
'''.format(**locals())
        if dtable.name not in ('AllCounters', 'AllRatios', 'AllThroughputs'):
            # if debugging is enabled, additionally populate the debug sections
            dtable_requiredCounters = RequiredCountersGenerator(dtable.name).make_data_table()
            dtable_requiredRatios = RequiredRatiosGenerator(dtable.name).make_data_table()
            dtable_requiredThroughputs = RequiredThroughputsGenerator(dtable.name).make_data_table()
            requiredCounters_expected = '[' + ', '.join("'{}'".format(x) for x in sorted(set(dtable.required_counters))) + ']'
            requiredRatios_expected = '[' + ', '.join("'{}'".format(x) for x in sorted(set(dtable.required_ratios))) + ']'
            requiredThroughputs_expected = '[' + ', '.join("'{}'".format(x) for x in sorted(dtable.required_throughputs)) + ']'
            js += r'''
        if (g_debug) {{
          counters = {dtable_requiredCounters.jscall}
          if (JSON.stringify(counters) != JSON.stringify({requiredCounters_expected})) {{
            alert("{dtable.name}'s required counters mismatch! Please paste the latest list to the generator.");
            console.log("{dtable.name}'s required counters mismatch!");
            console.log("Actual: ", counters);
            console.log("Generator: {requiredCounters_expected}");
          }}
          ratios = {dtable_requiredRatios.jscall}
          if (JSON.stringify(ratios) != JSON.stringify({requiredRatios_expected})) {{
            alert("{dtable.name}'s required ratios don't match the expectation! Please paste the latest list to the generator.");
            console.log("{dtable.name}'s required ratios mismatch!");
            console.log("Actual: ", ratios);
            console.log("Generator: {requiredRatios_expected}");
          }}
          throughputs = {dtable_requiredThroughputs.jscall}
          if (JSON.stringify(throughputs) != JSON.stringify({requiredThroughputs_expected})) {{
            alert("{dtable.name}'s required throughputs don't match the expectation! Please paste the latest list to the generator.");
            console.log("{dtable.name}'s required throughputs mismatch!");
            console.log("Actual: ", throughputs);
            console.log("Generator: {requiredThroughputs_expected}");
          }}
        }}
'''.format(**locals())

    # if debugging is not enabled, hide the debug sections
    js += r'''
        if (!g_debug) {
          var debugSections = document.getElementsByClassName('debug_section');
          for(debugSection of debugSections) {
            debugSection.style.display = "none";
          }
        }
'''
    return js

def generate_range_html_common(sections, head_css=get_common_css(), head_js_common_functions=get_js_common_functions(), head_js_on_body_loaded_fn=get_range_common_on_body_loaded):
    dtables = get_data_tables(sections)

    # overall sketch
    html_skeleton = r'''
<html>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>

  <head>
    <title>MyRangeName</title>
{css}
{js}
  </head>

  <body onload="onBodyLoaded()" style="background-color:#202020;">
    <noscript>
      <p>Enable javascript to see report contents</span>
    </noscript>
{body}
  </body>
</html>
'''

    # 1. head
    # 1.1 css
    # 1.1.1 common_css
    css = r'''
    <style id="ReportStyle">
{head_css}
    </style>
'''.format(**locals())

    # 1.2 js
    # 1.2.1 js_common_functions
    js = r'''
    <script type="text/JavaScript">
{head_js_common_functions}
'''.format(**locals())
    # 1.2.2 its own js functions
    js += r'''
      function onClick_ShowWorkflow() {
        let showWorkflowCheckboxElement = document.getElementById('show-workflow');
        let checkboxValue = showWorkflowCheckboxElement.checked;
        let instructionElements = document.getElementsByClassName("workflow");
        for (let elem of instructionElements) {
          elem.hidden = !checkboxValue;
        }
      }

      function onClick_MobileLayout() {
        let mobileLayoutCheckboxElement = document.getElementById('mobile-layout');
        let checkboxValue = mobileLayoutCheckboxElement.checked;
        let tableSpacingElements = document.getElementsByName("table_spacing");
        for (let elem of tableSpacingElements) {
          elem.hidden = !checkboxValue;
        }
      }
'''
    # 1.2.3 per-table data population function definitions
    for dtable in dtables:
        js += dtable.jsfunc
    # 1.2.4 onBodyLoaded
    js += r'''
      function onBodyLoaded() {{
        document.title = g_rangeName;
        document.getElementById('titlebar_text').innerHTML = g_rangeName;

        // Call the onClick handlers to react to the initial checked state.  This allows controlling of the input elements' default "checked" state in the HTML.
        onClick_ShowWorkflow();
        onClick_MobileLayout();

{on_body_loaded}
      }}
    </script>
'''.format(on_body_loaded = head_js_on_body_loaded_fn(dtables))


    # 2. body
    body = r'''
    <div>
      <div class="titlearea">
        <div class="titlebar">
          <img src="https://developer.nvidia.com/sites/all/themes/devzone_new/nvidia_logo.png"/>
          <span class="title" id="titlebar_text">Nsight Perf SDK Profiler Report</span>
        </div>
        <div class="global_settings">
          <span style="background-color: #555555;">
            <label for="show-workflow">Show Workflow:</label>
            <input id="show-workflow" type="checkbox" checked onclick="onClick_ShowWorkflow()"/>
          </span>
          <span style="background-color: #333333;">
            <label for="mobile-layout">Mobile Layout:</label>
            <input id="mobile-layout" type="checkbox" checked onclick="onClick_MobileLayout()"/>
          </span>
        </div>
      </div>
'''
    # 2.1 data tables
    for section in sections:
        body += r'''
      <div class="section">
'''
        if section.title:
            body += '''
        <div class="section_title">{section.title}</div>
'''.format(section=section)
        for dtable in section.dtables:
            if dtable.workflow:
                body += '''
        <div class="workflow">{dtable.workflow}</div>
'''.format(dtable=dtable)
            body += dtable.html
            if section.inter_table_spacing:
                body += '''
        <br name="table_spacing">
'''
        body += r'''
      </div>
'''
    # 2.2 debugging data tables(all classified by "debug_section" tag)
    for dtable in dtables:
        if dtable.name in ('AllCounters', 'AllRatios', 'AllThroughputs'):
            continue
        body += r'''
      <div class="debug_section">
'''
        dtable_requiredCounters = RequiredCountersGenerator(dtable.name).make_data_table()
        dtable_requiredRatios = RequiredRatiosGenerator(dtable.name).make_data_table()
        dtable_requiredThroughputs = RequiredThroughputsGenerator(dtable.name).make_data_table()
        body += dtable_requiredCounters.html
        body += dtable_requiredRatios.html
        body += dtable_requiredThroughputs.html
        body += r'''
      </div>
'''
    # 2.3 footer
    body += r'''
    </div>

    <div id="footer">
      <span>This report is not licensed for benchmarking, nor comparison between GPU parts(<a href="readme.html#unintended_use">learn more</a>).</span>
    </div>
'''
    # 2.4 global js data(note the "debug" is controlled by the pass-in param)
    body += r'''
    <script>
      g_json = {
        /***JSON_DATA_HERE***/
      };
'''
    body += r'''
      g_time = g_json.secondsSinceEpoch || 0;
      g_rangeName = g_json.rangeName || 'Perf Marker Name';
      g_device = g_json.device || {};
      g_device.gpuName = g_device.gpuName || 'Unknown GPU';
      g_device.chipName = g_device.chipName || 'Unknown Chip';
      g_device.clockLockingStatus = g_device.clockLockingStatus || 'Unknown';

      g_counters = g_json.counters || {};
      g_ratios = g_json.ratios || {};
      g_throughputs = g_json.throughputs || {};

      // for debugging
      g_debug = (g_json.debug == undefined) ? true : g_json.debug;
      g_populateDummyValues = (g_json.populateDummyValues == undefined) ? true : g_json.populateDummyValues;
      g_counters_referenced = new Set();
      g_ratios_referenced = new Set();
      g_throughputs_referenced = new Set();
    </script>
'''

    html = html_skeleton.format(**locals())
    return html


def generate_summary_html_common(sections, head_css=get_common_css(), head_js_common_functions=get_js_common_functions(), head_js_on_body_loaded_fn=get_range_common_on_body_loaded):
    dtables = get_data_tables(sections)

    # overall sketch
    html_skeleton = r'''
<html>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>

  <head>
    <title>Summary</title>
{css}
{js}
  </head>

  <body onload="onBodyLoaded()" style="background-color:#202020;">
    <noscript>
      <p>Enable javascript to see report contents</span>
    </noscript>
{body}
  </body>
</html>
'''

    # 1. head
    # 1.1 css
    # 1.1.1 common_css
    css = r'''
    <style id="ReportStyle">
{head_css}
    </style>
'''.format(**locals())

    # 1.2 js
    # 1.2.1 js_common_functions
    js = r'''
    <script type="text/JavaScript">
{head_js_common_functions}
'''.format(**locals())
    # 1.2.2 its own js functions
    js += r'''
'''
    # 1.2.3 per-table data population function definitions
    for dtable in dtables:
        js += dtable.jsfunc
    # 1.2.4 onBodyLoaded
    js += r'''
      function onBodyLoaded() {{
{on_body_loaded}
      }}
    </script>
'''.format(on_body_loaded = head_js_on_body_loaded_fn(dtables))


    # 2. body
    body = r'''
    <div>
    <div class="titlearea">
        <div class="titlebar">
          <img src="https://developer.nvidia.com/sites/all/themes/devzone_new/nvidia_logo.png"/>
          <span class="title" id="titlebar_text">Nsight Perf SDK Profiler Report</span>
        </div>
    </div>
'''
    # 2.1 data tables
    for section in sections:
        body += r'''
      <div class="section">
'''
        if section.title:
            body += '''
        <div class="section_title">{section.title}</div>
'''.format(section=section)
        for dtable in section.dtables:
            if dtable.workflow:
                body += '''
        <div class="workflow">{dtable.workflow}</div>
'''.format(dtable=dtable)
            body += dtable.html
        body += r'''
      </div>
'''
    # 2.2 debugging data tables(all classified by "debug_section" tag)
    for dtable in dtables:
        if dtable.name in ('AllCounters', 'AllRatios', 'AllThroughputs'):
            continue
        body += r'''
      <div class="debug_section">
'''
        dtable_requiredCounters = RequiredCountersGenerator(dtable.name).make_data_table()
        dtable_requiredRatios = RequiredRatiosGenerator(dtable.name).make_data_table()
        dtable_requiredThroughputs = RequiredThroughputsGenerator(dtable.name).make_data_table()
        body += dtable_requiredCounters.html
        body += dtable_requiredRatios.html
        body += dtable_requiredThroughputs.html
        body += r'''
      </div>
'''
    # 2.3 footer
    body += r'''
    </div>

    <div id="footer">
      <span>This report is not licensed for benchmarking, nor comparison between GPU parts(<a href="readme.html#unintended_use">learn more</a>).</span>
    </div>
'''
    # 2.4 global js data(note the "debug" is controlled by the pass-in param)
    body += r'''
    <script>
      g_json = {
        /***JSON_DATA_HERE***/
      };
'''
    body += r'''
      g_time = g_json.secondsSinceEpoch || 0;
      g_device = g_json.device || {};
      g_device.gpuName = g_device.gpuName || 'Unknown GPU';
      g_device.chipName = g_device.chipName || 'Unknown Chip';

      g_ranges = g_json.ranges || [];
      g_range_file_names = g_json.range_file_names || [];
      g_ranges_counters = g_json.rangesCounters || {};
      g_ranges_ratios = g_json.rangesRatios || {};
      g_ranges_throughputs = g_json.rangesThroughputs || {};

      g_counters = {};
      g_ratios = {};
      g_throughputs = {};

      // for debugging
      g_debug = (g_json.debug == undefined) ? true : g_json.debug;
      g_populateDummyValues = (g_json.populateDummyValues == undefined) ? true : g_json.populateDummyValues;
      if (g_populateDummyValues) {
        g_ranges.push('Q0 / FRAME');
        g_range_file_names.push('00000_FRAME.html');
        g_ranges.push('Q0 / LEFT_EYE');
        g_range_file_names.push('00001_LEFT_EYE.html');
        g_ranges.push('Q0 / LEFT_EYE / SCENE');
        g_range_file_names.push('00002_SCENE.html');
        g_ranges.push('Q0 / LEFT_EYE / SCENE / GPU_PARTICLES');
        g_range_file_names.push('00003_GPU_PARTICLES.html');
        g_ranges.push('Q0 / LEFT_EYE / SCENE / VOLUMETRIC_CLOUDS_SHADOWGEN');
        g_range_file_names.push('00004_VOLUMETRIC_CLOUDS_SHADOWGEN.html');
        g_ranges.push('Q0 / LEFT_EYE / SCENE / ' + 'I_AM_A_LONG' + 'G'.repeat(50) + '_STRING');
        g_range_file_names.push('00005_I_AM_A_LONG_STRING.html');
      }
      g_counters_referenced = new Set();
      g_ratios_referenced = new Set();
      g_throughputs_referenced = new Set();
    </script>
'''

    html = html_skeleton.format(**locals())
    return html
