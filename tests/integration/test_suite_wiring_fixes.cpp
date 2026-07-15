/// @file test_suite_wiring_fixes.cpp
/// @brief Integration tests for the C++ framework's suite-wiring semantics
///        needed to run a real host-model suite (e.g. SCM_GFS_v16):
///          1. Suite::execute_phase("all"/"") dispatches to every group,
///             matching the legacy Fortran suite cap's group_name='all'
///             convention (host code calls init/timestep_init/
///             timestep_final/final with group_name="all").
///          2. SubcycleElement only repeats loop_count times for the "run"
///             phase; init/timestep_init/timestep_final/final execute
///             exactly once per group regardless of subcycle nesting.
///          3. FrameworkOrchestrator::register_suite_factory() +
///             initialize() auto-constructs and registers a suite from a
///             compiled-in factory, without a manual register_suite() call.

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

/// Records every phase invocation (and the iteration count seen) so tests
/// can assert exact call counts per phase.
class CountingScheme : public ccpp::SchemeBase {
public:
    explicit CountingScheme(const std::string& id) : id_(id) {}

    const std::string& name() const override { return id_; }

    ccpp::SchemeError phase_init(ccpp::SchemeContext&) override {
        ++init_count_;
        return {};
    }
    ccpp::SchemeError phase_timestep_init(ccpp::SchemeContext&) override {
        ++timestep_init_count_;
        return {};
    }
    ccpp::SchemeError phase_run(ccpp::SchemeContext&) override {
        ++run_count_;
        return {};
    }
    ccpp::SchemeError phase_timestep_final(ccpp::SchemeContext&) override {
        ++timestep_final_count_;
        return {};
    }
    ccpp::SchemeError phase_final(ccpp::SchemeContext&) override {
        ++final_count_;
        return {};
    }

    int init_count_ = 0;
    int timestep_init_count_ = 0;
    int run_count_ = 0;
    int timestep_final_count_ = 0;
    int final_count_ = 0;

private:
    std::string id_;
};

} // anonymous namespace

/// group_name="all" (the convention scm.F90 uses for
/// init/timestep_init/timestep_final/final) must dispatch to every group in
/// the suite, not fail with "group not found" just because no group is
/// literally named "all".
TEST(SuiteWiringFixes, ExecutePhaseAllDispatchesEveryGroup) {
    auto scheme_a = std::make_unique<CountingScheme>("a");
    auto scheme_b = std::make_unique<CountingScheme>("b");
    CountingScheme* a = scheme_a.get();
    CountingScheme* b = scheme_b.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elems_a;
    elems_a.push_back(std::make_unique<ccpp::SchemeElement>(a));
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elems_b;
    elems_b.push_back(std::make_unique<ccpp::SchemeElement>(b));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("time_vary", std::move(elems_a)));
    groups.push_back(std::make_unique<ccpp::Group>("radiation", std::move(elems_b)));

    ccpp::Suite suite("test_suite", std::move(groups));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};

    int rc = suite.execute_phase("all", "init", ctx);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(a->init_count_, 1);
    EXPECT_EQ(b->init_count_, 1);

    rc = suite.execute_phase("", "final", ctx);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(a->final_count_, 1);
    EXPECT_EQ(b->final_count_, 1);

    // A specifically-named group must still dispatch only to that group.
    rc = suite.execute_phase("radiation", "run", ctx);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(a->run_count_, 0);
    EXPECT_EQ(b->run_count_, 1);
}

/// group_name="all" must short-circuit on the first group's error and not
/// silently continue into a later group (mirrors the legacy Fortran cap's
/// errflg-checked per-group dispatch).
TEST(SuiteWiringFixes, ExecutePhaseAllShortCircuitsOnFirstGroupError) {
    class FailingScheme : public ccpp::SchemeBase {
    public:
        const std::string& name() const override {
            static const std::string n = "failing";
            return n;
        }
        ccpp::SchemeError phase_init(ccpp::SchemeContext&) override {
            return ccpp::SchemeError{7, "boom"};
        }
    };

    auto failing = std::make_unique<FailingScheme>();
    auto scheme_b = std::make_unique<CountingScheme>("b");
    CountingScheme* b = scheme_b.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elems_a;
    elems_a.push_back(std::make_unique<ccpp::SchemeElement>(failing.get()));
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elems_b;
    elems_b.push_back(std::make_unique<ccpp::SchemeElement>(b));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("first", std::move(elems_a)));
    groups.push_back(std::make_unique<ccpp::Group>("second", std::move(elems_b)));

    ccpp::Suite suite("test_suite", std::move(groups));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};

    int rc = suite.execute_phase("all", "init", ctx);
    EXPECT_EQ(rc, 7);
    // The second group's scheme must never have run.
    EXPECT_EQ(b->init_count_, 0);
}

