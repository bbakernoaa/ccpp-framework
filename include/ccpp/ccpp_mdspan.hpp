// ccpp_mdspan.hpp — Vendored mdspan aliases with Fortran layout_left semantics
// Part of the CCPP Framework C++ Modernization (Phase 1)
#pragma once

#include "mdspan.hpp"  // Kokkos reference implementation (C++17 compatible)

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace ccpp {

// ---------------------------------------------------------------------------
// Column-major (Fortran layout) mdspan alias
// ---------------------------------------------------------------------------

/// Generic mdspan with layout_left (column-major / Fortran ordering).
template<typename T, typename Extents>
using mdspan_left = Kokkos::mdspan<T, Extents, Kokkos::layout_left>;

// ---------------------------------------------------------------------------
// Convenience aliases for common physics array types
// ---------------------------------------------------------------------------

/// 2D column-major array of double (e.g., ncol x nlev)
using real64_2d = mdspan_left<double, Kokkos::dextents<int, 2>>;

/// 3D column-major array of double (e.g., ncol x nlev x nblk)
using real64_3d = mdspan_left<double, Kokkos::dextents<int, 3>>;

/// 4D column-major array of double (e.g., ncol x nlev x nblk x ntracers)
using real64_4d = mdspan_left<double, Kokkos::dextents<int, 4>>;

/// 1D array of 32-bit integers
using int32_1d = mdspan_left<int, Kokkos::dextents<int, 1>>;

// ---------------------------------------------------------------------------
// Factory helper: construct an mdspan from a raw pointer and dimension array
// ---------------------------------------------------------------------------

namespace detail {

template<typename T, std::size_t Rank, std::size_t... Is>
auto make_mdspan_left_impl(T* data,
                           const std::array<int, Rank>& dims,
                           std::index_sequence<Is...>) {
    using extents_type = Kokkos::dextents<int, Rank>;
    using mapping_type = typename Kokkos::layout_left::template mapping<extents_type>;
    return Kokkos::mdspan<T, extents_type, Kokkos::layout_left>(
        data, mapping_type(extents_type(dims[Is]...)));
}

} // namespace detail

/// Construct a layout_left mdspan from a raw Fortran pointer and dimensions.
/// No data is copied — the mdspan is a non-owning view over the provided memory.
///
/// @tparam T     Element type (e.g., double, int)
/// @tparam Rank  Number of dimensions (deduced from dims array size)
/// @param data   Pointer to the first element of the Fortran array
/// @param dims   Array extents in column-major order (fastest-varying first)
/// @return       An mdspan<T, dextents<int, Rank>, layout_left> viewing the data
template<typename T, std::size_t Rank>
auto make_mdspan_left(T* data, const std::array<int, Rank>& dims) {
    return detail::make_mdspan_left_impl(data, dims, std::make_index_sequence<Rank>{});
}

} // namespace ccpp
