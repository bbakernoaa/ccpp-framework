/// @file ccpp_bridge.cpp
/// @brief Implementation of the C-linkage bridge functions for the CCPP C++ Framework.
///
/// Maintains a static global FrameworkOrchestrator instance. Each bridge function
/// wraps its body in try/catch blocks to prevent C++ exceptions from propagating
/// across the iso_c_binding boundary into Fortran.

#include "ccpp/ccpp_bridge.h"
#include "ccpp/ccpp_framework_orchestrator.hpp"
#include "ccpp/ccpp_host_state_descriptor.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

namespace {

/// Static global orchestrator instance (lives for the program's duration).
ccpp::FrameworkOrchestrator g_orchestrator;

/// Clear the error message buffer (fill with null bytes).
void clear_errmsg(char* errmsg, int errmsg_len) {
    if (errmsg != nullptr && errmsg_len > 0) {
        std::memset(errmsg, 0, static_cast<size_t>(errmsg_len));
    }
}

/// Copy an error message into the Fortran output buffer.
/// Truncates at errmsg_len-1 and always null-terminates. Never overflows.
void copy_errmsg(const std::string& msg, char* errmsg, int errmsg_len) {
    if (errmsg == nullptr || errmsg_len <= 0) {
        return;
    }
    // Determine how many characters we can copy (leave room for null terminator).
    const size_t max_chars = static_cast<size_t>(errmsg_len - 1);
    const size_t copy_len = std::min(msg.size(), max_chars);
    std::memcpy(errmsg, msg.c_str(), copy_len);
    errmsg[copy_len] = '\0';
}

} // anonymous namespace

extern "C" {

int ccpp_framework_init(
    const char* suite_name,
    int         num_suites,
    void**      host_ptrs,
    const int*  dims,
    int         ndims,
    char*       errmsg,
    int         errmsg_len)
{
    try {
        clear_errmsg(errmsg, errmsg_len);

        // Construct the HostStateDescriptor from the C parameters.
        ccpp::HostStateDescriptor host_desc;
        host_desc.host_name = "fortran_host";

        // Register suite names. The suite_name parameter provides the base name;
        // for multi-suite configurations num_suites indicates how many.
        if (suite_name != nullptr) {
            // For now, register the single provided suite name.
            // Multi-suite support (comma-separated or array-based) will be
            // extended in future integration tasks.
            host_desc.suite_names.emplace_back(suite_name);
        }
        (void)num_suites;

        // Parse host pointers and their associated dimension metadata.
        // The dims array is structured as: for each host pointer, the first
        // entry is the rank, followed by 'rank' extent values.
        // E.g., for a 2D array with shape (10, 20): [2, 10, 20]
        int dim_offset = 0;
        int ptr_idx = 0;
        while (dim_offset < ndims && host_ptrs != nullptr) {
            ccpp::HostVarDescriptor var;
            var.data_ptr = host_ptrs[ptr_idx];

            // Read rank from dims array.
            const int rank = dims[dim_offset];
            dim_offset++;

            // Read 'rank' extent values.
            for (int d = 0; d < rank && dim_offset < ndims; ++d) {
                var.dimensions.push_back(dims[dim_offset]);
                dim_offset++;
            }

            host_desc.variables.push_back(std::move(var));
            ptr_idx++;
        }

        // Call the orchestrator's initialize method.
        int rc = g_orchestrator.initialize(host_desc);
        if (rc != 0) {
            copy_errmsg(g_orchestrator.last_error_msg(), errmsg, errmsg_len);
        }
        return rc;

    } catch (const std::exception& e) {
        const std::string msg = std::string("ccpp_framework_init: ") + e.what();
        std::cerr << msg << std::endl;
        copy_errmsg(msg, errmsg, errmsg_len);
        return 1;
    } catch (...) {
        const std::string msg = "ccpp_framework_init: Unknown C++ exception";
        std::cerr << msg << std::endl;
        copy_errmsg(msg, errmsg, errmsg_len);
        return 1;
    }
}

int ccpp_framework_run(
    const char* suite_name,
    const char* group_name,
    const char* phase,
    int         col_start,
    int         col_end,
    int         thread_idx,
    char*       errmsg,
    int         errmsg_len)
{
    try {
        clear_errmsg(errmsg, errmsg_len);

        // Convert C strings to std::string for the orchestrator call.
        const std::string s_suite = (suite_name != nullptr) ? suite_name : "";
        const std::string s_group = (group_name != nullptr) ? group_name : "";
        const std::string s_phase = (phase != nullptr) ? phase : "";

        int rc = g_orchestrator.run(s_suite, s_group, s_phase,
                                    col_start, col_end, thread_idx);
        if (rc != 0) {
            copy_errmsg(g_orchestrator.last_error_msg(), errmsg, errmsg_len);
        }
        return rc;

    } catch (const std::exception& e) {
        const std::string msg = std::string("ccpp_framework_run: ") + e.what();
        std::cerr << msg << std::endl;
        copy_errmsg(msg, errmsg, errmsg_len);
        return 1;
    } catch (...) {
        const std::string msg = "ccpp_framework_run: Unknown C++ exception";
        std::cerr << msg << std::endl;
        copy_errmsg(msg, errmsg, errmsg_len);
        return 1;
    }
}

int ccpp_framework_finalize(
    char* errmsg,
    int   errmsg_len)
{
    try {
        clear_errmsg(errmsg, errmsg_len);

        int rc = g_orchestrator.finalize();
        if (rc != 0) {
            copy_errmsg(g_orchestrator.last_error_msg(), errmsg, errmsg_len);
        }
        return rc;

    } catch (const std::exception& e) {
        const std::string msg = std::string("ccpp_framework_finalize: ") + e.what();
        std::cerr << msg << std::endl;
        copy_errmsg(msg, errmsg, errmsg_len);
        return 1;
    } catch (...) {
        const std::string msg = "ccpp_framework_finalize: Unknown C++ exception";
        std::cerr << msg << std::endl;
        copy_errmsg(msg, errmsg, errmsg_len);
        return 1;
    }
}

} // extern "C"
