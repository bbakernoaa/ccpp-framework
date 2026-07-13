// Property 2: Mdspan Construction Preserves Fortran Memory Layout
// Feature: cpp-bridge-framework, Property 2: Mdspan Construction Preserves Fortran Memory Layout
//
// **Validates: Requirements 4.3, 4.4, 4.5**
//
// For any raw pointer to a multi-dimensional array with column-major (Fortran)
// ordering and for any valid dimension set (rank 1 through 4, extents >= 1),
// constructing an mdspan<T, dextents<int, Rank>, layout_left> SHALL:
//   1. Set data_handle() identical to the input pointer (zero copy)
//   2. Set extent(i) matching the input dimension at position i
//   3. Produce element access mdspan(i0, i1, ..., iN) that returns the same
//      value as offset-based access using column-major indexing:
//      ptr[i0 + i1*d0 + i2*d0*d1 + ...]

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "ccpp/ccpp_mdspan.hpp"

#include <array>
#include <numeric>
#include <vector>

namespace {

// Generator for extents in range [1, 64]
rc::Gen<int> genExtent() {
    return rc::gen::inRange(1, 65);  // [1, 64]
}

// Compute column-major linear offset for given indices and dimensions
template<std::size_t Rank>
std::size_t columnMajorOffset(const std::array<int, Rank>& indices,
                              const std::array<int, Rank>& dims) {
    std::size_t offset = 0;
    std::size_t stride = 1;
    for (std::size_t d = 0; d < Rank; ++d) {
        offset += static_cast<std::size_t>(indices[d]) * stride;
        stride *= static_cast<std::size_t>(dims[d]);
    }
    return offset;
}

// Compute total number of elements from dimensions
template<std::size_t Rank>
std::size_t totalElements(const std::array<int, Rank>& dims) {
    std::size_t total = 1;
    for (std::size_t d = 0; d < Rank; ++d) {
        total *= static_cast<std::size_t>(dims[d]);
    }
    return total;
}

} // namespace

// ---------------------------------------------------------------------------
// Property 2, Rank 1: mdspan construction preserves layout for 1D arrays
// ---------------------------------------------------------------------------
RC_GTEST_PROP(MdspanLayoutPreservation, Rank1_DataHandleAndExtentsAndAccess, ()) {
    const auto d0 = *genExtent();
    std::array<int, 1> dims = {d0};

    // Allocate buffer and fill with index-based values
    const std::size_t n = totalElements(dims);
    std::vector<double> buffer(n);
    std::iota(buffer.begin(), buffer.end(), 0.0);

    double* ptr = buffer.data();
    auto span = ccpp::make_mdspan_left<double, 1>(ptr, dims);

    // Property 1: data_handle() == input pointer (zero copy)
    RC_ASSERT(span.data_handle() == ptr);

    // Property 2: extent(i) matches input dimension
    RC_ASSERT(span.extent(0) == d0);

    // Property 3: Element access matches column-major offset
    // Test a random subset of indices to keep runtime bounded
    const int numSamples = std::min(d0, 10);
    for (int sample = 0; sample < numSamples; ++sample) {
        const int i0 = *rc::gen::inRange(0, d0);
        std::array<int, 1> indices = {i0};
        const std::size_t offset = columnMajorOffset(indices, dims);
        RC_ASSERT(span(i0) == ptr[offset]);
    }
}

// ---------------------------------------------------------------------------
// Property 2, Rank 2: mdspan construction preserves layout for 2D arrays
// ---------------------------------------------------------------------------
RC_GTEST_PROP(MdspanLayoutPreservation, Rank2_DataHandleAndExtentsAndAccess, ()) {
    const auto d0 = *genExtent();
    const auto d1 = *genExtent();
    std::array<int, 2> dims = {d0, d1};

    const std::size_t n = totalElements(dims);
    std::vector<double> buffer(n);
    std::iota(buffer.begin(), buffer.end(), 0.0);

    double* ptr = buffer.data();
    auto span = ccpp::make_mdspan_left<double, 2>(ptr, dims);

    // Property 1: data_handle() == input pointer (zero copy)
    RC_ASSERT(span.data_handle() == ptr);

    // Property 2: extent(i) matches input dimension
    RC_ASSERT(span.extent(0) == d0);
    RC_ASSERT(span.extent(1) == d1);

    // Property 3: Element access matches column-major offset
    const int numSamples = std::min(d0 * d1, 20);
    for (int sample = 0; sample < numSamples; ++sample) {
        const int i0 = *rc::gen::inRange(0, d0);
        const int i1 = *rc::gen::inRange(0, d1);
        std::array<int, 2> indices = {i0, i1};
        const std::size_t offset = columnMajorOffset(indices, dims);
        RC_ASSERT(span(i0, i1) == ptr[offset]);
    }
}

