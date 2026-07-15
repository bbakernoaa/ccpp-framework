/// @file ccpp_suite.hpp
/// @brief Suite class for the CCPP C++ Framework pipeline.
///
/// A Suite owns groups and optional init/final schemes. It dispatches
/// execution to the named group or runs init/final schemes as appropriate.
#pragma once

#include "ccpp_group.hpp"
#include "ccpp_scheme_base.hpp"
#include "ccpp_scheme_context.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ccpp {

/// A named suite owning groups and optional init/final schemes.
///
/// Suites correspond to `<suite>` elements in Suite XML definitions.
/// `execute_phase()` dispatches to the named group or runs the suite-level
/// init/final scheme depending on the phase and group name.
class Suite {
public:
    /// Construct a Suite with a name, owned groups, and optional init/final schemes.
    /// @param name         Suite identifier (matches Suite XML suite name).
    /// @param groups       Owned groups to dispatch execution to.
    /// @param init_scheme  Optional non-owning pointer to the suite-level init scheme.
    /// @param final_scheme Optional non-owning pointer to the suite-level final scheme.
    explicit Suite(const std::string& name,
                   std::vector<std::unique_ptr<Group>> groups,
                   SchemeBase* init_scheme = nullptr,
                   SchemeBase* final_scheme = nullptr);

    /// Execute a phase for the named group.
    ///
    /// If group_name matches a group's name, dispatches to that group's
    /// execute(). If group_name is empty or the literal "all", every group
    /// is executed in order (short-circuiting on the first error) — this
    /// matches the host-side convention of calling init/timestep_init/
    /// timestep_final/final with group_name="all" to run across every
    /// group, while "run" is always called per named group.
    /// Returns 0 on success, or a non-zero error code on failure.
    /// @param group_name  Name of the group to execute, or "" / "all" for every group.
    /// @param phase       Phase name ("init", "timestep_init", "run", etc.)
    /// @param ctx         Per-call context (column range, threading, host views, etc.)
    /// @return 0 on success, non-zero error code on failure.
    int execute_phase(const std::string& group_name,
                      const std::string& phase,
                      SchemeContext& ctx);

    /// Returns the suite name.
    const std::string& name() const { return name_; }

    /// Returns the owned groups.
    const std::vector<std::unique_ptr<Group>>& groups() const { return groups_; }

private:
    std::string name_;
    std::vector<std::unique_ptr<Group>> groups_;
    SchemeBase* init_scheme_;   // nullable, non-owning
    SchemeBase* final_scheme_;  // nullable, non-owning
};

} // namespace ccpp