/// SubcycleElement must only repeat for the "run" phase. A scheme nested
/// inside <subcycle loop="N"> must have its init/timestep_init/
/// timestep_final/final methods invoked exactly once per group execution,
/// never N times, matching the legacy Fortran cap's flattened/deduplicated
/// non-run phase dispatch.
TEST(SuiteWiringFixes, SubcycleOnlyRepeatsForRunPhase) {
    auto scheme = std::make_unique<CountingScheme>("nested");
    CountingScheme* s = scheme.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> children;
    children.push_back(std::make_unique<ccpp::SchemeElement>(s));

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(
        std::make_unique<ccpp::SubcycleElement>(5, std::move(children)));

    ccpp::Group group("phys_ps", std::move(elements));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};

    ASSERT_FALSE(group.execute("init", ctx));
    EXPECT_EQ(s->init_count_, 1) << "init must run exactly once, not loop_count times";

    ASSERT_FALSE(group.execute("timestep_init", ctx));
    EXPECT_EQ(s->timestep_init_count_, 1);

    ASSERT_FALSE(group.execute("run", ctx));
    EXPECT_EQ(s->run_count_, 5) << "run must loop exactly loop_count times";

    ASSERT_FALSE(group.execute("timestep_final", ctx));
    EXPECT_EQ(s->timestep_final_count_, 1);

    ASSERT_FALSE(group.execute("final", ctx));
    EXPECT_EQ(s->final_count_, 1);
}

/// A suite factory registered via register_suite_factory() is automatically
/// constructed and registered by initialize() for any matching name in
/// HostStateDescriptor::suite_names, with no manual register_suite() call
/// required — this is the mechanism the generated suite_factory_*.cpp files
/// use to self-register.
TEST(SuiteWiringFixes, OrchestratorAutoConstructsRegisteredFactory) {
    static bool factory_called = false;
    factory_called = false;

    struct AutoFactoryScheme : public ccpp::SchemeBase {
        const std::string& name() const override {
            static const std::string n = "auto_scheme";
            return n;
        }
    };

    // A factory function matching FrameworkOrchestrator::SuiteFactoryFn's
    // signature: std::unique_ptr<Suite> (*)().
    static auto factory_fn = []() -> std::unique_ptr<ccpp::Suite> {
        factory_called = true;
        std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
        static AutoFactoryScheme scheme_instance;
        elements.push_back(
            std::make_unique<ccpp::SchemeElement>(&scheme_instance));
        std::vector<std::unique_ptr<ccpp::Group>> groups;
        groups.push_back(
            std::make_unique<ccpp::Group>("main", std::move(elements)));
        return std::make_unique<ccpp::Suite>(
            "auto_registered_suite", std::move(groups));
    };

    ccpp::FrameworkOrchestrator::register_suite_factory(
        "auto_registered_suite", factory_fn);

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("auto_registered_suite");

    int rc = orchestrator.initialize(host_desc);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(factory_called)
        << "initialize() must invoke the registered factory automatically";

    // No manual register_suite() call — run() must still find the suite.
    rc = orchestrator.run("auto_registered_suite", "main", "run", 1, 5, 1);
    EXPECT_EQ(rc, 0);

    ASSERT_EQ(orchestrator.finalize(), 0);
}

/// A suite name with no registered factory must behave exactly as before
/// (no auto-construction) — the host/test can still call register_suite()
/// manually after initialize(), preserving backward compatibility with
/// existing integration tests.
TEST(SuiteWiringFixes, UnregisteredSuiteNameRequiresManualRegistration) {
    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.host_name = "test_host";
    host_desc.suite_names.push_back("no_factory_registered_suite_xyz");

    ASSERT_EQ(orchestrator.initialize(host_desc), 0);

    ccpp::SchemeContext ctx{1, 5, 1, 1, nullptr, nullptr, nullptr};
    int rc = orchestrator.run("no_factory_registered_suite_xyz", "main", "run", 1, 5, 1);
    EXPECT_NE(rc, 0) << "Suite was never registered (no factory, no manual call)";

    ASSERT_EQ(orchestrator.finalize(), 0);
}
