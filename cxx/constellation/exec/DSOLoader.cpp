/**
 * @file
 * @brief Implementation of the DSOLoader
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DSOLoader.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "constellation/build.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::satellite;
using namespace constellation::utils;

DSOLoader::DSOLoader(const std::string& dso_name, Logger& logger, const std::filesystem::path& hint) {
    // Possible paths:
    // - custom executable: hint
    // - in dev environment: builddir/satellites/XYZ/libXYZ.suffix
    // - in installed environment: libdir/ConstellationSatellites/libXYZ.suffix
    const auto dso_file_name = "lib" + dso_name + CNSTLN_DSO_SUFFIX;

    auto possible_paths = std::vector<std::filesystem::path>();
    auto add_paths = [&](const std::filesystem::path& path) {
        const auto abs_path = std::filesystem::absolute(path);
        if(std::filesystem::exists(abs_path)) {
            for(const auto& entry : std::filesystem::recursive_directory_iterator(abs_path)) {
                if(std::filesystem::is_regular_file(entry) && entry.path().extension() == CNSTLN_DSO_SUFFIX) {
                    LOG(logger, TRACE) << "Adding " << entry.path() << " to library lookup";
                    possible_paths.emplace_back(entry.path());
                }
            }
        }
    };

    // Hint has highest priority:
    if(!hint.empty()) {
        add_paths(hint);
    }

    const auto build_dir = std::filesystem::path(CNSTLN_BUILDDIR) / "cxx" / "satellites";
    add_paths(build_dir);

    const auto lib_dir = std::filesystem::path(CNSTLN_LIBDIR) / "ConstellationSatellites";
    add_paths(lib_dir);

    // Check files following priority
    std::filesystem::path library_path {};
    for(const auto& path : possible_paths) {
        LOG(logger, TRACE) << "Looking for " << dso_name << " in " << path;
        if(transform(path.filename().string(), ::tolower) == transform(dso_file_name, ::tolower)) {
            LOG(logger, DEBUG) << "Found " << dso_name << " in " << path;
            library_path = path;
            break;
        }
    }

    const std::string path_str = library_path.string();
    dso_name_ = library_path.stem().string().substr(3);

    if(path_str.empty()) {
        throw DSOLoadingError(dso_name, "Could not find " + dso_file_name);
    }

    // Load the DSO
#ifdef _WIN32
    handle_ = static_cast<void*>(LoadLibrary(path_str.c_str()));
    if(handle_ == nullptr) {
        const auto last_win_error = std::system_category().message(GetLastError());
        throw DSOLoadingError(dso_name_, last_win_error);
    }
#else
    handle_ = dlopen(path_str.c_str(), RTLD_NOW);
    if(handle_ == nullptr) {
        const auto* error_dlopen = dlerror(); // NOLINT(concurrency-mt-unsafe)
        throw DSOLoadingError(dso_name_, error_dlopen);
    }
#endif

    LOG(logger, DEBUG) << "Loaded shared library " << library_path.filename();
}

DSOLoader::~DSOLoader() {
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
}

Generator* DSOLoader::loadSatelliteGenerator() {
    return getFunctionFromDSO<satellite::Generator>("generator");
}

void* DSOLoader::get_raw_function_from_dso(const std::string& function_name) {
#ifdef _WIN32
    void* function = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), function_name.c_str()));
    if(function == nullptr) {
        const auto last_win_error = std::system_category().message(GetLastError());
        throw DSOFunctionLoadingError(function_name, dso_name_, last_win_error);
    }
#else
    void* function = dlsym(handle_, function_name.c_str());
    if(function == nullptr) {
        const auto* error_dlsym = dlerror(); // NOLINT(concurrency-mt-unsafe)
        throw DSOFunctionLoadingError(function_name, dso_name_, error_dlsym);
    }
#endif

    return function;
}
