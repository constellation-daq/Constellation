/**
 * @file
 * @brief Implementation of PyLoader
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "PyLoader.hpp"

#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>

#ifdef CNSTLN_EXEC_PYTHON
#include <Python.h>
#endif

#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/exceptions.hpp"

using namespace constellation::exec;
using namespace constellation::utils;

#ifdef CNSTLN_EXEC_PYTHON
namespace {
    std::string handle_python_error() {
        std::string exception_message {};
        if(PyErr_Occurred() != nullptr) {
            // Fetch exception
            PyObject* py_exc_type {};
            PyObject* py_exc_value {};
            PyObject* py_exc_traceback {};
            PyErr_Fetch(&py_exc_type, &py_exc_value, &py_exc_traceback);

            // Convert exception to string
            auto* py_exc_str = PyObject_Str(py_exc_value);
            exception_message = PyUnicode_AsUTF8AndSize(py_exc_str, nullptr);

            // Clean up exception variables
            Py_XDECREF(py_exc_type);
            Py_XDECREF(py_exc_value);
            Py_XDECREF(py_exc_traceback);
            Py_XDECREF(py_exc_str);
        }
        return exception_message;
    }

    std::pair<PyObject*, std::string> run_py_func_from_module(const std::string& module,
                                                              const std::string& function,
                                                              PyObject* arg) {
        // Import module
        auto* py_module = PyImport_ImportModule(module.c_str());
        if(py_module == nullptr) {
            throw PyLoadingError(module, "Python module not found");
        }

        // Get function from module
        auto* py_func = PyObject_GetAttrString(py_module, function.c_str());
        if(py_func == nullptr) {
            Py_DECREF(py_module);
            throw PyLoadingError(function, "function does not exist in " + quote(module) + " Python module");
        }

        LOG(TRACE) << "Imported function " << quote(function) << " from Python module " << quote(module);

        // Create the function arguments
        auto* py_func_args = PyTuple_New(1);
        PyTuple_SetItem(py_func_args, 0, arg);

        // Call the function
        auto* py_res = PyObject_Call(py_func, py_func_args, nullptr);

        // Clean up py_func, py_func_args and py_module
        Py_DECREF(py_func);
        Py_DECREF(py_func_args);
        Py_DECREF(py_module);

        // Check for exceptions
        auto exception_message = handle_python_error();

        return {py_res, std::move(exception_message)};
    }
} // namespace
#endif

PyLoader::PyLoader() {
#ifdef CNSTLN_EXEC_PYTHON
    Py_Initialize();
#endif
}

PyLoader::~PyLoader() {
#ifdef CNSTLN_EXEC_PYTHON
    handle_python_error();
    Py_Finalize();
#endif
}

std::map<std::string, std::string> PyLoader::loadModules() { // NOLINT(readability-convert-member-functions-to-static)
#ifdef CNSTLN_EXEC_PYTHON
    std::map<std::string, std::string> satellite_modules {};

    // Run discover_plugins function
    const auto [py_res, exception_message] = run_py_func_from_module(
        "constellation.tools.discover_plugins", "discover_plugins", PyUnicode_FromString("constellation.satellites"));

    // Convert return value
    if(py_res != nullptr) {
        // Iterate over dict
        PyObject* key {};
        PyObject* value {};
        Py_ssize_t pos = 0;
        while(PyDict_Next(py_res, &pos, &key, &value) != 0) {
            const auto [it, _] =
                satellite_modules.emplace(PyUnicode_AsUTF8AndSize(key, nullptr), PyUnicode_AsUTF8AndSize(value, nullptr));
            LOG(TRACE) << "Found satellite type " << it->first << " in Python module " << quote(it->second);
        }

        // Clean up py_res
        Py_DECREF(py_res);
    }

    if(!exception_message.empty()) {
        throw PyLoaderPythonException(exception_message);
    }

    return satellite_modules;
#else
    throw PyLoaderNotBuildError();
#endif
}

void PyLoader::runModule( // NOLINT(readability-convert-member-functions-to-static)
    [[maybe_unused]] const std::string& module,
    [[maybe_unused]] const std::vector<std::string>& args) {
#ifdef CNSTLN_EXEC_PYTHON
    // Recreate arguments for main function
    const auto args_size = static_cast<Py_ssize_t>(args.size());
    auto* py_args = PyList_New(args_size);
    for(Py_ssize_t n = 0; n < args_size; ++n) {
        PyList_SetItem(py_args, n, PyUnicode_FromString(args.at(n).c_str()));
    }

    // Run main function and discard return value
    const auto [py_res, exception_message] = run_py_func_from_module(module + ".__main__", "main", py_args);
    Py_XDECREF(py_res);

    if(!exception_message.empty()) {
        throw PyLoaderPythonException(exception_message);
    }
#else
    throw PyLoaderNotBuildError();
#endif
}
