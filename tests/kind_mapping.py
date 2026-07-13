"""
Kind-Type Mapping Module for capgen-ng C++ emitter.

This module defines the bijective mapping between Fortran kind parameters
and C++ numeric types, including size and alignment information. This is the
reference implementation that the capgen-ng C++ emitter will use for code
generation.

The mapping is equivalent to the compile-time mapping defined in
ccpp/framework/include/ccpp/ccpp_kind_map.hpp.
"""

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class CppTypeInfo:
    """Description of a C++ type mapped from a Fortran kind."""

    cpp_type: str  # C++ type name (e.g., "double", "float", "int32_t")
    size_bytes: int  # sizeof() in bytes
    alignment: int  # alignof() in bytes


# ============================================================================
# Forward mapping: Fortran kind → C++ type info
# ============================================================================

FORTRAN_KIND_TO_CPP: dict[str, CppTypeInfo] = {
    "REAL64": CppTypeInfo(cpp_type="double", size_bytes=8, alignment=8),
    "kind_phys": CppTypeInfo(cpp_type="double", size_bytes=8, alignment=8),
    "REAL32": CppTypeInfo(cpp_type="float", size_bytes=4, alignment=4),
    "INT32": CppTypeInfo(cpp_type="int32_t", size_bytes=4, alignment=4),
    "INT64": CppTypeInfo(cpp_type="int64_t", size_bytes=8, alignment=8),
    "logical": CppTypeInfo(cpp_type="int32_t", size_bytes=4, alignment=4),
}

# Character kinds are parameterized by length N
# character(len=N) → std::array<char, N> with size=N, alignment=1


@dataclass(frozen=True)
class CharacterTypeInfo:
    """C++ type info for character(len=N)."""

    length: int

    @property
    def cpp_type(self) -> str:
        return f"std::array<char, {self.length}>"

    @property
    def size_bytes(self) -> int:
        return self.length

    @property
    def alignment(self) -> int:
        return 1


# ============================================================================
# Reverse mapping: C++ type → Fortran kind(s)
# ============================================================================

# For non-character types: maps C++ type name back to canonical Fortran kind.
# Note: "kind_phys" is an alias for REAL64, so the canonical reverse is "REAL64".
# "logical" also maps to int32_t but is semantically distinct from INT32.
CPP_TO_FORTRAN_KIND: dict[str, str] = {
    "double": "REAL64",
    "float": "REAL32",
    "int64_t": "INT64",
}

# int32_t is special: it can map back to either INT32 or logical.
# We define the canonical mapping for the numeric integer case.
# The logical case is handled separately.
CPP_INT32_FORTRAN_KINDS: list[str] = ["INT32", "logical"]


# ============================================================================
# Public API used by the capgen-ng emitter
# ============================================================================


def map_kind_to_cpp(fortran_kind: str) -> CppTypeInfo:
    """
    Map a Fortran kind parameter to its C++ type info.

    Args:
        fortran_kind: The Fortran kind string (e.g., "REAL64", "kind_phys",
                      "INT32", "INT64", "REAL32", "logical")

    Returns:
        CppTypeInfo with the mapped C++ type, size, and alignment.

    Raises:
        ValueError: If the kind is not supported.
    """
    if fortran_kind in FORTRAN_KIND_TO_CPP:
        return FORTRAN_KIND_TO_CPP[fortran_kind]
    raise ValueError(f"Unsupported Fortran kind: {fortran_kind}")


def map_character_kind_to_cpp(length: int) -> CharacterTypeInfo:
    """
    Map a Fortran character(len=N) to its C++ type info.

    Args:
        length: The character length N (must be >= 1).

    Returns:
        CharacterTypeInfo for std::array<char, N>.

    Raises:
        ValueError: If length < 1.
    """
    if length < 1:
        raise ValueError(f"Character length must be >= 1, got {length}")
    return CharacterTypeInfo(length=length)


def map_cpp_to_kind(cpp_type: str) -> str:
    """
    Map a C++ type name back to its canonical Fortran kind.

    For non-character, non-ambiguous types this is a direct reverse lookup.
    For int32_t, returns "INT32" as the canonical numeric kind.

    Args:
        cpp_type: The C++ type string (e.g., "double", "float", "int32_t", "int64_t")

    Returns:
        The canonical Fortran kind string.

    Raises:
        ValueError: If the C++ type is not in the target set.
    """
    if cpp_type == "int32_t":
        return "INT32"
    if cpp_type in CPP_TO_FORTRAN_KIND:
        return CPP_TO_FORTRAN_KIND[cpp_type]
    raise ValueError(f"Unsupported C++ type: {cpp_type}")


def get_all_supported_kinds() -> list[str]:
    """Return all supported Fortran kind parameter names (excluding character)."""
    return list(FORTRAN_KIND_TO_CPP.keys())


def get_canonical_kinds() -> list[str]:
    """
    Return canonical (non-alias) Fortran kinds for bijection testing.

    Excludes 'kind_phys' since it's an alias for REAL64.
    """
    return ["REAL64", "REAL32", "INT32", "INT64", "logical"]
