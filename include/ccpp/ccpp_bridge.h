/// @file ccpp_bridge.h
/// @brief C-linkage bridge functions for the CCPP C++ Framework.
///
/// These functions form the ABI boundary between Fortran (via iso_c_binding)
/// and the C++ framework core. Every function catches all C++ exceptions
/// internally and returns a non-zero integer status code with an error
/// message copied into the caller-provided buffer — no exception ever
/// crosses this boundary.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the C++ framework from host-provided suite name(s) and
/// host variable pointer/dimension arrays.
///
/// @param suite_name  Null-terminated suite name string.
/// @param num_suites   Number of suites represented by suite_name (currently 1).
/// @param host_ptrs    Array of raw pointers to host variable storage.
/// @param dims         Flattened rank-prefixed dimension array: for each
///                      host pointer, one rank value followed by that many
///                      extents, e.g. for a 2D array of shape (10,20):
///                      [2, 10, 20].
/// @param ndims        Total number of entries in the dims array.
/// @param errmsg        Caller-provided buffer to receive an error message.
/// @param errmsg_len    Size of the errmsg buffer in bytes.
/// @return 0 on success, non-zero error code on failure.
int ccpp_framework_init(
    const char* suite_name,
    int         num_suites,
    void**      host_ptrs,
    const int*  dims,
    int         ndims,
    char*       errmsg,
    int         errmsg_len);

/// Run a suite/group/phase for the given column range and thread index.
///
/// @param suite_name  Null-terminated suite name string.
/// @param group_name  Null-terminated group name string ("" or "all" for every group).
/// @param phase       Null-terminated phase name string ("init", "run", etc.)
/// @param col_start   First column index (1-based).
/// @param col_end     Last column index (1-based).
/// @param thread_idx  Thread number (1-based).
/// @param errmsg      Caller-provided buffer to receive an error message.
/// @param errmsg_len  Size of the errmsg buffer in bytes.
/// @return 0 on success, non-zero error code on failure.
int ccpp_framework_run(
    const char* suite_name,
    const char* group_name,
    const char* phase,
    int         col_start,
    int         col_end,
    int         thread_idx,
    char*       errmsg,
    int         errmsg_len);

/// Finalize the C++ framework, releasing all suite resources.
///
/// @param errmsg      Caller-provided buffer to receive an error message.
/// @param errmsg_len  Size of the errmsg buffer in bytes.
/// @return 0 on success, non-zero error code on failure.
int ccpp_framework_finalize(
    char* errmsg,
    int   errmsg_len);

#ifdef __cplusplus
} // extern "C"
#endif
