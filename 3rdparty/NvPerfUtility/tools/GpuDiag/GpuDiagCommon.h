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

#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#if defined(_WIN32)
#include <codecvt>
#endif

namespace nv { namespace perf { namespace tool {

    template <typename T>
    inline std::string SizeToString(T size)
    {
        const char SIZE_UNITS[5][4] = { "B", "KiB", "MiB", "GiB", "TiB" };
        double size_ = static_cast<double>(size);
        size_t unitIdx = 0;
        while (size_ >= 1024.0)
        {
            size_ /= 1024.0;
            ++unitIdx;
        }
        std::stringstream ss;
        ss.precision(2);
        ss << std::fixed << size_ << " " << SIZE_UNITS[unitIdx];
        return ss.str();
    }

    template <size_t ArraySize>
    inline std::string IdToString(const uint8_t id[ArraySize])
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t ii = 0; ii < ArraySize; ++ii)
        {
            if (ii && !(ii % 2))
            {
                ss << "-";
            }
            ss << std::setw(2) << static_cast<uint32_t>(id[ii]);
        }
        return ss.str();
    }

#if defined(_WIN32)
    std::string WStrToUtf8(const std::wstring& wstr)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::string str = converter.to_bytes(wstr);
        return str;
    }
#endif

}}} // nv::perf::tool
