#pragma once

#include <nanobind/nanobind.h>
#include <string>
#include "api.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

namespace python {

// Initialize Python interpreter
RZPYTHON_API void initialize();

// Finalize Python interpreter
RZPYTHON_API void finalize();

// Import a Python module
RZPYTHON_API void import(const std::string& module_name);

// Internal helper for dynamic type conversion
RZPYTHON_API PyObject* call_raw(const std::string& code);

// Call Python code and get return values of specific types
template<typename T>
T call(const std::string& code);

// Specializations for common types
template<>
RZPYTHON_API int call<int>(const std::string& code);
template<>
RZPYTHON_API float call<float>(const std::string& code);
template<>
RZPYTHON_API void call<void>(const std::string& code);

// Bind C++ object to Python variable name
template<typename T>
void bind_object(const std::string& name, T* obj);

// Helper function to get nanobind cast for objects
template<typename T>
void reference(const std::string& name, T* obj);

}  // namespace python

USTC_CG_NAMESPACE_CLOSE_SCOPE

// Template implementations - include after declarations
#include "rzpython_impl.hpp"
