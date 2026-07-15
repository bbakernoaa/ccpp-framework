/// @file ccpp_framework_orchestrator.cpp
/// @brief Implementation of the CCPP Framework Orchestrator.
///
/// Provides initialize()/run()/finalize()/constituents() implementations,
/// including auto-construction of suites from generated factories
/// registered via register_suite_factory().

#include "ccpp/ccpp_framework_orchestrator.hpp"
#include "ccpp/ccpp_scheme_context.hpp"

#include <stdexcept>

namespace ccpp {

int FrameworkOrchestrator::initialize(const HostStateDescriptor& host_desc) {
    // Store the host descriptor for lifetime reference.
    host_desc_ = host_desc;

    // Clear any prior state.
    suites_.clear();
    constituent_registry_ = ConstituentRegistry{};
    last_error_ = 0;
    last_error_msg_.clear();

    // Auto-construct suites from any capgen-ng generated factories that
    // have been registered for the requested suite names (see
    // register_suite_factory()). Test/integration code that has no
    // generated factory for a suite name may still call register_suite()
    // manually after initialize() returns — this loop only handles names
    // with a known factory and leaves everything else untouched.
    const auto& factories = factory_registry();
    for (const auto& suite_name : host_desc.suite_names) {
        auto it = factories.find(suite_name);
        if (it != factories.end() && it->second != nullptr) {
            std::unique_ptr<Suite> suite = it->second();
            if (suite) {
                suites_[suite_name] = std::move(suite);
            }
        }
    }

    // Invoke scheme registration phases on any registered suites (either
    // just auto-constructed above, or registered manually beforehand).
    // This collects constituent declarations from all schemes.
    for (auto& [suite_name, suite_ptr] : suites_) {
        // Registration is done via a special "register" phase dispatch.
        // Individual schemes implement phase_register() to declare constituents.
        // The full registration flow will be wired when suite factories are ready.
        (void)suite_name;
        (void)suite_ptr;
    }

    return 0;
}

int FrameworkOrchestrator::run(const std::string& suite_name,
                               const std::string& group_name,
                               const std::string& phase,
                               int col_start, int col_end,
                               int thread_idx) {
    // Look up the suite by name.
    auto it = suites_.find(suite_name);
    if (it == suites_.end()) {
        last_error_ = 1;
        last_error_msg_ = "Suite not found: " + suite_name;
        return last_error_;
    }

    // Construct the per-call context.
    SchemeContext ctx{};
    ctx.col_start = col_start;
    ctx.col_end = col_end;
    ctx.thread_idx = thread_idx;
    ctx.instance_idx = 1;
    ctx.constituents = &constituent_registry_;

    // Dispatch to the suite's execute_phase method.
    int rc = it->second->execute_phase(group_name, phase, ctx);
    if (rc != 0) {
        last_error_ = rc;
        last_error_msg_ = "Suite '" + suite_name + "' phase '" + phase
                          + "' group '" + group_name + "' returned error code "
                          + std::to_string(rc);
    }
    return rc;
}

int FrameworkOrchestrator::finalize() {
    // Tear down all suite resources.
    suites_.clear();

    // Clear the constituent registry.
    constituent_registry_ = ConstituentRegistry{};

    // Reset error state.
    last_error_ = 0;
    last_error_msg_.clear();

    // Clear stored host descriptor.
    host_desc_ = HostStateDescriptor{};

    return 0;
}

ConstituentRegistry& FrameworkOrchestrator::constituents(const std::string& /*suite_name*/) {
    // Currently a single shared registry across all suites.
    // The suite_name parameter is reserved for future per-suite registries.
    return constituent_registry_;
}

void FrameworkOrchestrator::register_suite(std::unique_ptr<Suite> suite) {
    if (!suite) {
        return;
    }
    const std::string name = suite->name();
    suites_[name] = std::move(suite);
}

std::unordered_map<std::string, FrameworkOrchestrator::SuiteFactoryFn>&
FrameworkOrchestrator::factory_registry() {
    // Function-local static: safe to call from static-initialization-order
    // sensitive contexts (generated registration TUs call
    // register_suite_factory() from their own static initializers).
    static std::unordered_map<std::string, SuiteFactoryFn> registry;
    return registry;
}

void FrameworkOrchestrator::register_suite_factory(
    const std::string& suite_name, SuiteFactoryFn factory_fn) {
    factory_registry()[suite_name] = factory_fn;
}

} // namespace ccpp
