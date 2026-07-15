/// @file test_bridge_success_property.cpp
/// @brief Property-based test for successful execution returning zero (Property 6).
///
/// Feature: cpp-bridge-framework, Property 6: Successful Execution Returns Zero
/// **Validates: Requirements 7.3**
///
/// For any valid pipeline configuration built from mock schemes that always
/// succeed (return a default-constructed SchemeError), running any phase
/// through FrameworkOrchestrator::run() must always return 0.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <ccpp/ccpp_framework_orchestrator.hpp>
#include <ccpp/ccpp_group.hpp>
#include <ccpp/ccpp_pipeline_element.hpp>
#include <ccpp/ccpp_scheme_base.hpp>
#include <ccpp/ccpp_scheme_context.hpp>
#include <ccpp/ccpp_suite.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

/// A scheme that always succeeds on every phase.
class AlwaysSucceedsScheme : public ccpp::SchemeBase {
public:
    explicit AlwaysSucceedsScheme(std::string id) : id_(std::move(id)) {}
    const std::string& name() const override { return id_; }
    // All phase_* default implementations already return SchemeError{} (success).

private:
    std::string id_;
};

} // anonymous namespace

/// Property 6: any suite built entirely from always-succeeding mock schemes
/// returns fail_flag == 0 for every phase/group combination.
RC_GTEST_PROP(BridgeSuccess, AlwaysSucceedingPipelineReturnsZero, ()) {
    const auto num_groups = *rc::gen::inRange(1, 5);
    const auto num_schemes_per_group = *rc::gen::inRange(1, 6);

    std::vector<std::unique_ptr<AlwaysSucceedsScheme>> schemes;
    std::vector<std::unique_ptr<ccpp::Group>> groups;
    std::vector<std::string> group_names;

    for (int g = 0; g < num_groups; ++g) {
        std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
        for (int s = 0; s < num_schemes_per_group; ++s) {
            schemes.push_back(std::make_unique<AlwaysSucceedsScheme>(
                "s" + std::to_string(g) + "_" + std::to_string(s)));
            elements.push_back(
                std::make_unique<ccpp::SchemeElement>(schemes.back().get()));
        }
        std::string gname = "group_" + std::to_string(g);
        group_names.push_back(gname);
        groups.push_back(std::make_unique<ccpp::Group>(gname, std::move(elements)));
    }

    auto suite = std::make_unique<ccpp::Suite>("success_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orch;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("success_suite");
    RC_ASSERT(orch.initialize(host_desc) == 0);
    orch.register_suite(std::move(suite));

    static const char* phases[] = {
        "init", "timestep_init", "run", "timestep_final", "final"};

    for (const auto& gname : group_names) {
        for (const char* phase : phases) {
            int rc = orch.run("success_suite", gname, phase, 1, 10, 1);
            RC_ASSERT(rc == 0);
        }
    }

    // group_name="all" must also succeed across every group.
    for (const char* phase : phases) {
        int rc = orch.run("success_suite", "all", phase, 1, 10, 1);
        RC_ASSERT(rc == 0);
    }

    RC_ASSERT(orch.finalize() == 0);
}
