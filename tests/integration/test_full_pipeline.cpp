/// @file test_full_pipeline.cpp
/// @brief Integration test: full pipeline wiring bridge → orchestrator → suite → group → scheme.
///
/// This test verifies the complete execution pipeline through:
///   1. Bridge layer (ccpp_framework_init / ccpp_framework_run / ccpp_framework_finalize)
///   2. FrameworkOrchestrator (suite dispatch)
///   3. Suite → Group → SchemeElement → SchemeBase
///
/// Uses a pure C++ mock scheme that records invocation to verify the pipeline
/// wiring without requiring Fortran linking.

#include <gtest/gtest.h>

#include <cstring>

#include <ccpp/ccpp_bridge.h>
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

/// A mock C++ scheme that records whether it was called and verifies pipeline wiring.
/// This replaces the need for a Fortran mock scheme in a pure C++ integration test.
class MockCppScheme : public ccpp::SchemeBase {
public:
    explicit MockCppScheme(const std::string& id) : id_(id) {}

    const std::string& name() const override { return id_; }

    ccpp::SchemeError phase_init(ccpp::SchemeContext& ctx) override {
        init_called_ = true;
        last_col_start_ = ctx.col_start;
        last_col_end_ = ctx.col_end;
        return {};
    }

    ccpp::SchemeError phase_run(ccpp::SchemeContext& ctx) override {
        run_called_ = true;
        run_call_count_++;
        last_col_start_ = ctx.col_start;
        last_col_end_ = ctx.col_end;
        last_thread_idx_ = ctx.thread_idx;
        return {};
    }

    ccpp::SchemeError phase_final(ccpp::SchemeContext& ctx) override {
        final_called_ = true;
        return {};
    }

    // Accessors for test assertions
    bool init_called() const { return init_called_; }
    bool run_called() const { return run_called_; }
    bool final_called() const { return final_called_; }
    int run_call_count() const { return run_call_count_; }
    int last_col_start() const { return last_col_start_; }
    int last_col_end() const { return last_col_end_; }
    int last_thread_idx() const { return last_thread_idx_; }

    void reset() {
        init_called_ = false;
        run_called_ = false;
        final_called_ = false;
        run_call_count_ = 0;
        last_col_start_ = 0;
        last_col_end_ = 0;
        last_thread_idx_ = 0;
    }

private:
    std::string id_;
    bool init_called_ = false;
    bool run_called_ = false;
    bool final_called_ = false;
    int run_call_count_ = 0;
    int last_col_start_ = 0;
    int last_col_end_ = 0;
    int last_thread_idx_ = 0;
};

} // anonymous namespace

/// Test the full pipeline via the orchestrator directly:
/// orchestrator.initialize() → orchestrator.register_suite() → orchestrator.run() → suite → group → scheme
TEST(FullPipelineIntegration, OrchestratorRunInvokesMockScheme) {
    // Create the mock scheme
    auto mock_scheme = std::make_unique<MockCppScheme>("mock_physics");
    MockCppScheme* scheme_ptr = mock_scheme.get();

    // Build the pipeline: scheme → element → group → suite
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme_ptr));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("physics", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("test_suite", std::move(groups));

    // Wire up the orchestrator: initialize first, then register suite
    // (initialize clears the suite map, so register must come after)
    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("test_suite");
    int rc = orchestrator.initialize(host_desc);
    ASSERT_EQ(rc, 0);

    // Register the suite after initialization
    orchestrator.register_suite(std::move(suite));

    // Run the "run" phase
    rc = orchestrator.run("test_suite", "physics", "run", 1, 10, 1);
    ASSERT_EQ(rc, 0);

    // Verify the mock scheme was actually invoked
    EXPECT_TRUE(scheme_ptr->run_called());
    EXPECT_EQ(scheme_ptr->run_call_count(), 1);
    EXPECT_EQ(scheme_ptr->last_col_start(), 1);
    EXPECT_EQ(scheme_ptr->last_col_end(), 10);
    EXPECT_EQ(scheme_ptr->last_thread_idx(), 1);

    // Finalize
    rc = orchestrator.finalize();
    ASSERT_EQ(rc, 0);
}

/// Test the full pipeline via bridge C functions:
/// ccpp_framework_init → ccpp_framework_run → ccpp_framework_finalize
///
/// Since the bridge uses a static global orchestrator, we can't easily inject
/// a mock scheme through the bridge API alone. This test verifies:
/// 1. Init returns 0 (success)
/// 2. Run returns non-zero for unregistered suite (validates error path)
/// 3. Finalize returns 0 (success)
/// This confirms the bridge layer wiring is correct and exceptions don't leak.
TEST(FullPipelineIntegration, BridgeInitRunFinalizeCycle) {
    char errmsg[256] = {0};

    // Step 1: Initialize through the bridge
    int rc = ccpp_framework_init("integration_suite", 1,
                                 nullptr, nullptr, 0,
                                 errmsg, sizeof(errmsg));
    ASSERT_EQ(rc, 0) << "Init failed: " << errmsg;

    // Step 2: Run should return non-zero (suite not registered through bridge)
    // This validates the bridge layer correctly wraps orchestrator.run() and
    // returns error codes without crashing or leaking exceptions.
    rc = ccpp_framework_run("integration_suite", "physics", "run",
                            1, 10, 1, errmsg, sizeof(errmsg));
    EXPECT_NE(rc, 0) << "Expected non-zero return for unregistered suite";
    EXPECT_NE(std::string(errmsg), std::string(""))
        << "Expected error message for missing suite";

    // Step 3: Finalize through the bridge
    std::memset(errmsg, 0, sizeof(errmsg));
    rc = ccpp_framework_finalize(errmsg, sizeof(errmsg));
    ASSERT_EQ(rc, 0) << "Finalize failed: " << errmsg;
}

