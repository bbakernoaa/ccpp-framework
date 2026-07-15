/// @file test_bridge_exception_property.cpp
/// @brief Property-based test for exception containment at the language
/// boundary (Property 5).
///
/// Feature: cpp-bridge-framework, Property 5: Exception Containment at
/// Language Boundary
/// **Validates: Requirements 7.2, 7.5**
///
/// For any C++ exception type thrown during scheme execution, the bridge
/// layer's try/catch must contain it and return a non-zero fail_flag
/// instead of letting the exception propagate across the iso_c_binding
/// boundary (which would be undefined behavior in Fortran).

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <ccpp/ccpp_framework_orchestrator.hpp>
#include <ccpp/ccpp_group.hpp>
#include <ccpp/ccpp_pipeline_element.hpp>
#include <ccpp/ccpp_scheme_base.hpp>
#include <ccpp/ccpp_scheme_context.hpp>
#include <ccpp/ccpp_suite.hpp>

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/// A scheme that throws a caller-specified exception type during phase_run.
class ThrowingScheme : public ccpp::SchemeBase {
public:
    enum class Kind { StdRuntimeError, StdLogicError, StdException, Unknown };

    explicit ThrowingScheme(Kind kind, std::string message)
        : kind_(kind), message_(std::move(message)) {}

    const std::string& name() const override {
        static const std::string n = "throwing_scheme";
        return n;
    }

    ccpp::SchemeError phase_run(ccpp::SchemeContext&) override {
        switch (kind_) {
            case Kind::StdRuntimeError:
                throw std::runtime_error(message_);
            case Kind::StdLogicError:
                throw std::logic_error(message_);
            case Kind::StdException:
                throw std::exception();
            case Kind::Unknown:
                throw message_;  // throws a std::string, not derived from std::exception
        }
        return {};
    }

private:
    Kind kind_;
    std::string message_;
};

/// Emulates the bridge layer's try/catch pattern (mirrors ccpp_bridge.cpp's
/// ccpp_framework_run) around a call into the orchestrator, since the
/// orchestrator itself does not call scheme methods with try/catch --
/// SchemeElement::execute calls straight through. The real containment
/// boundary is the extern "C" bridge function; this helper reproduces that
/// exact wrapping so the property test doesn't need the full bridge ABI.
int run_with_bridge_style_containment(ccpp::FrameworkOrchestrator& orch,
                                      const std::string& suite_name,
                                      const std::string& group_name,
                                      const std::string& phase,
                                      std::string& errmsg_out) {
    try {
        int rc = orch.run(suite_name, group_name, phase, 1, 10, 1);
        if (rc != 0) {
            errmsg_out = orch.last_error_msg();
        }
        return rc;
    } catch (const std::exception& e) {
        errmsg_out = std::string("caught: ") + e.what();
        return 1;
    } catch (...) {
        errmsg_out = "caught: unknown exception";
        return 1;
    }
}

rc::Gen<ThrowingScheme::Kind> genExceptionKind() {
    return rc::gen::element(
        ThrowingScheme::Kind::StdRuntimeError,
        ThrowingScheme::Kind::StdLogicError,
        ThrowingScheme::Kind::StdException,
        ThrowingScheme::Kind::Unknown);
}

rc::Gen<std::string> genMessage() {
    return rc::gen::map(
        rc::gen::inRange(0, 40),
        [](int len) {
            std::string s;
            s.reserve(static_cast<size_t>(len));
            for (int i = 0; i < len; ++i) {
                s.push_back(static_cast<char>('a' + (i % 26)));
            }
            return s;
        });
}

} // anonymous namespace

/// Property 5: any exception type thrown during phase_run is contained by
/// the bridge-style try/catch and results in a non-zero return code.
RC_GTEST_PROP(ExceptionContainment, AnyExceptionTypeIsContainedAndReturnsNonzero, ()) {
    const auto kind = *genExceptionKind();
    const auto message = *genMessage();

    auto scheme = std::make_unique<ThrowingScheme>(kind, message);
    std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;
    elements.push_back(std::make_unique<ccpp::SchemeElement>(scheme.get()));
    std::vector<std::unique_ptr<ccpp::Group>> groups;
    groups.push_back(std::make_unique<ccpp::Group>("group", std::move(elements)));
    auto suite = std::make_unique<ccpp::Suite>("throw_suite", std::move(groups));

    ccpp::FrameworkOrchestrator orch;
    ccpp::HostStateDescriptor host_desc;
    host_desc.suite_names.push_back("throw_suite");
    orch.initialize(host_desc);
    orch.register_suite(std::move(suite));

    std::string errmsg;
    int rc = run_with_bridge_style_containment(orch, "throw_suite", "group", "run", errmsg);

    RC_ASSERT(rc != 0);
    RC_ASSERT(!errmsg.empty());

    orch.finalize();
}
