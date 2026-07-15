/// @file ccpp_scheme_context.hpp
/// @brief Per-call context passed to every scheme phase in the CCPP C++ Framework.
///
/// SchemeContext is a simple aggregate that carries column range, threading info,
/// and pointers to shared framework resources (host views, suite data, constituents).
#pragma once

namespace ccpp {

// Forward declarations for types defined in other headers.
// These avoid circular includes since SchemeContext is referenced by SchemeBase
// and PipelineElement, which in turn reference these types.
struct HostStateViews;
class ConstituentRegistry;

/// Per-call context passed to every scheme phase.
///
/// Fields:
///   col_start     - First column index (horizontal_loop_begin, 1-based Fortran convention)
///   col_end       - Last column index  (horizontal_loop_end, 1-based Fortran convention)
///   thread_idx    - Thread number (1-based)
///   instance_idx  - Instance number (1-based)
///   host_views    - Pointer to host state mdspan views (valid for framework lifetime)
///   suite_data    - Pointer to suite-owned interstitial data (opaque)
///   constituents  - Pointer to the constituent registry for this suite
struct SchemeContext {
    int col_start;       // horizontal_loop_begin
    int col_end;         // horizontal_loop_end
    int thread_idx;       // thread_number (1-based)
    int instance_idx;    // instance_number (1-based)

    // Host state views (set during init, valid for framework lifetime)
    HostStateViews* host_views = nullptr;

    // Suite-owned interstitial data
    void* suite_data = nullptr;

    // Constituent registry reference
    ConstituentRegistry* constituents = nullptr;
};

} // namespace ccpp
