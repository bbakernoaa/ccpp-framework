#ifndef CCPP_KIND_MAP_HPP
#define CCPP_KIND_MAP_HPP

/// @file ccpp_kind_map.hpp
/// @brief Compile-time mapping from Fortran kind parameters to C++ types.
///
/// This header provides a type-level mapping between Fortran kind parameters
/// (REAL64, REAL32, INT32, INT64, character, logical) and their corresponding
/// C++ numeric types. The mapping is used by the capgen-ng C++ emitter for
/// code generation validation and by the runtime framework for type safety.

#include <array>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace ccpp {

// ============================================================================
// Fortran Kind Tag Types
// ============================================================================

/// Tag types representing Fortran kind parameters.
/// Used as template arguments for compile-time type dispatch.
struct kind_real64 {};
struct kind_real32 {};
struct kind_int32 {};
struct kind_int64 {};
template <std::size_t N> struct kind_character {};
struct kind_logical {};

// ============================================================================
// Primary Kind-to-Type Mapping (via template specialization)
// ============================================================================

/// Primary template — unspecialized form causes a compile error for unsupported kinds.
template <typename FortranKind>
struct kind_type_map;

/// REAL64 / kind_phys → double (default physics precision)
template <>
struct kind_type_map<kind_real64> {
    using type = double;
    static constexpr std::size_t fortran_size = 8;
    static constexpr std::size_t fortran_alignment = 8;
};

/// REAL32 → float (32-bit physics builds)
template <>
struct kind_type_map<kind_real32> {
    using type = float;
    static constexpr std::size_t fortran_size = 4;
    static constexpr std::size_t fortran_alignment = 4;
};

/// INT32 → int32_t (standard integer)
template <>
struct kind_type_map<kind_int32> {
    using type = std::int32_t;
    static constexpr std::size_t fortran_size = 4;
    static constexpr std::size_t fortran_alignment = 4;
};

/// INT64 → int64_t (extended integer)
template <>
struct kind_type_map<kind_int64> {
    using type = std::int64_t;
    static constexpr std::size_t fortran_size = 8;
    static constexpr std::size_t fortran_alignment = 8;
};

/// character(len=N) → std::array<char, N> (fixed-length strings)
template <std::size_t N>
struct kind_type_map<kind_character<N>> {
    using type = std::array<char, N>;
    static constexpr std::size_t fortran_size = N;
    static constexpr std::size_t fortran_alignment = 1;
};

/// logical → int32_t (Fortran logical as C int, 0/1)
template <>
struct kind_type_map<kind_logical> {
    using type = std::int32_t;
    static constexpr std::size_t fortran_size = 4;
    static constexpr std::size_t fortran_alignment = 4;
};

// ============================================================================
// Convenience Alias
// ============================================================================

/// Alias template for extracting the mapped C++ type from a Fortran kind tag.
template <typename FortranKind>
using kind_type_t = typename kind_type_map<FortranKind>::type;

// ============================================================================
// Compile-Time Size/Alignment Verification
// ============================================================================

/// Verify that the C++ type size matches the expected Fortran kind size.
/// Returns true if sizeof(mapped_type) == fortran_size for the given kind.
template <typename FortranKind>
constexpr bool verify_kind_size() {
    return sizeof(kind_type_t<FortranKind>) == kind_type_map<FortranKind>::fortran_size;
}

/// Verify that the C++ type alignment is compatible with Fortran alignment.
/// Returns true if alignof(mapped_type) >= fortran_alignment for the given kind.
template <typename FortranKind>
constexpr bool verify_kind_alignment() {
    return alignof(kind_type_t<FortranKind>) >= kind_type_map<FortranKind>::fortran_alignment;
}

/// Combined size and alignment verification for a given kind.
template <typename FortranKind>
constexpr bool verify_kind_compat() {
    return verify_kind_size<FortranKind>() && verify_kind_alignment<FortranKind>();
}

// ============================================================================
// Static Assertions (compile-time guarantees)
// ============================================================================

static_assert(verify_kind_compat<kind_real64>(),
    "REAL64 mapped type must be 8 bytes with >= 8-byte alignment");
static_assert(verify_kind_compat<kind_real32>(),
    "REAL32 mapped type must be 4 bytes with >= 4-byte alignment");
static_assert(verify_kind_compat<kind_int32>(),
    "INT32 mapped type must be 4 bytes with >= 4-byte alignment");
static_assert(verify_kind_compat<kind_int64>(),
    "INT64 mapped type must be 8 bytes with >= 8-byte alignment");
static_assert(verify_kind_compat<kind_logical>(),
    "logical mapped type must be 4 bytes with >= 4-byte alignment");
static_assert(verify_kind_compat<kind_character<1>>(),
    "character(len=1) mapped type must be 1 byte");

// Verify type identity guarantees
static_assert(std::is_same_v<kind_type_t<kind_real64>, double>,
    "REAL64 must map to double");
static_assert(std::is_same_v<kind_type_t<kind_real32>, float>,
    "REAL32 must map to float");
static_assert(std::is_same_v<kind_type_t<kind_int32>, std::int32_t>,
    "INT32 must map to int32_t");
static_assert(std::is_same_v<kind_type_t<kind_int64>, std::int64_t>,
    "INT64 must map to int64_t");
static_assert(std::is_same_v<kind_type_t<kind_logical>, std::int32_t>,
    "logical must map to int32_t");
static_assert(std::is_same_v<kind_type_t<kind_character<10>>, std::array<char, 10>>,
    "character(len=10) must map to std::array<char, 10>");

// ============================================================================
// Enum for Runtime Kind Identification
// ============================================================================

/// Runtime-usable enumeration of supported Fortran kinds.
/// Useful for capgen-ng emitter validation and dynamic dispatch.
enum class FortranKind : int {
    real64 = 0,
    real32 = 1,
    int32 = 2,
    int64 = 3,
    character = 4,
    logical = 5
};

/// Returns the expected C++ type size in bytes for a given runtime kind enum.
constexpr std::size_t kind_size_bytes(FortranKind k) {
    switch (k) {
        case FortranKind::real64:    return 8;
        case FortranKind::real32:    return 4;
        case FortranKind::int32:     return 4;
        case FortranKind::int64:     return 8;
        case FortranKind::character: return 1;  // per-character; actual size is len*1
        case FortranKind::logical:   return 4;
    }
    return 0;  // unreachable
}

/// Returns the expected C++ type alignment in bytes for a given runtime kind enum.
constexpr std::size_t kind_alignment_bytes(FortranKind k) {
    switch (k) {
        case FortranKind::real64:    return 8;
        case FortranKind::real32:    return 4;
        case FortranKind::int32:     return 4;
        case FortranKind::int64:     return 8;
        case FortranKind::character: return 1;
        case FortranKind::logical:   return 4;
    }
    return 0;  // unreachable
}

/// Total number of supported Fortran kind categories.
constexpr int kind_count = 6;

} // namespace ccpp

#endif // CCPP_KIND_MAP_HPP
