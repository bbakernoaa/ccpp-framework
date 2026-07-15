/// @file ccpp_host_state_descriptor.hpp
/// @brief Descriptor structs for host variable state passed during framework initialization.
///
/// HostVarDescriptor describes a single host-owned variable (pointer, dimensions, type info).
/// HostStateDescriptor aggregates all host variables and suite names for bridge-layer construction.
#pragma once

#include <string>
#include <vector>

namespace ccpp {

/// Describes one host variable's storage for bridge-layer construction.
///
/// Fields:
///   standard_name - CCPP standard name identifying the variable
///   data_ptr      - Raw pointer to the host-owned array storage
///   dimensions    - Column-major extents of the array
///   fortran_type  - Fortran type string ("real", "integer", "character")
///   kind          - Fortran kind parameter ("kind_phys", "REAL64", etc.)
///   intent        - Variable intent ("in", "out", "inout")
struct HostVarDescriptor {
    std::string standard_name;
    void* data_ptr = nullptr;
    std::vector<int> dimensions;   // column-major extents
    std::string fortran_type;      // "real", "integer", "character"
    std::string kind;              // "kind_phys", "REAL64", etc.
    std::string intent;            // "in", "out", "inout"
};

/// Full host state descriptor passed during initialization.
///
/// Fields:
///   host_name   - Identifier for the host model
///   variables   - Collection of all host variable descriptors
///   suite_names - Names of suites to register during initialization
struct HostStateDescriptor {
    std::string host_name;
    std::vector<HostVarDescriptor> variables;
    std::vector<std::string> suite_names;
};

/// Runtime view over host state variables, indexed by CCPP standard name.
///
/// Constructed by FrameworkOrchestrator::initialize() from a
/// HostStateDescriptor and referenced (non-owning) by every SchemeContext
/// for the framework's lifetime via SchemeContext::host_views. This is the
/// concrete definition for the type forward-declared in
/// ccpp_scheme_context.hpp.
///
/// Today this holds the raw pointer/dimension pairs verbatim from the
/// HostStateDescriptor; typed mdspan accessor wiring (matching capgen-ng's
/// generated host_accessor.hpp naming convention) is completed in a later
/// task once real host variable marshalling exists on the Fortran side.
struct HostStateViews {
    /// Host variable descriptors, in the same order as
    /// HostStateDescriptor::variables.
    std::vector<HostVarDescriptor> variables;

    /// Look up a variable's raw data pointer by CCPP standard name.
    /// @return The pointer, or nullptr if standard_name is not found.
    void* find(const std::string& standard_name) const {
        for (const auto& v : variables) {
            if (v.standard_name == standard_name) {
                return v.data_ptr;
            }
        }
        return nullptr;
    }
};

} // namespace ccpp
