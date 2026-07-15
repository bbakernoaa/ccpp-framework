/// @file ccpp_suite.cpp
/// @brief Implementation of the Suite class for the CCPP C++ Framework.

#include "ccpp/ccpp_suite.hpp"

#include <utility>

namespace ccpp {

Suite::Suite(const std::string& name,
             std::vector<std::unique_ptr<Group>> groups,
             SchemeBase* init_scheme,
             SchemeBase* final_scheme)
    : name_(name),
      groups_(std::move(groups)),
      init_scheme_(init_scheme),
      final_scheme_(final_scheme) {}

int Suite::execute_phase(const std::string& group_name,
                         const std::string& phase,
                         SchemeContext& ctx) {
    // If the phase is "init" and we have a suite-level init scheme, run it.
    if (phase == "init" && init_scheme_ != nullptr) {
        SchemeError err = init_scheme_->phase_init(ctx);
        if (err) {
            return err.code;
        }
    }

    // If the phase is "final" and we have a suite-level final scheme, run it.
    if (phase == "final" && final_scheme_ != nullptr) {
        SchemeError err = final_scheme_->phase_final(ctx);
        if (err) {
            return err.code;
        }
    }

    // An empty group_name or the literal "all" dispatches every group in
    // order, short-circuiting on the first group error — mirroring the
    // legacy Fortran suite cap's `select case(group_name); case('', 'all')`
    // semantics (generator/suite_cap.py::_physics_dispatch_lines). The
    // host (e.g. scm.F90) calls ccpp_physics_init/timestep_init/
    // timestep_final/final with group_name='all' precisely because those
    // phases are meant to run across every group, unlike "run" which is
    // always called per named group ("radiation", "phys_ps", "phys_ts", ...).
    if (group_name.empty() || group_name == "all") {
        for (auto& group : groups_) {
            SchemeError err = group->execute(phase, ctx);
            if (err) {
                return err.code;
            }
        }
        return 0;
    }

    // Dispatch to the named group.
    for (auto& group : groups_) {
        if (group->name() == group_name) {
            SchemeError err = group->execute(phase, ctx);
            return err.code;
        }
    }

    // Group not found — return an error code.
    return -1;
}

} // namespace ccpp
