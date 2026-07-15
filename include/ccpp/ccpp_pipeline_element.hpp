/// @file ccpp_pipeline_element.hpp
/// @brief Pipeline element hierarchy for the CCPP C++ Framework.
///
/// Defines the abstract PipelineElement interface and concrete implementations:
/// - SchemeElement: dispatches to a SchemeBase* based on phase name
/// - SubcycleElement: iterates child elements N times sequentially
#pragma once

#include "ccpp_scheme_base.hpp"
#include "ccpp_scheme_context.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ccpp {

/// Base class for pipeline items (scheme call or subcycle loop).
///
/// The pipeline executes elements sequentially. Each element returns a
/// SchemeError; a non-zero code short-circuits the remaining elements.
class PipelineElement {
public:
    virtual ~PipelineElement() = default;

    /// Execute this element for the given phase with the provided context.
    /// @param phase  Phase name ("init", "timestep_init", "run", "timestep_final", "final")
    /// @param ctx    Per-call context (column range, threading, host views, etc.)
    /// @return SchemeError with code==0 on success, non-zero on failure.
    virtual SchemeError execute(const std::string& phase, SchemeContext& ctx) = 0;
};

/// A single scheme call site in the pipeline.
///
/// Dispatches to the correct SchemeBase phase method based on the phase string.
/// The scheme pointer is non-owning; the Suite or Group owns scheme lifetimes.
class SchemeElement : public PipelineElement {
public:
    /// Construct a SchemeElement wrapping a non-owning scheme pointer.
    /// @param scheme Pointer to the scheme implementation (must outlive this element).
    explicit SchemeElement(SchemeBase* scheme) : scheme_(scheme) {}

    /// Dispatches to the appropriate SchemeBase phase method.
    SchemeError execute(const std::string& phase, SchemeContext& ctx) override;

private:
    SchemeBase* scheme_;  // non-owning; Suite owns the scheme lifetime
};

/// A subcycle loop wrapping child pipeline elements.
///
/// Executes all children sequentially for loop_count iterations when the
/// phase is "run". For all other phases (init, timestep_init,
/// timestep_final, final) children execute exactly once regardless of
/// loop_count, matching the legacy Fortran cap generator's flattened,
/// deduplicated non-run phase dispatch (a scheme's init/final entry point
/// must run exactly once per group, not once per subcycle iteration).
/// Short-circuits on the first non-zero error code from any child.
class SubcycleElement : public PipelineElement {
public:
    /// Construct a SubcycleElement with a loop count and owned child elements.
    /// @param loop_count Number of iterations to execute children during
    ///                   the "run" phase only.
    /// @param children   Owned pipeline elements to iterate over.
    SubcycleElement(int loop_count,
                    std::vector<std::unique_ptr<PipelineElement>> children);

    /// Iterates children loop_count_ times for phase=="run"; exactly once
    /// for all other phases. Short-circuits on first error.
    SchemeError execute(const std::string& phase, SchemeContext& ctx) override;

private:
    int loop_count_;
    std::vector<std::unique_ptr<PipelineElement>> children_;
};

} // namespace ccpp
