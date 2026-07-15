/// @file test_pipeline_property.cpp
/// @brief Property-based test for pipeline execution ordering (Property 4).
///
/// Feature: cpp-bridge-framework, Property 4: Pipeline Execution Ordering
/// **Validates: Requirements 6.1, 6.2, 6.3, 6.5**
///
/// Verifies that:
/// - Schemes within each group execute in exact sequential order
/// - Groups within a suite execute in exact sequential order
/// - Subcycled schemes execute exactly L times before proceeding (run phase only)
/// - No scheme execution overlaps with another in time

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <ccpp/ccpp_scheme_base.hpp>
#include <ccpp/ccpp_scheme_context.hpp>
#include <ccpp/ccpp_pipeline_element.hpp>
#include <ccpp/ccpp_group.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

/// A mock scheme that records its invocation in a shared execution trace.
/// Each call to phase_run appends its id to the trace vector.
class TracingScheme : public ccpp::SchemeBase {
public:
    /// Construct a TracingScheme with a given id and shared trace vector.
    /// @param id    Unique identifier for this scheme instance.
    /// @param trace Shared vector to which invocations are appended.
    TracingScheme(const std::string& id, std::vector<std::string>& trace)
        : id_(id), trace_(trace) {}

    const std::string& name() const override { return id_; }

    ccpp::SchemeError phase_run(ccpp::SchemeContext& ctx) override {
        trace_.push_back(id_);
        return {};
    }

private:
    std::string id_;
    std::vector<std::string>& trace_;
};

/// Generator for the number of schemes per group [1, 8].
rc::Gen<int> genNumSchemes() {
    return rc::gen::inRange(1, 9);
}

/// Generator for the number of groups [1, 5].
rc::Gen<int> genNumGroups() {
    return rc::gen::inRange(1, 6);
}

/// Generator for subcycle loop count [1, 5].
rc::Gen<int> genLoopCount() {
    return rc::gen::inRange(1, 6);
}

/// Generator for subcycle depth probability (whether a position becomes a subcycle).
rc::Gen<bool> genIsSubcycle() {
    // ~30% chance of being a subcycle
    return rc::gen::map(rc::gen::inRange(0, 10), [](int v) { return v < 3; });
}

} // anonymous namespace

/// Property 4: Pipeline Execution Ordering - Flat group with N schemes.
///
/// For any group with N schemes executed in the "run" phase,
/// the recorded trace must match the sequential order of scheme ids.
RC_GTEST_PROP(PipelineOrdering, flat_group_executes_in_order, ()) {
    const auto num_schemes = *genNumSchemes();

    // Create shared trace vector
    std::vector<std::string> trace;

    // Create schemes and pipeline elements
    std::vector<std::unique_ptr<TracingScheme>> schemes;
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;

    for (int i = 0; i < num_schemes; ++i) {
        std::string id = "scheme_" + std::to_string(i);
        schemes.push_back(std::make_unique<TracingScheme>(id, trace));
        elements.push_back(
            std::make_unique<ccpp::SchemeElement>(schemes.back().get()));
    }

    // Create a group and execute
    ccpp::Group group("test_group", std::move(elements));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};
    auto err = group.execute("run", ctx);

    RC_ASSERT(!err);
    RC_ASSERT(trace.size() == static_cast<std::size_t>(num_schemes));

    // Verify sequential order
    for (int i = 0; i < num_schemes; ++i) {
        RC_ASSERT(trace[i] == "scheme_" + std::to_string(i));
    }
}

