/// @file test_mixed_pipeline.cpp
/// @brief Integration test: mixed Fortran-proxy-style and native C++ scheme
/// pipeline, verifying both execute in correct order through the
/// polymorphic SchemeBase interface.
///
/// A "Fortran proxy" scheme is simulated here as a C++ class that just
/// records its invocation (real Fortran linkage is exercised at the SCM
/// build level, not in these framework-only unit tests).

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <ccpp/ccpp_framework_orchestrator.hpp>
#include <ccpp/ccpp_group.hpp>
#include <ccpp/ccpp_pipeline_element.hpp>
#include <ccpp/ccpp_scheme_base.hpp>
#include <ccpp/ccpp_scheme_context.hpp>
#include <ccpp/ccpp_suite.hpp>

namespace {

/// Records every invocation in a shared trace, tagging entries with a
/// language label so tests can verify interleaving order across both
/// "Fortran proxy" and native C++ scheme types.
class TaggedScheme : public ccpp::SchemeBase {
public:
    TaggedScheme(std::string id, std::string tag, std::vector<std::string>& trace)
        : id_(std::move(id)), tag_(std::move(tag)), trace_(trace) {}

    const std::string& name() const override { return id_; }

    ccpp::SchemeError phase_init(ccpp::SchemeContext&) override {
        trace_.push_back(tag_ + ":" + id_ + ":init");
        return {};
    }
    ccpp::SchemeError phase_run(ccpp::SchemeContext&) override {
        trace_.push_back(tag_ + ":" + id_ + ":run");
        return {};
    }
    ccpp::SchemeError phase_final(ccpp::SchemeContext&) override {
        trace_.push_back(tag_ + ":" + id_ + ":final");
        return {};
    }

private:
    std::string id_;
    std::string tag_;
    std::vector<std::string>& trace_;
};

} // anonymous namespace

/// A mixed pipeline (Fortran-proxy-style scheme followed by a native C++
/// scheme) executes both in the correct sequential order for the run phase.
TEST(MixedPipelineIntegration, FortranAndCppSchemesExecuteInOrder) {
    std::vector<std::string> trace;

    auto fortran_like = std::make_unique<TaggedScheme>("mp_thompson", "fortran", trace);
    auto native_cpp = std::make_unique<TaggedScheme>("native_radiation", "cpp", trace);

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(fortran_like.get()));
    elements.push_back(std::make_unique<ccpp::SchemeElement>(native_cpp.get()));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("mixed_group", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("mixed_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("mixed_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Execute the "run" phase
    int rc = orchestrator.run("mixed_suite", "mixed_group", "run", 1, 10, 1);
    ASSERT_EQ(rc, 0);

    ASSERT_EQ(trace.size(), 2u);
    EXPECT_EQ(trace[0], "fortran:mp_thompson:run");
    EXPECT_EQ(trace[1], "cpp:native_radiation:run");

    ASSERT_EQ(orchestrator.finalize(), 0);
}

/// Verifies both scheme types respond correctly to every phase, not just run.
TEST(MixedPipelineIntegration, BothSchemeTypesRespondToAllPhases) {
    std::vector<std::string> trace;

    auto fortran_like = std::make_unique<TaggedScheme>("scheme_a", "fortran", trace);
    auto native_cpp = std::make_unique<TaggedScheme>("scheme_b", "cpp", trace);

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(fortran_like.get()));
    elements.push_back(std::make_unique<ccpp::SchemeElement>(native_cpp.get()));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("interleaved_group", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("interleaved_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("interleaved_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Execute
    ASSERT_EQ(orchestrator.run("interleaved_suite", "interleaved_group", "init", 1, 10, 1), 0);
    ASSERT_EQ(orchestrator.run("interleaved_suite", "interleaved_group", "run", 1, 10, 1), 0);
    ASSERT_EQ(orchestrator.run("interleaved_suite", "interleaved_group", "final", 1, 10, 1), 0);

    std::vector<std::string> expected = {
        "fortran:scheme_a:init", "cpp:scheme_b:init",
        "fortran:scheme_a:run",  "cpp:scheme_b:run",
        "fortran:scheme_a:final", "cpp:scheme_b:final",
    };
    ASSERT_EQ(trace, expected);

    ASSERT_EQ(orchestrator.finalize(), 0);
}

/// A radiation-style group with a subcycle containing a mix of both scheme
/// types must still preserve ordering and phase-gated looping.
TEST(MixedPipelineIntegration, SubcycleWithMixedSchemesPreservesOrder) {
    std::vector<std::string> trace;

    auto fortran_like = std::make_unique<TaggedScheme>("rrtmg_sw", "fortran", trace);
    auto native_cpp = std::make_unique<TaggedScheme>("native_post", "cpp", trace);

    std::vector<std::unique_ptr<ccpp::PipelineElement>> children;
    children.push_back(std::make_unique<ccpp::SchemeElement>(fortran_like.get()));
    children.push_back(std::make_unique<ccpp::SchemeElement>(native_cpp.get()));

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(
        std::make_unique<ccpp::SubcycleElement>(3, std::move(children)));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("rad_suite_group", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("rad_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("rad_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Execute init phase
    ASSERT_EQ(orchestrator.run("rad_suite", "rad_suite_group", "init", 1, 10, 1), 0);
    ASSERT_EQ(trace.size(), 2u) << "init must run once despite loop_count=3";

    trace.clear();
    ASSERT_EQ(orchestrator.run("rad_suite", "rad_suite_group", "run", 1, 10, 1), 0);
    ASSERT_EQ(trace.size(), 6u) << "run must loop 3 times * 2 schemes";

    ASSERT_EQ(orchestrator.finalize(), 0);
}
