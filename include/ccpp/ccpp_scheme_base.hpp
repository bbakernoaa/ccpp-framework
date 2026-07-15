/// @file ccpp_scheme_base.hpp
/// @brief Polymorphic scheme base class and error type for the CCPP C++ Framework.
///
/// Defines the abstract interface through which the pipeline dispatches to
/// physics scheme implementations (Fortran proxies or native C++).
#pragma once

#include <string>

namespace ccpp {

// Forward declarations for types defined in other headers.
struct SchemeContext;
class ConstituentRegistry;

/// Error state returned from every scheme phase call.
/// A default-constructed SchemeError represents success (code == 0).
struct SchemeError {
    int code = 0;
    std::string message;

    /// Contextual conversion to bool: true means an error occurred.
    explicit operator bool() const { return code != 0; }
};

/// Abstract base class for all scheme implementations.
///
/// The pipeline invokes schemes exclusively through this interface,
/// enabling language-agnostic dispatch (Fortran proxies, native C++, Kokkos).
/// Default phase implementations are no-ops so that derived classes only
/// need to override the phases they actually implement.
class SchemeBase {
public:
    virtual ~SchemeBase() = default;

    /// Returns the scheme's canonical name (e.g., "mp_thompson").
    virtual const std::string& name() const = 0;

    /// Physics phase entry points.
    /// Default implementations return empty SchemeError (no-op / success).
    virtual SchemeError phase_init(SchemeContext& ctx)           { return {}; }
    virtual SchemeError phase_timestep_init(SchemeContext& ctx)  { return {}; }
    virtual SchemeError phase_run(SchemeContext& ctx)            { return {}; }
    virtual SchemeError phase_timestep_final(SchemeContext& ctx) { return {}; }
    virtual SchemeError phase_final(SchemeContext& ctx)          { return {}; }

    /// Registration phase for constituent declaration.
    virtual SchemeError phase_register(ConstituentRegistry& reg) { return {}; }
};

} // namespace ccpp