// ---------------------------------------------------------------------------
// Property 2, Rank 3: mdspan construction preserves layout for 3D arrays
// ---------------------------------------------------------------------------
RC_GTEST_PROP(MdspanLayoutPreservation, Rank3_DataHandleAndExtentsAndAccess, ()) {
    const auto d0 = *genExtent();
    const auto d1 = *genExtent();
    const auto d2 = *genExtent();
    std::array<int, 3> dims = {d0, d1, d2};

    // Limit total size to avoid excessive memory usage
    const std::size_t n = totalElements(dims);
    RC_PRE(n <= 65536);

    std::vector<double> buffer(n);
    std::iota(buffer.begin(), buffer.end(), 0.0);

    double* ptr = buffer.data();
    auto span = ccpp::make_mdspan_left<double, 3>(ptr, dims);

    // Property 1: data_handle() == input pointer (zero copy)
    RC_ASSERT(span.data_handle() == ptr);

    // Property 2: extent(i) matches input dimension
    RC_ASSERT(span.extent(0) == d0);
    RC_ASSERT(span.extent(1) == d1);
    RC_ASSERT(span.extent(2) == d2);

    // Property 3: Element access matches column-major offset
    const int numSamples = 20;
    for (int sample = 0; sample < numSamples; ++sample) {
        const int i0 = *rc::gen::inRange(0, d0);
        const int i1 = *rc::gen::inRange(0, d1);
        const int i2 = *rc::gen::inRange(0, d2);
        std::array<int, 3> indices = {i0, i1, i2};
        const std::size_t offset = columnMajorOffset(indices, dims);
        RC_ASSERT(span(i0, i1, i2) == ptr[offset]);
    }
}

// ---------------------------------------------------------------------------
// Property 2, Rank 4: mdspan construction preserves layout for 4D arrays
// ---------------------------------------------------------------------------
RC_GTEST_PROP(MdspanLayoutPreservation, Rank4_DataHandleAndExtentsAndAccess, ()) {
    const auto d0 = *genExtent();
    const auto d1 = *genExtent();
    const auto d2 = *genExtent();
    const auto d3 = *genExtent();
    std::array<int, 4> dims = {d0, d1, d2, d3};

    // Limit total size to avoid excessive memory usage
    const std::size_t n = totalElements(dims);
    RC_PRE(n <= 65536);

    std::vector<double> buffer(n);
    std::iota(buffer.begin(), buffer.end(), 0.0);

    double* ptr = buffer.data();
    auto span = ccpp::make_mdspan_left<double, 4>(ptr, dims);

    // Property 1: data_handle() == input pointer (zero copy)
    RC_ASSERT(span.data_handle() == ptr);

    // Property 2: extent(i) matches input dimension
    RC_ASSERT(span.extent(0) == d0);
    RC_ASSERT(span.extent(1) == d1);
    RC_ASSERT(span.extent(2) == d2);
    RC_ASSERT(span.extent(3) == d3);

    // Property 3: Element access matches column-major offset
    const int numSamples = 20;
    for (int sample = 0; sample < numSamples; ++sample) {
        const int i0 = *rc::gen::inRange(0, d0);
        const int i1 = *rc::gen::inRange(0, d1);
        const int i2 = *rc::gen::inRange(0, d2);
        const int i3 = *rc::gen::inRange(0, d3);
        std::array<int, 4> indices = {i0, i1, i2, i3};
        const std::size_t offset = columnMajorOffset(indices, dims);
        RC_ASSERT(span(i0, i1, i2, i3) == ptr[offset]);
    }
}

// ---------------------------------------------------------------------------
// Property 2, Integer type: verify layout preservation with int32_t (int32_1d)
// ---------------------------------------------------------------------------
RC_GTEST_PROP(MdspanLayoutPreservation, Int32_Rank1_DataHandleAndExtentsAndAccess, ()) {
    const auto d0 = *genExtent();
    std::array<int, 1> dims = {d0};

    const std::size_t n = totalElements(dims);
    std::vector<int> buffer(n);
    std::iota(buffer.begin(), buffer.end(), 0);

    int* ptr = buffer.data();
    auto span = ccpp::make_mdspan_left<int, 1>(ptr, dims);

    // Property 1: data_handle() == input pointer (zero copy)
    RC_ASSERT(span.data_handle() == ptr);

    // Property 2: extent(i) matches input dimension
    RC_ASSERT(span.extent(0) == d0);

    // Property 3: Element access matches column-major offset
    const int numSamples = std::min(d0, 10);
    for (int sample = 0; sample < numSamples; ++sample) {
        const int i0 = *rc::gen::inRange(0, d0);
        std::array<int, 1> indices = {i0};
        const std::size_t offset = columnMajorOffset(indices, dims);
        RC_ASSERT(span(i0) == ptr[offset]);
    }
}
