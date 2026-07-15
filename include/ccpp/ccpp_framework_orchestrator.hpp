/// @file ccpp_framework_orchestrator.hpp
/// @brief Root orchestration class for the CCPP C++ Framework.
///
/// FrameworkOrchestrator owns the execution lifecycle: initialization from host
/// state descriptors, phase dispatch through suites/groups, and finalization.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "ccpp_constituent_registry.hpp"
#include "ccpp_host_state_descriptor.hpp"
#include "ccpp_suite.hpp"

namespace ccpp {

/// Central C++ class that owns the execution lifecycle.
///
/// Lifecycle:
///   1. initialize() — Constructs mdspan views from host pointers, auto-constructs
///                     and registers any suite whose name has a matching factory
///                     registered via register_suite_factory(), and invokes scheme
///                     registration phases to collect constituent declarations.
///   2. run()        — Dispatches to a named suite/group/phase with column range
///                     and thread index.
///   3. finalize()   — Tears down all suite resources and clears the registry.
class FrameworkOrchestrator {
public:
    FrameworkOrchestrator() = default;
    ~FrameworkOrchestrator() = default;

    // Non-copyable, movable.
    FrameworkOrchestrator(const FrameworkOrchestrator&) = delete;
    FrameworkOrchestrator& operator=(const FrameworkOrchestrator&) = delete;
    FrameworkOrchestrator(FrameworkOrchestrator&&) = default;
    FrameworkOrchestrator& operator=(FrameworkOrchestrator&&) = default;

    /// Initialize the framework from a host state descriptor.
    ///
    /// Stores host descriptor info, auto-constructs and registers a Suite
    /// for every name in host_desc.suite_names that has a matching factory
    /// registered via register_suite_factory() (see below), and invokes
    /// scheme registration phases for constituent collection.
    ///
    /// @param host_desc  Descriptor containing host variable pointers and suite names.
    /// @return 0 on success, non-zero error code on failure.
    int initialize(const HostStateDescriptor& host_desc);

    /// Execute a suite phase for a named group.
    ///
    /// Looks up the suite by name, constructs a SchemeContext with the provided
    /// column range and thread index, then dispatches to the suite's execute_phase.
    ///
    /// @param suite_name  Name of the suite to dispatch to.
    /// @param group_name  Name of the group within the suite to execute.
    /// @param phase       Phase name ("init", "timestep_init", "run", etc.)
    /// @param col_start   First column index (1-based).
    /// @param col_end     Last column index (1-based).
    /// @param thread_idx  Thread number (1-based, default 1).
    /// @return 0 on success, non-zero error code on failure.
    int run(const std::string& suite_name,
            const std::string& group_name,
            const std::string& phase,
            int col_start, int col_end,
            int thread_idx = 1);

    /// Release all resources (suites, registry, host descriptor).
    ///
    /// @return 0 on success, non-zero error code on failure.
    int finalize();

    /// Access the constituent registry.
    ///
    /// The registry is shared across all suites. Schemes declare constituents
    /// during the registration phase, and the host binds storage afterward.
    ///
    /// @param suite_name  Suite name (reserved for future per-suite registries).
    /// @return Reference to the constituent registry.
    ConstituentRegistry& constituents(const std::string& suite_name);

    /// Returns the last error code set by the orchestrator.
    int last_error() const { return last_error_; }

    /// Returns the last error message set by the orchestrator.
    const std::string& last_error_msg() const { return last_error_msg_; }

    /// Register a pre-built suite (used for testing and integration wiring).
    ///
    /// @param suite  Unique pointer to a fully constructed Suite object.
    void register_suite(std::unique_ptr<Suite> suite);

    /// Signature of a capgen-ng generated suite factory function, e.g.
    /// ``ccpp::generated::create_suite_SCM_GFS_v16``.
    using SuiteFactoryFn = std::unique_ptr<Suite> (*)();

    /// Register a generated suite factory function under *suite_name*.
    ///
    /// initialize() consults this table for every name in
    /// HostStateDescriptor::suite_names and, when a match is found, invokes
    /// the factory and registers the resulting Suite automatically — the
    /// host no longer needs to call register_suite() itself for suites
    /// known to the generated code. Intended to be called once per
    /// compiled-in suite from a small generated translation unit (see
    /// capgen-ng's suite factory emission) before initialize() runs;
    /// typically wired up via a self-registering static initializer emitted
    /// alongside each generated suite_factory_*.cpp.
    ///
    /// @param suite_name  Suite name as it appears in Suite XML / the host's
    ///                    HostStateDescriptor::suite_names.
    /// @param factory_fn  Function pointer returning a fully constructed Suite.
    static void register_suite_factory(const std::string& suite_name,
                                       SuiteFactoryFn factory_fn);

private:
    /// Map of suite name → owned Suite objects.
    std::unordered_map<std::string, std::unique_ptr<Suite>> suites_;

    /// Shared constituent registry (collected during scheme registration phases).
    ConstituentRegistry constituent_registry_;

    /// Last error state for diagnostic queries.
    int last_error_ = 0;
    std::string last_error_msg_;

    /// Stored host state descriptor (kept for lifetime of the framework).
    HostStateDescriptor host_desc_;

    /// Process-wide table of suite name -> generated factory function,
    /// populated via register_suite_factory(). Shared across all
    /// FrameworkOrchestrator instances since generated factories are
    /// compiled-in, not per-instance state.
    static std::unordered_map<std::string, SuiteFactoryFn>& factory_registry();
};

} // namespace ccpp