/// Property 4: Pipeline Execution Ordering - Subcycle iterates correctly.
///
/// For any subcycle with loop_count L containing M schemes,
/// the trace must contain L*M entries with correct repetition pattern.
RC_GTEST_PROP(PipelineOrdering, subcycle_iterates_correct_times, ()) {
    const auto loop_count = *genLoopCount();
    const auto num_schemes = *genNumSchemes();

    std::vector<std::string> trace;

    // Create schemes and subcycle children
    std::vector<std::unique_ptr<TracingScheme>> schemes;
    std::vector<std::unique_ptr<ccpp::PipelineElement>> children;

    for (int i = 0; i < num_schemes; ++i) {
        std::string id = "sub_scheme_" + std::to_string(i);
        schemes.push_back(std::make_unique<TracingScheme>(id, trace));
        children.push_back(
            std::make_unique<ccpp::SchemeElement>(schemes.back().get()));
    }

    // Create subcycle element and wrap in a group
    std::vector<std::unique_ptr<ccpp::PipelineElement>> group_elements;
    group_elements.push_back(
        std::make_unique<ccpp::SubcycleElement>(loop_count, std::move(children)));

    ccpp::Group group("subcycle_group", std::move(group_elements));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};
    auto err = group.execute("run", ctx);

    RC_ASSERT(!err);

    // Total invocations should be loop_count * num_schemes
    const auto expected_total =
        static_cast<std::size_t>(loop_count) * num_schemes;
    RC_ASSERT(trace.size() == expected_total);

    // Verify the pattern: schemes repeat in order for each iteration
    for (int iter = 0; iter < loop_count; ++iter) {
        for (int s = 0; s < num_schemes; ++s) {
            std::size_t idx = static_cast<std::size_t>(iter) * num_schemes + s;
            RC_ASSERT(trace[idx] == "sub_scheme_" + std::to_string(s));
        }
    }
}

/// Property 4: Pipeline Execution Ordering - Mixed pipeline (schemes + subcycles).
///
/// For any pipeline with a mix of flat schemes and subcycle elements,
/// the execution trace matches the expected sequential expansion.
RC_GTEST_PROP(PipelineOrdering, mixed_pipeline_ordering, ()) {
    // Generate a random pipeline structure
    const auto num_elements = *rc::gen::inRange(2, 8); // 2-7 elements

    std::vector<std::string> trace;
    std::vector<std::string> expected;

    std::vector<std::unique_ptr<TracingScheme>> all_schemes;
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;

    int scheme_counter = 0;

    for (int e = 0; e < num_elements; ++e) {
        bool make_subcycle = *genIsSubcycle();

        if (make_subcycle) {
            // Create a subcycle with random loop count and children
            int loop = *genLoopCount();
            int num_children = *rc::gen::inRange(1, 5);

            std::vector<std::unique_ptr<ccpp::PipelineElement>> children;

            // Build expected trace for subcycle
            std::vector<std::string> child_ids;
            for (int c = 0; c < num_children; ++c) {
                std::string id = "s" + std::to_string(scheme_counter++);
                child_ids.push_back(id);
                all_schemes.push_back(std::make_unique<TracingScheme>(id, trace));
                children.push_back(
                    std::make_unique<ccpp::SchemeElement>(all_schemes.back().get()));
            }

            // Expected: children repeated loop times
            for (int iter = 0; iter < loop; ++iter) {
                for (const auto& cid : child_ids) {
                    expected.push_back(cid);
                }
            }

            elements.push_back(
                std::make_unique<ccpp::SubcycleElement>(loop, std::move(children)));
        } else {
            // Create a flat scheme element
            std::string id = "s" + std::to_string(scheme_counter++);
            expected.push_back(id);
            all_schemes.push_back(std::make_unique<TracingScheme>(id, trace));
            elements.push_back(
                std::make_unique<ccpp::SchemeElement>(all_schemes.back().get()));
        }
    }

    // Execute the group
    ccpp::Group group("mixed_group", std::move(elements));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};
    auto err = group.execute("run", ctx);

    RC_ASSERT(!err);
    RC_ASSERT(trace.size() == expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        RC_ASSERT(trace[i] == expected[i]);
    }
}

/// Property 4: Pipeline Execution Ordering - Multiple groups in sequence.
///
/// For any N groups each with M schemes, executing them sequentially
/// produces a trace that matches group1_schemes + group2_schemes + ...
RC_GTEST_PROP(PipelineOrdering, multiple_groups_sequential_order, ()) {
    const auto num_groups = *genNumGroups();

    std::vector<std::string> trace;
    std::vector<std::string> expected;

    std::vector<std::unique_ptr<TracingScheme>> all_schemes;
    std::vector<std::unique_ptr<ccpp::Group>> groups;

    for (int g = 0; g < num_groups; ++g) {
        int num_schemes = *genNumSchemes();
        std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;

        for (int s = 0; s < num_schemes; ++s) {
            std::string id = "g" + std::to_string(g) + "_s" + std::to_string(s);
            expected.push_back(id);
            all_schemes.push_back(std::make_unique<TracingScheme>(id, trace));
            elements.push_back(
                std::make_unique<ccpp::SchemeElement>(all_schemes.back().get()));
        }

        groups.push_back(std::make_unique<ccpp::Group>(
            "group_" + std::to_string(g), std::move(elements)));
    }

    // Execute all groups in sequence (simulating suite behavior)
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};
    for (auto& grp : groups) {
        auto err = grp->execute("run", ctx);
        RC_ASSERT(!err);
    }

    RC_ASSERT(trace.size() == expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        RC_ASSERT(trace[i] == expected[i]);
    }
}

