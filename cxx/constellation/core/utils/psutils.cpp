/**
 * @file
 * @brief Utility implementation for system information
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "psutils.hpp"

#include <cstdint>
#include <string>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fstream>
#include <string>

#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#endif

using namespace constellation::utils;

double constellation::utils::get_cpu_load_average() {
#ifdef _WIN32
    // On windows, access to load average is not straight-forward, use active CPU count as simple proxy
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return 100.0 * static_cast<double>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)) / std::thread::hardware_concurrency();
#else
    double loads[3] {};
    if(getloadavg(loads, 3) != -1) {
        return 100.0 * loads[0] / std::thread::hardware_concurrency();
    }
    return 0.0;
#endif
}

std::uint64_t constellation::utils::get_available_memory() {

#ifdef _WIN32
    MEMORYSTATUSEX mem {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return mem.ullAvailPhys / 1024ULL / 1024ULL;
#endif

#ifdef __linux__
    auto file = std::ifstream("/proc/meminfo");
    std::string key;
    std::uint64_t value;
    std::string unit;

    while(file >> key >> value >> unit) {
        if(key == "MemAvailable:")
            return value / 1024ULL;
    }
    return 0;
#endif

#ifdef __APPLE__
    auto host = mach_host_self();
    vm_statistics64_data_t vmstat;
    auto count = HOST_VM_INFO64_COUNT;

    host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmstat), &count);
    uint64_t free = vmstat.free_count + vmstat.inactive_count;

    vm_size_t page_size;
    host_page_size(host, &page_size);

    return free * page_size / 1024ULL / 1024ULL;

#endif
    return 0;
}
