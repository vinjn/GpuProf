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
#include <sstream>
#include <iomanip>
#include <vector>

#include <sys/utsname.h>
#include <errno.h>

#include <json/json.hpp>

#include "GpuDiagCommon.h"

namespace nv { namespace perf { namespace tool { namespace linux_ {

    using namespace nv::perf::tool;
    using namespace nlohmann;

    struct State
    {
    };

    // a simple RAII wrapper
    class Pipe
    {
    private:
        FILE* m_pPipe;
        std::string m_cmd; // DFD
    public:
        Pipe()
            : m_pPipe()
        {
        }
        Pipe(FILE* pPipe, const std::string& cmd)
            : m_pPipe(pPipe)
            , m_cmd(cmd)
        {
        }
        Pipe(Pipe&& pipe)
            : m_pPipe(pipe.m_pPipe)
            , m_cmd(pipe.m_cmd)
        {
        }
        Pipe& operator=(Pipe&& rhs)
        {
            m_pPipe = rhs.m_pPipe;
            m_cmd = rhs.m_cmd;
            rhs.m_pPipe = nullptr;
            rhs.m_cmd = std::string();
            return *this;
        }
        ~Pipe()
        {
            if (m_pPipe)
            {
                int ret = pclose(m_pPipe);
                if (ret)
                {
                    NV_PERF_LOG_ERR(50, "Failed pclose for cmd: %s\nError: %s\n", m_cmd.c_str(), strerror(errno));
                }
            }
        }
    private:
        Pipe(const Pipe& pipe);
        Pipe& operator=(const Pipe& rhs);
    };

    bool ReadFromCmd(const char* pCmd, std::string& output)
    {
        FILE* pPipe = popen(pCmd, "r");
        if (!pPipe)
        {
            NV_PERF_LOG_ERR(50, "Failed popen for cmd: %s\nError: %s\n", pCmd, strerror(errno));
            return false;
        }
        Pipe pipe(pPipe, pCmd);

        std::stringstream ss;
        const size_t BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE] = {};
        while (true)
        {
            char* pStr = std::fgets(buffer, BUFFER_SIZE, pPipe);
            if (!pStr)
            {
                if (ferror(pPipe))
                {
                    NV_PERF_LOG_ERR(50, "Error detected for cmd: %s\nError: %s\n", pCmd, strerror(errno));
                    return false;
                }
                output = ss.str();
                return true;
            }
            const size_t length = strlen(buffer);
            if (length)
            {
                if (buffer[length - 1] == '\n')
                {
                    buffer[length - 1] = '\0';
                }
                ss << buffer;
                buffer[0] = '\0';
            }
        }
        return true;
    }

    inline std::string ReadFromCmd(const char* pCmd)
    {
        std::string output;
        bool success = ReadFromCmd(pCmd, output);
        if (!success)
        {
            NV_PERF_LOG_ERR(50, "Failed ReadFromCmd for cmd\n", pCmd);
            return "Unknown";
        }
        return output;
    }

    inline std::string GetOSNameFromUName()
    {
        utsname name;
        int ret = uname(&name);
        if (ret)
        {
            NV_PERF_LOG_ERR(10, "Failed uname: %s\n", strerror(errno));
            return "Unknown";
        }

        std::stringstream ss;
        ss << name.sysname << "(" << name.machine << ") " << name.release;
        // TODO: if it's Ubuntu, we can further convert the kernel versions to Ubuntu versions:
        // https://askubuntu.com/questions/517136/list-of-ubuntu-versions-with-corresponding-linux-kernel-version/517140#517140
        return ss.str();
    }

    inline std::string GetOSName(const State& state)
    {
        std::string osVerStr;
        bool success = ReadFromCmd("lsb_release -ds", osVerStr);
        if (success)
        {
            return osVerStr;
        }
        NV_PERF_LOG_ERR(10, "Reading os version from lsb_release failed, trying reading from uname\n");
        return GetOSNameFromUName();
    }

    inline std::string GetPhysicalMemorySize()
    {
        std::string sizeStr;
        bool success  = ReadFromCmd("awk '/MemTotal/ { print $2 }' /proc/meminfo", sizeStr);
        if (success)
        {
            return sizeStr + " kB";
        }
        return "Unknown";
    }

    inline bool InitializeState(State& state)
    {
        return true;
    }

    inline void AppendState(const State& state, ordered_json& node)
    {
        node["OS"] = GetOSName(state);
        node["Processor"] = ReadFromCmd("cat /proc/cpuinfo | grep \"model name\" | cut -d \":\" -f2 | head -1");
        node["NumberOfProcessors"] = ReadFromCmd("nproc --all"); // both "nproc" & "grep -c ^processor /proc/cpuinfo" may not work with hyperthreading
        node["PhysicalMemory"] = GetPhysicalMemorySize();
    }

    inline void CleanupState(State& state)
    {
        state = State();
    }

}}}} // nv::perf::tool::linux_
