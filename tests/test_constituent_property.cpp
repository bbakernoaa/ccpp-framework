/// @file test_constituent_property.cpp
/// @brief Property-based test for constituent registry round-trip (Property 8).
///
/// Feature: cpp-bridge-framework, Property 8: Constituent Registry Round-Trip
/// **Validates: Requirements 11.2, 11.3, 11.4**
///
/// For any set of randomly generated constituent metadata, verify that:
///   1. Metadata survives a declare() -> query cycle with all fields intact.
///   2. bind_storage() followed by get_view() produces an mdspan view whose
///      data_handle() equals the bound pointer and whose extents match dims.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <ccpp/ccpp_constituent_registry.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

rc::Gen<std::string> genSimpleName() {
    return rc::gen::map(
        rc::gen::inRange(1, 20),
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

/// Property 8: declared metadata survives a query round-trip unchanged.
RC_GTEST_PROP(ConstituentRegistryRoundTrip, DeclareThenQueryPreservesMetadata, ()) {
    const auto name = *genSimpleName();
    const auto standard_name = *genSimpleName();
    const auto units = *genSimpleName();
    const auto advected = *rc::gen::arbitrary<bool>();
    const auto molar_mass = *rc::gen::map(rc::gen::inRange(0, 100000),
                                          [](int v) { return v / 1000.0; });
    const auto default_value = *rc::gen::map(rc::gen::inRange(-100000, 100000),
                                             [](int v) { return v / 1000.0; });

    ccpp::ConstituentMetadata meta;
    meta.name = name;
    meta.standard_name = standard_name;
    meta.units = units;
    meta.dimensions = {"horizontal_dimension", "vertical_layer_dimension"};
    meta.advected = advected;
    meta.molar_mass = molar_mass;
    meta.default_value = default_value;

    ccpp::ConstituentRegistry registry;
    registry.declare(meta);

    RC_ASSERT(registry.count() == 1u);
    const auto& all = registry.all();
    RC_ASSERT(all.size() == 1u);
    RC_ASSERT(all[0].name == name);
    RC_ASSERT(all[0].standard_name == standard_name);
    RC_ASSERT(all[0].units == units);
    RC_ASSERT(all[0].dimensions.size() == 2u);
    RC_ASSERT(all[0].advected == advected);
    RC_ASSERT(all[0].molar_mass == molar_mass);
    RC_ASSERT(all[0].default_value == default_value);
}

/// Property 8: bound storage produces an mdspan view with matching pointer
/// and extents.
RC_GTEST_PROP(ConstituentRegistryRoundTrip, BindStorageProducesCorrectView, ()) {
    const auto standard_name = *genSimpleName();
    const auto d0 = *rc::gen::inRange(1, 17);
    const auto d1 = *rc::gen::inRange(1, 17);
    const auto d2 = *rc::gen::inRange(1, 17);

    ccpp::ConstituentMetadata meta;
    meta.name = "tracer";
    meta.standard_name = standard_name;
    meta.units = "kg kg-1";

    ccpp::ConstituentRegistry registry;
    registry.declare(meta);

    std::vector<double> storage(static_cast<std::size_t>(d0) * d1 * d2, 0.0);
    std::array<int, 3> dims{d0, d1, d2};
    registry.bind_storage(standard_name, storage.data(), dims);

    auto view = registry.get_view(standard_name);
    RC_ASSERT(view.data_handle() == storage.data());
    RC_ASSERT(static_cast<int>(view.extent(0)) == d0);
    RC_ASSERT(static_cast<int>(view.extent(1)) == d1);
    RC_ASSERT(static_cast<int>(view.extent(2)) == d2);
}

/// Declaring a duplicate standard_name must throw.
TEST(ConstituentRegistry, DuplicateDeclareThrows) {
    ccpp::ConstituentRegistry registry;
    ccpp::ConstituentMetadata meta;
    meta.name = "qv";
    meta.standard_name = "water_vapor_specific_humidity";
    registry.declare(meta);
    EXPECT_THROW(registry.declare(meta), std::runtime_error);
}

/// Binding storage to an undeclared constituent must throw.
TEST(ConstituentRegistry, BindUnknownConstituentThrows) {
    ccpp::ConstituentRegistry registry;
    std::vector<double> storage(10, 0.0);
    std::array<int, 3> dims{1, 1, 1};
    EXPECT_THROW(registry.bind_storage("unknown", storage.data(), dims),
                 std::runtime_error);
}

/// Querying a view for a declared-but-unbound constituent must throw.
TEST(ConstituentRegistry, GetViewWithoutBindingThrows) {
    ccpp::ConstituentRegistry registry;
    ccpp::ConstituentMetadata meta;
    meta.name = "qv";
    meta.standard_name = "water_vapor_specific_humidity";
    registry.declare(meta);
    EXPECT_THROW(registry.get_view("water_vapor_specific_humidity"),
                 std::runtime_error);
}
