// ccpp_contiguous_buffer.hpp — Pack/unpack strided submdspan into contiguous buffer
// Part of the CCPP Framework C++ Modernization (Phase 1)
#pragma once

#include "ccpp_mdspan.hpp"

#include <cstddef>
#include <vector>

namespace ccpp {

namespace detail {

// ---------------------------------------------------------------------------
// Column-major iteration helpers
// These iterate over all elements of an mdspan in column-major order
// (leftmost index varies fastest), matching Fortran memory layout.
// ---------------------------------------------------------------------------

/// Pack a rank-2 mdspan into a contiguous buffer in column-major order.
template<typename T, typename MdspanType>
void pack_rank2(const MdspanType& src, std::vector<T>& buffer) {
    const auto n0 = src.extent(0);
    const auto n1 = src.extent(1);
    buffer.resize(static_cast<std::size_t>(n0) * static_cast<std::size_t>(n1));
    std::size_t idx = 0;
    for (decltype(n1) j = 0; j < n1; ++j) {
        for (decltype(n0) i = 0; i < n0; ++i) {
            buffer[idx++] = src(i, j);
        }
    }
}

/// Pack a rank-3 mdspan into a contiguous buffer in column-major order.
template<typename T, typename MdspanType>
void pack_rank3(const MdspanType& src, std::vector<T>& buffer) {
    const auto n0 = src.extent(0);
    const auto n1 = src.extent(1);
    const auto n2 = src.extent(2);
    buffer.resize(static_cast<std::size_t>(n0) *
                  static_cast<std::size_t>(n1) *
                  static_cast<std::size_t>(n2));
    std::size_t idx = 0;
    for (decltype(n2) k = 0; k < n2; ++k) {
        for (decltype(n1) j = 0; j < n1; ++j) {
            for (decltype(n0) i = 0; i < n0; ++i) {
                buffer[idx++] = src(i, j, k);
            }
        }
    }
}

/// Unpack a contiguous buffer into a rank-2 mdspan in column-major order.
template<typename T, typename MdspanType>
void unpack_rank2(const std::vector<T>& buffer, MdspanType& dst) {
    const auto n0 = dst.extent(0);
    const auto n1 = dst.extent(1);
    std::size_t idx = 0;
    for (decltype(n1) j = 0; j < n1; ++j) {
        for (decltype(n0) i = 0; i < n0; ++i) {
            dst(i, j) = buffer[idx++];
        }
    }
}

/// Unpack a contiguous buffer into a rank-3 mdspan in column-major order.
template<typename T, typename MdspanType>
void unpack_rank3(const std::vector<T>& buffer, MdspanType& dst) {
    const auto n0 = dst.extent(0);
    const auto n1 = dst.extent(1);
    const auto n2 = dst.extent(2);
    std::size_t idx = 0;
    for (decltype(n2) k = 0; k < n2; ++k) {
        for (decltype(n1) j = 0; j < n1; ++j) {
            for (decltype(n0) i = 0; i < n0; ++i) {
                dst(i, j, k) = buffer[idx++];
            }
        }
    }
}

} // namespace detail

// ---------------------------------------------------------------------------
// ContiguousBuffer: pack strided submdspan data into contiguous memory
// ---------------------------------------------------------------------------

/// Pack a strided submdspan into a contiguous buffer for Fortran interop.
///
/// This class is used by auto-generated proxy wrappers to ensure that legacy
/// Fortran physics schemes receive contiguous memory even when the source data
/// is a strided submdspan slice of a higher-dimensional host array.
///
/// @tparam T         Element type (e.g., double)
/// @tparam SrcMdspan The source mdspan type (typically a submdspan result)
template<typename T, typename SrcMdspan>
class ContiguousBuffer {
public:
    /// Pack source view into internal contiguous storage (column-major order).
    /// Leftmost index varies fastest, matching Fortran layout semantics.
    void pack(const SrcMdspan& src) {
        if constexpr (SrcMdspan::rank() == 2) {
            detail::pack_rank2<T>(src, buffer_);
        } else if constexpr (SrcMdspan::rank() == 3) {
            detail::pack_rank3<T>(src, buffer_);
        }
        // Static assert for unsupported ranks
        static_assert(SrcMdspan::rank() == 2 || SrcMdspan::rank() == 3,
                      "ContiguousBuffer only supports rank-2 and rank-3 mdspan types");
    }

    /// Unpack internal buffer back into the strided destination view.
    /// The destination must have the same extents as the source used for packing.
    void unpack(SrcMdspan& dst) const {
        if constexpr (SrcMdspan::rank() == 2) {
            detail::unpack_rank2<T>(buffer_, dst);
        } else if constexpr (SrcMdspan::rank() == 3) {
            detail::unpack_rank3<T>(buffer_, dst);
        }
        static_assert(SrcMdspan::rank() == 2 || SrcMdspan::rank() == 3,
                      "ContiguousBuffer only supports rank-2 and rank-3 mdspan types");
    }

    /// Raw pointer to contiguous data (for passing to Fortran).
    T* data() { return buffer_.data(); }
    const T* data() const { return buffer_.data(); }

    /// Number of elements in the buffer.
    std::size_t size() const { return buffer_.size(); }

private:
    std::vector<T> buffer_;
};

} // namespace ccpp
