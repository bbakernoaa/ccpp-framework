/// @file ccpp_group.hpp
/// @brief Group class for the CCPP C++ Framework pipeline.
///
/// A Group owns a sequence of PipelineElement objects and executes them
/// sequentially, short-circuiting on the first non-zero error code.
#pragma once

#include "ccpp_pipeline_element.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ccpp {

/// A named group of pipeline elements executed sequentially.
///
/// Groups correspond to `<group>` elements in Suite XML definitions.
/// Execution iterates elements in order and stops immediately if any
/// element returns a non-zero SchemeError code.
class Group {
public:
    /// Construct a Group with a name and owned pipeline elements.
    /// @param name     Group identifier (matches Suite XML group name).
    /// @param elements Owned pipeline elements to execute in order.
    explicit Group(const std::string& name,
                   std::vector<std::unique_ptr<PipelineElement>> elements);

    /// Execute all pipeline elements for the given phase.
    ///
    /// Iterates elements sequentially. Short-circuits on the first
    /// element that returns a SchemeError with a non-zero code.
    /// @param phase Phase name ("init", "timestep_init", "run", etc.)
    /// @param ctx   Per-call context (column range, threading, host views, etc.)
    /// @return SchemeError with code==0 on success, or the first error encountered.
    SchemeError execute(const std::string& phase, SchemeContext& ctx);

    /// Returns the group name.
    const std::string& name() const { return name_; }

    /// Returns the number of pipeline elements in this group.
    size_t size() const { return elements_.size(); }

private:
    std::string name_;
    std::vector<std::unique_ptr<PipelineElement>> elements_;
};

} // namespace ccpp
