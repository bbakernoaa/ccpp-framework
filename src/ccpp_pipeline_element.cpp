/// @file ccpp_pipeline_element.cpp
/// @brief Implementation of PipelineElement subclasses for the CCPP C++ Framework.

#include "ccpp/ccpp_pipeline_element.hpp"

#include <utility>

namespace ccpp {

// --------------------------------------------------------------------------
// SchemeElement
// --------------------------------------------------------------------------

SchemeError SchemeElement::execute(const std::string& phase, SchemeContext& ctx) {
    if (phase == "init") {
        return scheme_->phase_init(ctx);
    } else if (phase == "timestep_init") {
        return scheme_->phase_timestep_init(ctx);
    } else if (phase == "run") {
        return scheme_->phase_run(ctx);
    } else if (phase == "timestep_final") {
        return scheme_->phase_timestep_final(ctx);
    } else if (phase == "final") {
        return scheme_->phase_final(ctx);
    }
    // Unknown phase: return error
    return SchemeError{1, "Unknown phase: " + phase};
}

// --------------------------------------------------------------------------
// SubcycleElement
// --------------------------------------------------------------------------

SubcycleElement::SubcycleElement(int loop_count,
                                 std::vector<std::unique_ptr<PipelineElement>> children)
    : loop_count_(loop_count),
      children_(std::move(children)) {}

SchemeError SubcycleElement::execute(const std::string& phase, SchemeContext& ctx) {
    // Subcycle looping only applies to the "run" phase. This mirrors the
    // legacy Fortran cap generator (generator/suite_resolver.py's
    // _resolve_flat_phase / suite_cap.py's per-group dispatch): init,
    // timestep_init, timestep_final, and final calls are resolved from a
    // flattened, deduplicated scheme list — each scheme's non-run phase
    // method runs exactly once per group, regardless of how many times it
    // is nested inside a <subcycle loop="N"> for the run phase. Looping
    // N times for e.g. phase_init would erroneously re-initialize the
    // scheme's state N times.
    const int iterations = (phase == "run") ? loop_count_ : 1;
    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& child : children_) {
            SchemeError err = child->execute(phase, ctx);
            if (err) {
                return err;
            }
        }
    }
    return {};
}

} // namespace ccpp
