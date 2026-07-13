"""
Property-Based Tests for Kind-Type Mapping Bijection (Property 1).

Feature: cpp-bridge-framework, Property 1: Kind-Type Mapping Bijection

Validates: Requirements 1.5

This module uses Hypothesis to verify that the capgen-ng emitter's kind mapping
function satisfies the bijection property:
  1. Every Fortran kind maps to exactly one C++ type (injective)
  2. Every C++ type in the target set maps back to its source kind (round-trip)
  3. Size in bytes matches between Fortran kind and mapped C++ type

The mapping under test corresponds to the compile-time mapping defined in
ccpp/framework/include/ccpp/ccpp_kind_map.hpp.
"""

import sys
from pathlib import Path

import pytest
from hypothesis import given, settings, assume
from hypothesis import strategies as st

# Ensure the tests directory is importable
sys.path.insert(0, str(Path(__file__).parent))

from kind_mapping import (
    CppTypeInfo,
    CharacterTypeInfo,
    FORTRAN_KIND_TO_CPP,
    map_kind_to_cpp,
    map_character_kind_to_cpp,
    map_cpp_to_kind,
    get_all_supported_kinds,
    get_canonical_kinds,
)


# ============================================================================
# Expected sizes for validation (from the C++ header ccpp_kind_map.hpp)
# ============================================================================

EXPECTED_SIZES: dict[str, int] = {
    "REAL64": 8,
    "kind_phys": 8,
    "REAL32": 4,
    "INT32": 4,
    "INT64": 8,
    "logical": 4,
}

EXPECTED_CPP_TYPES: dict[str, str] = {
    "REAL64": "double",
    "kind_phys": "double",
    "REAL32": "float",
    "INT32": "int32_t",
    "INT64": "int64_t",
    "logical": "int32_t",
}


# ============================================================================
# Strategies
# ============================================================================

# Strategy: any supported Fortran kind (including aliases)
all_kinds_strategy = st.sampled_from(get_all_supported_kinds())

# Strategy: canonical (non-alias) Fortran kinds for bijection testing
canonical_kinds_strategy = st.sampled_from(get_canonical_kinds())

# Strategy: character length N in [1, 1024]
character_length_strategy = st.integers(min_value=1, max_value=1024)


# ============================================================================
# Property 1.1: Every Fortran kind maps to exactly one C++ type (injective)
# ============================================================================


class TestKindMappingInjective:
    """Validates: Requirements 1.5"""

    @given(kind=all_kinds_strategy)
    @settings(max_examples=200)
    def test_every_kind_maps_to_one_type(self, kind: str):
        """
        Property: For any supported Fortran kind, map_kind_to_cpp returns
        exactly one CppTypeInfo (the function is total on its domain).

        **Validates: Requirements 1.5**
        """
        result = map_kind_to_cpp(kind)
        assert isinstance(result, CppTypeInfo)
        assert isinstance(result.cpp_type, str)
        assert len(result.cpp_type) > 0

    @given(kind=all_kinds_strategy)
    @settings(max_examples=200)
    def test_mapping_is_deterministic(self, kind: str):
        """
        Property: For any supported Fortran kind, calling map_kind_to_cpp
        multiple times returns the same result (deterministic/functional).

        **Validates: Requirements 1.5**
        """
        result1 = map_kind_to_cpp(kind)
        result2 = map_kind_to_cpp(kind)
        assert result1 == result2

    @given(length=character_length_strategy)
    @settings(max_examples=200)
    def test_character_kind_maps_to_array(self, length: int):
        """
        Property: For any valid character length N >= 1,
        map_character_kind_to_cpp(N) returns a CharacterTypeInfo representing
        std::array<char, N>.

        **Validates: Requirements 1.5**
        """
        result = map_character_kind_to_cpp(length)
        assert isinstance(result, CharacterTypeInfo)
        assert result.cpp_type == f"std::array<char, {length}>"


# ============================================================================
# Property 1.2: Round-trip mapping (bijection on canonical kinds)
# ============================================================================