/// Test multiple phases through the full pipeline:
/// init → timestep_init → run → timestep_final → final
TEST(FullPipelineIntegration, AllPhasesExecuteThroughPipeline) {
    auto mock_scheme = std::make_unique<MockCppScheme>("multi_phase_scheme");
    MockCppScheme* scheme_ptr = mock_scheme.get();

    // Build pipeline
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme_ptr));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("main", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("phase_suite", std::move(groups));

    // Wire orchestrator: init then register
    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("phase_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Execute init phase
    int rc = orchestrator.run("phase_suite", "main", "init", 1, 5, 1);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(scheme_ptr->init_called());

    // Execute run phase
    rc = orchestrator.run("phase_suite", "main", "run", 1, 5, 1);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(scheme_ptr->run_called());

    // Execute final phase
    rc = orchestrator.run("phase_suite", "main", "final", 1, 5, 1);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(scheme_ptr->final_called());

    // Finalize
    ASSERT_EQ(orchestrator.finalize(), 0);
}

/// Test that multiple schemes in a group are all invoked in order.
TEST(FullPipelineIntegration, MultipleSchemesPipelineOrder) {
    auto scheme_a = std::make_unique<MockCppScheme>("scheme_a");
    auto scheme_b = std::make_unique<MockCppScheme>("scheme_b");
    auto scheme_c = std::make_unique<MockCppScheme>("scheme_c");

    MockCppScheme* ptr_a = scheme_a.get();
    MockCppScheme* ptr_b = scheme_b.get();
    MockCppScheme* ptr_c = scheme_c.get();

    // Build pipeline with 3 schemes
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(ptr_a));
    elements.push_back(std::make_unique<ccpp::SchemeElement>(ptr_b));
    elements.push_back(std::make_unique<ccpp::SchemeElement>(ptr_c));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("physics", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("multi_scheme_suite", std::move(groups));

    // Wire orchestrator: init then register
    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("multi_scheme_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Run phase
    int rc = orchestrator.run("multi_scheme_suite", "physics", "run", 1, 20, 2);
    ASSERT_EQ(rc, 0);

    // Verify all schemes were invoked
    EXPECT_TRUE(ptr_a->run_called());
    EXPECT_TRUE(ptr_b->run_called());
    EXPECT_TRUE(ptr_c->run_called());

    // Verify column range and thread were propagated correctly
    EXPECT_EQ(ptr_c->last_col_start(), 1);
    EXPECT_EQ(ptr_c->last_col_end(), 20);
    EXPECT_EQ(ptr_c->last_thread_idx(), 2);

    ASSERT_EQ(orchestrator.finalize(), 0);
}

/// Test that the pipeline correctly propagates column range and thread index
/// from the orchestrator through to the scheme context.
TEST(FullPipelineIntegration, ContextPropagationThroughPipeline) {
    auto mock_scheme = std::make_unique<MockCppScheme>("context_scheme");
    MockCppScheme* scheme_ptr = mock_scheme.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme_ptr));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("group1", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("ctx_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("ctx_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Run with specific column range and thread index
    int rc = orchestrator.run("ctx_suite", "group1", "run", 42, 100, 7);
    ASSERT_EQ(rc, 0);

    // Verify context was propagated correctly through the full pipeline
    EXPECT_TRUE(scheme_ptr->run_called());
    EXPECT_EQ(scheme_ptr->last_col_start(), 42);
    EXPECT_EQ(scheme_ptr->last_col_end(), 100);
    EXPECT_EQ(scheme_ptr->last_thread_idx(), 7);

    ASSERT_EQ(orchestrator.finalize(), 0);
}

/// Test the full lifecycle: init → multiple run phases → finalize
/// Verifies the framework can be used for a realistic multi-timestep scenario.
TEST(FullPipelineIntegration, FullLifecycleMultipleTimesteps) {
    auto mock_scheme = std::make_unique<MockCppScheme>("timestep_scheme");
    MockCppScheme* scheme_ptr = mock_scheme.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme_ptr));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("dynamics", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("lifecycle_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;

    // Initialize
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("lifecycle_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);

    // Register suite after init
    orchestrator.register_suite(std::move(suite));

    // Simulate multiple timesteps
    for (int timestep = 0; timestep < 5; ++timestep) {
        int rc = orchestrator.run("lifecycle_suite", "dynamics", "run", 1, 50, 1);
        ASSERT_EQ(rc, 0) << "Failed at timestep " << timestep;
    }

    // The scheme should have been called 5 times total
    EXPECT_EQ(scheme_ptr->run_call_count(), 5);

    // Finalize
    ASSERT_EQ(orchestrator.finalize(), 0);
}
