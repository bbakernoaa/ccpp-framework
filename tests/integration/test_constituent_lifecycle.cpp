/// @file test_constituent_lifecycle.cpp
/// @brief Integration test for the full constituent lifecycle:
/// register -> bind -> access -> finalize, through the full pipeline
/// (orchestrator -> suite -> group -> scheme -> ConstituentRegistry).

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <ccpp/ccpp_constituent_registry.hpp>
#include <ccpp/ccpp_framework_orchestrator.hpp>
#include <ccpp/ccpp_group.hpp>
#include <ccpp/ccpp_pipeline_element.hpp>
#include <ccpp/ccpp_scheme_base.hpp>
#include <ccpp/ccpp_scheme_context.hpp>
#include <ccpp/ccpp_suite.hpp>

namespace {

/// A scheme that declares a constituent during its "register" phase (via
/// phase_register) and reads its bound view during phase_run, recording
/// the value it observes so the test can assert the full round trip.
class ConstituentAwareScheme : public ccpp::SchemeBase {
public:
    explicit ConstituentAwareScheme(std::string standard_name)
        : standard_name_(std::move(standard_name)) {}

    const std::string& name() const override {
        static const std::string n = "constituent_scheme";
        return n;
    }

    ccpp::SchemeError phase_register(ccpp::ConstituentRegistry& reg) override {
        ccpp::ConstituentMetadata meta;
        meta.name = "qv";
        meta.standard_name = standard_name_;
        meta.units = "kg kg-1";
        meta.dimensions = {"horizontal_dimension", "vertical_layer_dimension"};
        meta.advected = true;
        reg.declare(meta);
        registered_ = true;
        return {};
    }

    ccpp::SchemeError phase_run(ccpp::SchemeContext& ctx) override {
        if (ctx.constituents != nullptr) {
            auto view = ctx.constituents->get_view(standard_name_);
            if (view.extent(0) > 0 && view.extent(1) > 0 && view.extent(2) > 0) {
                observed_value_ = view(0, 0, 0);
                observed_ = true;
            }
        }
        return {};
    }

    bool registered() const { return registered_; }
    bool observed() const { return observed_; }
    double observed_value() const { return observed_value_; }

private:
    std::string standard_name_;
    bool registered_ = false;
    bool observed_ = false;
    double observed_value_ = 0.0;
};

} // anonymous namespace

/// Full lifecycle: a scheme registers a constituent, the host binds storage
/// to the shared registry, and the scheme's run phase can see the bound
/// values through the SchemeContext it's given during dispatch.
TEST(ConstituentLifecycleIntegration, RegisterBindAccessRoundTrip) {
    const std::string standard_name = "water_vapor_specific_humidity";

    auto scheme = std::make_unique<ConstituentAwareScheme>(standard_name);
    ConstituentAwareScheme* scheme_ptr = scheme.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme_ptr));

    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("physics", std::move(elements)));

    auto suite = std::make_unique<ccpp::Suite>("constituent_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("constituent_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // --- Step 1: Registration phase ---
    // Manually invoke phase_register on the scheme against the
    // orchestrator's shared registry (mirrors what a generated suite
    // factory's registration pass would do).
    ccpp::SchemeError reg_err =
        scheme_ptr->phase_register(orchestrator.constituents("constituent_suite"));
    ASSERT_FALSE(reg_err);
    EXPECT_TRUE(scheme_ptr->registered());
    EXPECT_EQ(orchestrator.constituents("constituent_suite").count(), 1u);

    // --- Step 2: Host binds storage ---
    const int ncol = 4;
    const int nlev = 3;
    std::vector<double> storage(static_cast<size_t>(ncol) * nlev, 0.0);
    storage[0] = 42.0;  // element (0,0) in column-major layout
    std::array<int, 3> dims{ncol, nlev, 1};
    orchestrator.constituents("constituent_suite")
        .bind_storage(standard_name, storage.data(), dims);

    // --- Step 3: Access during run ---
    int rc = orchestrator.run("constituent_suite", "physics", "run", 1, ncol, 1);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(scheme_ptr->observed());
    EXPECT_DOUBLE_EQ(scheme_ptr->observed_value(), 42.0);

    // --- Step 4: Finalize tears down the registry ---
    ASSERT_EQ(orchestrator.finalize(), 0);
    EXPECT_EQ(orchestrator.constituents("constituent_suite").count(), 0u)
        << "finalize() must clear the constituent registry";
}

/// Multiple timesteps: the bound storage values change between calls and
/// each phase_run observes the latest value, proving the view is a live
/// reference into host memory rather than a stale copy.
TEST(ConstituentLifecycleIntegration, ViewReflectsLiveHostMemoryAcrossTimesteps) {
    const std::string standard_name = "cloud_liquid_water_mixing_ratio";

    auto scheme = std::make_unique<ConstituentAwareScheme>(standard_name);
    ConstituentAwareScheme* scheme_ptr = scheme.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme_ptr));
    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("physics", std::move(elements)));
    auto suite = std::make_unique<ccpp::Suite>("multi_ts_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orchestrator;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("multi_ts_suite");
    ASSERT_EQ(orchestrator.initialize(host_desc), 0);
    orchestrator.register_suite(std::move(suite));

    // Register constituents
    ASSERT_FALSE(
        scheme_ptr->phase_register(orchestrator.constituents("multi_ts_suite")));

    std::vector<double> storage(4 * 3, 0.0);
    std::array<int, 3> dims{4, 3, 1};
    orchestrator.constituents("multi_ts_suite")
        .bind_storage(standard_name, storage.data(), dims);

    storage[0] = 1.0;
    ASSERT_EQ(orchestrator.run("multi_ts_suite", "physics", "run", 1, 4, 1), 0);
    EXPECT_DOUBLE_EQ(scheme_ptr->observed_value(), 1.0);

    storage[0] = 2.0;
    ASSERT_EQ(orchestrator.run("multi_ts_suite", "physics", "run", 1, 4, 1), 0);
    EXPECT_DOUBLE_EQ(scheme_ptr->observed_value(), 2.0);

    ASSERT_EQ(orchestrator.finalize(), 0);
}