class TestKindMappingRoundTrip:
    """Validates: Requirements 1.5"""

    @given(kind=canonical_kinds_strategy)
    @settings(max_examples=200)
    def test_forward_then_reverse_recovers_kind(self, kind: str):
        """
        Property: For any canonical Fortran kind, mapping forward to C++ type
        and then mapping back produces a Fortran kind that maps to the same
        C++ type. (Round-trip through the bijection.)

        For kinds that share a C++ type (INT32 and logical both map to int32_t),
        the reverse mapping returns the canonical numeric kind. We verify the
        round-trip is consistent: forward(reverse(forward(kind))) == forward(kind).

        **Validates: Requirements 1.5**
        """
        # Forward: kind → cpp_type
        cpp_info = map_kind_to_cpp(kind)

        # Reverse: cpp_type → canonical_kind
        recovered_kind = map_cpp_to_kind(cpp_info.cpp_type)

        # The recovered kind maps to the same C++ type
        recovered_info = map_kind_to_cpp(recovered_kind)
        assert recovered_info.cpp_type == cpp_info.cpp_type
        assert recovered_info.size_bytes == cpp_info.size_bytes
        assert recovered_info.alignment == cpp_info.alignment

    @given(kind=st.sampled_from(["REAL64", "REAL32", "INT64"]))
    @settings(max_examples=200)
    def test_unique_types_have_exact_round_trip(self, kind: str):
        """
        Property: For kinds that map to a unique C++ type (not shared with
        another kind), the round-trip is exact: reverse(forward(kind)) == kind.

        REAL64→double→REAL64, REAL32→float→REAL32, INT64→int64_t→INT64

        **Validates: Requirements 1.5**
        """
        cpp_info = map_kind_to_cpp(kind)
        recovered_kind = map_cpp_to_kind(cpp_info.cpp_type)
        assert recovered_kind == kind

    def test_alias_kind_phys_maps_same_as_real64(self):
        """
        Verify that 'kind_phys' is treated as an alias for REAL64.

        **Validates: Requirements 1.5**
        """
        real64_info = map_kind_to_cpp("REAL64")
        kind_phys_info = map_kind_to_cpp("kind_phys")
        assert real64_info == kind_phys_info


# ============================================================================
# Property 1.3: Size in bytes matches between Fortran kind and C++ type
# ============================================================================


class TestKindTypeSizeEquivalence:
    """Validates: Requirements 1.5"""

    @given(kind=all_kinds_strategy)
    @settings(max_examples=200)
    def test_size_matches_expected(self, kind: str):
        """
        Property: For any supported Fortran kind, the mapped C++ type's
        size_bytes equals the expected Fortran kind size.

        This verifies size equivalence between the Fortran and C++ worlds.

        **Validates: Requirements 1.5**
        """
        cpp_info = map_kind_to_cpp(kind)
        expected_size = EXPECTED_SIZES[kind]
        assert cpp_info.size_bytes == expected_size, (
            f"Kind '{kind}' → C++ type '{cpp_info.cpp_type}' has size "
            f"{cpp_info.size_bytes}, expected {expected_size}"
        )

    @given(kind=all_kinds_strategy)
    @settings(max_examples=200)
    def test_cpp_type_matches_expected(self, kind: str):
        """
        Property: For any supported Fortran kind, the mapped C++ type name
        matches the expected type from the kind map specification.

        **Validates: Requirements 1.5**
        """
        cpp_info = map_kind_to_cpp(kind)
        expected_type = EXPECTED_CPP_TYPES[kind]
        assert cpp_info.cpp_type == expected_type, (
            f"Kind '{kind}' mapped to '{cpp_info.cpp_type}', "
            f"expected '{expected_type}'"
        )

    @given(length=character_length_strategy)
    @settings(max_examples=200)
    def test_character_size_equals_length(self, length: int):
        """
        Property: For any character(len=N), the C++ type size is exactly N bytes.

        **Validates: Requirements 1.5**
        """
        char_info = map_character_kind_to_cpp(length)
        assert char_info.size_bytes == length

    @given(length=character_length_strategy)
    @settings(max_examples=200)
    def test_character_alignment_is_one(self, length: int):
        """
        Property: For any character(len=N), the C++ type alignment is 1 byte.

        **Validates: Requirements 1.5**
        """
        char_info = map_character_kind_to_cpp(length)
        assert char_info.alignment == 1

    @given(kind=all_kinds_strategy)
    @settings(max_examples=200)
    def test_alignment_is_positive(self, kind: str):
        """
        Property: For any supported Fortran kind, the mapped C++ type's
        alignment is positive and divides evenly into the size.

        **Validates: Requirements 1.5**
        """
        cpp_info = map_kind_to_cpp(kind)
        assert cpp_info.alignment > 0
        assert cpp_info.size_bytes % cpp_info.alignment == 0
