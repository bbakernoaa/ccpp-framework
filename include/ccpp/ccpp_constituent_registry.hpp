/// @file ccpp_constituent_registry.hpp
/// @brief Constituent metadata and registry for the CCPP C++ Framework.
///
/// Provides ConstituentMetadata (describes a physics constituent) and
/// ConstituentRegistry (register, bind storage, retrieve mdspan views).
#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "ccpp_mdspan.hpp"

namespace ccpp {

/// Metadata describing a single constituent (tracer, species, etc.).
struct ConstituentMetadata {
    std::string name;                        ///< Short name (e.g., "qv")
    std::string standard_name;               ///< Standard name used for lookup
    std::string units;                       ///< Physical units (e.g., "kg kg-1")
    std::vector<std::string> dimensions;     ///< Dimension names (e.g., {"horizontal_dimension", "vertical_layer_dimension"})
    bool advected = false;                   ///< Whether this constituent is advected by the dynamical core
    double molar_mass = 0.0;                 ///< Molar mass in kg/mol (0 if not applicable)
    double default_value = 0.0;              ///< Default initial value
};

/// Registry that collects constituent declarations from schemes and
/// provides mdspan views to bound storage during execution.
///
/// Lifecycle:
///   1. During scheme registration phase, schemes call declare() to register metadata.
///   2. After registration, the host allocates storage and calls bind_storage().
///   3. During execution, schemes call get_view() to obtain typed mdspan views.
class ConstituentRegistry {
public:
    /// Register a constituent during the scheme registration phase.
    /// Indexes by standard_name for subsequent lookups.
    /// @throws std::runtime_error if a constituent with the same standard_name already exists.
    void declare(const ConstituentMetadata& meta);

    /// Bind host-allocated storage to a previously declared constituent.
    /// @param name   The standard_name of the constituent.
    /// @param data   Pointer to the host-allocated array (column-major).
    /// @param dims   Extents of the 3D array (e.g., {ncol, nlay, nblk}).
    /// @throws std::runtime_error if the constituent name is not found.
    void bind_storage(const std::string& name, double* data,
                      const std::array<int, 3>& dims);

    /// Retrieve an mdspan view for a bound constituent.
    /// @param name The standard_name of the constituent.
    /// @return     A real64_3d mdspan wrapping the bound storage.
    /// @throws std::runtime_error if the constituent is not found or storage is not bound.
    real64_3d get_view(const std::string& name) const;

    /// Number of registered constituents.
    size_t count() const { return entries_.size(); }

    /// Access all registered constituent metadata.
    const std::vector<ConstituentMetadata>& all() const { return entries_; }

private:
    std::vector<ConstituentMetadata> entries_;
    std::unordered_map<std::string, size_t> name_index_;
    std::unordered_map<std::string, double*> storage_;
    std::unordered_map<std::string, std::array<int, 3>> storage_dims_;
};

} // namespace ccpp
