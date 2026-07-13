// Property 9: Submdspan Slicing Produces Correct 3D View
// Feature: cpp-bridge-framework, Property 9: Submdspan Slicing Produces Correct 3D View
//
// Validates: Requirements 5.1
//
// For any 4D host array with extents (ncol, nlay, nblk, ntracers) where all
// extents >= 1, and for any valid tracer index k in [0, ntracers), applying
// submdspan to fix the 4th dimension at k SHALL produce a 3D view with extents
// (ncol, nlay, nblk) where accessing element (i, j, m) in the slice returns the
// same value as accessing element (i, j, m, k) in the original 4D array.

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "ccpp/ccpp_mdspan.hpp"

#include <array>
#include <numeric>
#include <vector>

namespace {

// **Validates: Requirements 5.1**
RC_GTEST_PROP(SubmdspanSlicing,
              FixingFourthDimProducesCorrect3DView,
              ()) {
    // Generate random extents in [1, 16] for each dimension
    const auto ncol     = *rc::gen::inRange(1, 17);
    const auto nlay     = *rc::gen::inRange(1, 17);
    const auto nblk     = *rc::gen::inRange(1, 17);
    const auto ntracers = *rc::gen::inRange(1, 17);

    // Generate a random tracer index k in [0, ntracers)
    const auto k = *rc::gen::inRange(0, ntracers);

    // Allocate a backing buffer and fill with unique values
    const std::size_t total_size =
        static_cast<std::size_t>(ncol) * nlay * nblk * ntracers;
    std::vector<double> data(total_size);
    std::iota(data.begin(), data.end(), 0.0);

    // Construct a 4D layout_left mdspan
    std::array<int, 4> dims = {ncol, nlay, nblk, ntracers};
    auto view4d = ccpp::make_mdspan_left<double, 4>(data.data(), dims);

    // Apply submdspan: fix the 4th dimension at index k to get a 3D slice
    auto slice3d = Kokkos::submdspan(view4d,
                                     Kokkos::full_extent,
                                     Kokkos::full_extent,
                                     Kokkos::full_extent,
                                     k);

    // Verify the 3D slice has correct extents
    RC_ASSERT(slice3d.extent(0) == ncol);
    RC_ASSERT(slice3d.extent(1) == nlay);
    RC_ASSERT(slice3d.extent(2) == nblk);

    // Verify element access: slice3d(i, j, m) == view4d(i, j, m, k)
    for (int i = 0; i < ncol; ++i) {
        for (int j = 0; j < nlay; ++j) {
            for (int m = 0; m < nblk; ++m) {
                RC_ASSERT(slice3d(i, j, m) == view4d(i, j, m, k));
            }
        }
    }
}

} // namespace