/// Property 4: Pipeline Execution Ordering - Nested subcycles.
///
/// Verifies that a subcycle containing another subcycle iterates correctly:
/// outer_loop * (inner_loop * inner_schemes) executions.
RC_GTEST_PROP(PipelineOrdering, nested_subcycles_iterate_correctly, ()) {
    const auto outer_loop = *rc::gen::inRange(1, 4);
    const auto inner_loop = *rc::gen::inRange(1, 4);
    const auto num_schemes = *rc::gen::inRange(1, 4);

    std::vector<std::string> trace;
    std::vector<std::unique_ptr<TracingScheme>> all_schemes;

    // Build inner subcycle children
    std::vector<std::unique_ptr<ccpp::PipelineElement>> inner_children;
    std::vector<std::string> inner_ids;
    for (int s = 0; s < num_schemes; ++s) {
        std::string id = "inner_" + std::to_string(s);
        inner_ids.push_back(id);
        all_schemes.push_back(std::make_unique<TracingScheme>(id, trace));
        inner_children.push_back(
            std::make_unique<ccpp::SchemeElement>(all_schemes.back().get()));
    }

    // Build outer subcycle with the inner subcycle as its child
    std::vector<std::unique_ptr<ccpp::PipelineElement>> outer_children;
    outer_children.push_back(
        std::make_unique<ccpp::SubcycleElement>(inner_loop, std::move(inner_children)));

    // Wrap in a group
    std::vector<std::unique_ptr<ccpp::PipelineElement>> group_elements;
    group_elements.push_back(
        std::make_unique<ccpp::SubcycleElement>(outer_loop, std::move(outer_children)));

    ccpp::Group group("nested_group", std::move(group_elements));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};
    auto err = group.execute("run", ctx);

    RC_ASSERT(!err);

    // Total: outer_loop * inner_loop * num_schemes
    const auto expected_total =
        static_cast<std::size_t>(outer_loop) * inner_loop * num_schemes;
    RC_ASSERT(trace.size() == expected_total);

    // Verify the pattern: inner schemes repeat in order
    std::size_t idx = 0;
    for (int o = 0; o < outer_loop; ++o) {
        for (int i = 0; i < inner_loop; ++i) {
            for (int s = 0; s < num_schemes; ++s) {
                RC_ASSERT(trace[idx] == inner_ids[s]);
                ++idx;
            }
        }
    }
}

/// Regression test for the subcycle phase-gating fix: a scheme nested
/// inside <subcycle loop="N"> must have its non-run phase methods
/// (init/timestep_init/timestep_final/final) invoked exactly once per
/// group execution, never N times.
TEST(PipelineOrdering, subcycle_non_run_phases_execute_once) {
    class CountingScheme : public ccpp::SchemeBase {
    public:
        const std::string& name() const override {
            static const std::string n = "counting";
            return n;
        }
        ccpp::SchemeError phase_init(ccpp::SchemeContext&) override {
            ++init_count;
            return {};
        }
        ccpp::SchemeError phase_run(ccpp::SchemeContext&) override {
            ++run_count;
            return {};
        }
        int init_count = 0;
        int run_count = 0;
    };

    auto scheme = std::make_unique<CountingScheme>();
    CountingScheme* s = scheme.get();

    std::vector<std::unique_ptr<ccpp::PipelineElement>> children;
    children.push_back(std::make_unique<ccpp::SchemeElement>(s));

    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(
        std::make_unique<ccpp::SubcycleElement>(5, std::move(children)));

    ccpp::Group group("phys_ps", std::move(elements));
    ccpp::SchemeContext ctx{1, 10, 1, 1, nullptr, nullptr, nullptr};

    ASSERT_FALSE(group.execute("init", ctx));
    EXPECT_EQ(s->init_count, 1);

    ASSERT_FALSE(group.execute("run", ctx));
    EXPECT_EQ(s->run_count, 5);
}
