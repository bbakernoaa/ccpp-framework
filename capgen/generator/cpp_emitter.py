"""C++ code emitter for capgen-ng.

This module provides utilities and a driver class for emitting C++ source
artifacts (proxy wrapper headers/sources, suite factories, host accessors)
from the resolved suite structure produced by :mod:`generator.suite_resolver`.

Typical usage::

    emitter = CppEmitter(suite_resolution)
    emitter.emit_proxy_header(resolved_call, output_dir)
    emitter.emit_proxy_source(resolved_call, output_dir)
    emitter.emit_suite_factory(output_dir)
    emitter.emit_host_accessor(host_vars, output_dir)
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Dict, List, Optional, Tuple

if TYPE_CHECKING:
    from generator.suite_resolver import (
        ResolvedArg,
        ResolvedCall,
        ResolvedGroup,
        SuiteResolution,
    )

########################################################################
# Kind-Type Mapping
########################################################################

#: Mapping from Fortran kind parameter strings to C++ type strings.
KIND_TYPE_MAP: Dict[str, str] = {
    "REAL64": "double",
    "kind_phys": "double",
    "REAL32": "float",
    "INT32": "std::int32_t",
    "INT64": "std::int64_t",
    "logical": "std::int32_t",
    "character": "char",
}


def fortran_kind_to_cpp_type(kind_str: str) -> str:
    """Map a Fortran kind string to the corresponding C++ type.

    >>> fortran_kind_to_cpp_type('REAL64')
    'double'
    >>> fortran_kind_to_cpp_type('kind_phys')
    'double'
    >>> fortran_kind_to_cpp_type('INT32')
    'std::int32_t'
    """
    return KIND_TYPE_MAP[kind_str]


########################################################################
# C++ Code Generation Utilities
########################################################################

def generate_header_guard(filename: str) -> Tuple[str, str]:
    """Generate C++ header guard directives for *filename*.

    >>> top, bottom = generate_header_guard('my_header.hpp')
    >>> print(top)
    #ifndef MY_HEADER_HPP_
    #define MY_HEADER_HPP_
    >>> print(bottom)
    #endif // MY_HEADER_HPP_
    """
    guard = ""
    for ch in filename:
        guard += ch.upper() if ch.isalnum() else "_"
    guard += "_"
    top = "#ifndef {}\n#define {}".format(guard, guard)
    bottom = "#endif // {}".format(guard)
    return top, bottom


def generate_namespace_open(namespace: str) -> str:
    """Return opening brace for a C++ namespace.

    >>> generate_namespace_open('ccpp')
    'namespace ccpp {'
    """
    return "namespace {} {{".format(namespace)


def generate_namespace_close(namespace: str) -> str:
    """Return closing brace for a C++ namespace.

    >>> generate_namespace_close('ccpp')
    '} // namespace ccpp'
    """
    return "}} // namespace {}".format(namespace)


def generate_includes(headers: List[str]) -> str:
    """Generate #include directives. System headers start with '<'.

    >>> print(generate_includes(['<cstdint>', 'ccpp_scheme_base.hpp']))
    #include <cstdint>
    #include "ccpp_scheme_base.hpp"
    """
    lines = []
    for h in headers:
        if h.startswith("<"):
            lines.append("#include {}".format(h))
        else:
            lines.append('#include "{}"'.format(h))
    return "\n".join(lines)


def generate_extern_c_open() -> str:
    """>>> generate_extern_c_open()
    'extern \"C\" {'
    """
    return 'extern "C" {'


def generate_extern_c_close() -> str:
    """>>> generate_extern_c_close()
    '} // extern \"C\"'
    """
    return '} // extern "C"'


########################################################################
# Simple Data Model for Proxy Emission
########################################################################

#: Supported physics phase names corresponding to SchemeBase virtual methods.
PHASE_METHOD_MAP: Dict[str, str] = {
    "init": "phase_init",
    "timestep_init": "phase_timestep_init",
    "run": "phase_run",
    "timestep_final": "phase_timestep_final",
    "final": "phase_final",
    "register": "phase_register",
}


@dataclass
class HostVarAccessor:
    """Describes one host variable for C++ accessor emission."""
    standard_name: str
    local_name: str
    kind: str
    rank: int
    dimensions: List[str] = field(default_factory=list)
    intent: str = "inout"


@dataclass
class SchemeArg:
    """Describes one argument of a scheme phase for proxy emission."""
    standard_name: str
    local_name: str
    intent: str
    rank: int
    kind: str


@dataclass
class SchemePhase:
    """Describes one phase of a scheme."""
    phase_name: str
    args: List[SchemeArg] = field(default_factory=list)


@dataclass
class SchemeDescriptor:
    """Describes a Fortran scheme for C++ proxy wrapper generation."""
    name: str
    language: str = "Fortran"
    phases: List[SchemePhase] = field(default_factory=list)


########################################################################
# Proxy Emission Helpers
########################################################################

def _fortran_bind_c_symbol(scheme_name: str, phase_name: str) -> str:
    """Return the bind(C) mangled symbol name for a Fortran scheme phase.

    >>> _fortran_bind_c_symbol('mp_thompson', 'run')
    'mp_thompson_run'
    >>> _fortran_bind_c_symbol('RRTMGP_SW', 'init')
    'rrtmgp_sw_init'
    """
    return "{}_{}".format(scheme_name.lower(), phase_name.lower())


def _cpp_type_for_arg(arg: SchemeArg, kind_map: Dict[str, str]) -> str:
    """Return the C++ type string for a scheme argument."""
    return kind_map.get(arg.kind, "double")


def _extern_c_param(arg: SchemeArg, kind_map: Dict[str, str]) -> str:
    """Return the extern C function parameter declaration for one argument."""
    base = _cpp_type_for_arg(arg, kind_map)
    return "{}* {}".format(base, arg.local_name)


def _build_proxy_header_lines(
    scheme: SchemeDescriptor, kind_map: Dict[str, str]
) -> List[str]:
    """Build lines for a proxy wrapper header file."""
    lines: List[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include <string>')
    lines.append('#include "ccpp_scheme_base.hpp"')
    lines.append('#include "ccpp_contiguous_buffer.hpp"')
    lines.append("")
    lines.append("namespace ccpp {")
    lines.append("namespace generated {")
    lines.append("")
    class_name = "SchemeProxy_{}".format(scheme.name)
    lines.append("class {} : public SchemeBase {{".format(class_name))
    lines.append("public:")
    lines.append("    const std::string& name() const override {")
    lines.append('        static const std::string n = "{}";'.format(scheme.name))
    lines.append("        return n;")
    lines.append("    }")
    lines.append("")
    for phase in scheme.phases:
        method_name = PHASE_METHOD_MAP.get(phase.phase_name)
        if method_name and method_name != "phase_register":
            lines.append(
                "    SchemeError {}(SchemeContext& ctx) override;".format(method_name))
    lines.append("")
    lines.append("private:")
    buffer_decls = set()
    for phase in scheme.phases:
        for arg in phase.args:
            if arg.rank > 0 and arg.intent in ("in", "inout"):
                base = kind_map.get(arg.kind, "double")
                buf_name = "{}_buf_".format(arg.local_name)
                if buf_name not in buffer_decls:
                    buffer_decls.add(buf_name)
                    lines.append(
                        "    ContiguousBuffer<{}> {};".format(base, buf_name))
    lines.append("};")
    lines.append("")
    lines.append("} // namespace generated")
    lines.append("} // namespace ccpp")
    return lines


def _build_proxy_source_lines(
    scheme: SchemeDescriptor, kind_map: Dict[str, str]
) -> List[str]:
    """Build lines for a proxy wrapper source file."""
    filename_hpp = "scheme_proxy_{}.hpp".format(scheme.name.lower())
    lines: List[str] = []
    lines.append('#include "{}"'.format(filename_hpp))
    lines.append('#include "ccpp_scheme_context.hpp"')
    lines.append("")
    lines.append('extern "C" {')
    for phase in scheme.phases:
        if phase.phase_name == "register":
            continue
        symbol = _fortran_bind_c_symbol(scheme.name, phase.phase_name)
        params = [_extern_c_param(arg, kind_map) for arg in phase.args]
        params_str = ", ".join(params) if params else "void"
        lines.append("void {}({});".format(symbol, params_str))
    lines.append('} // extern "C"')
    lines.append("")
    lines.append("namespace ccpp {")
    lines.append("namespace generated {")
    lines.append("")
    class_name = "SchemeProxy_{}".format(scheme.name)
    for phase in scheme.phases:
        method_name = PHASE_METHOD_MAP.get(phase.phase_name)
        if not method_name or method_name == "phase_register":
            continue
        lines.append("SchemeError {}::{}(SchemeContext& ctx) {{".format(
            class_name, method_name))
        symbol = _fortran_bind_c_symbol(scheme.name, phase.phase_name)
        errflg_arg: Optional[SchemeArg] = None
        errmsg_arg: Optional[SchemeArg] = None
        for arg in phase.args:
            if arg.standard_name == "ccpp_error_code":
                errflg_arg = arg
            elif arg.standard_name == "ccpp_error_message":
                errmsg_arg = arg
        if errflg_arg:
            lines.append("    {} {} = 0;".format(
                kind_map.get(errflg_arg.kind, "std::int32_t"), errflg_arg.local_name))
        if errmsg_arg:
            lines.append('    char {}[512] = {{}};'.format(errmsg_arg.local_name))
        call_parts: List[str] = []
        for arg in phase.args:
            if arg.standard_name == "ccpp_error_code":
                call_parts.append("&{}".format(arg.local_name))
            elif arg.standard_name == "ccpp_error_message":
                call_parts.append(arg.local_name)
            elif arg.rank == 0:
                call_parts.append("/* &{} */".format(arg.local_name))
            else:
                call_parts.append("/* {}.data_handle() */".format(arg.local_name))
        if call_parts:
            lines.append("    {}({});".format(symbol, ", ".join(call_parts)))
        else:
            lines.append("    {}();".format(symbol))
        lines.append("")
        if errflg_arg:
            lines.append("    SchemeError err;")
            lines.append("    err.code = {};".format(errflg_arg.local_name))
            if errmsg_arg:
                lines.append("    if (err.code != 0) {")
                lines.append("        err.message = std::string({});".format(
                    errmsg_arg.local_name))
                lines.append("    }")
            lines.append("    return err;")
        else:
            lines.append("    return SchemeError{};")
        lines.append("}")
        lines.append("")
    lines.append("} // namespace generated")
    lines.append("} // namespace ccpp")
    return lines


def _scheme_descriptor_from_resolved_call(
    resolved_call: "ResolvedCall",
) -> SchemeDescriptor:
    """Build a SchemeDescriptor from a ResolvedCall object."""
    args = []
    for ra in resolved_call.args:
        rank = len(ra.scheme_dimensions) if ra.scheme_dimensions else 0
        kind = ra.kind_scheme if ra.kind_scheme else "REAL64"
        args.append(SchemeArg(
            standard_name=ra.standard_name,
            local_name=ra.scheme_local_name,
            intent=ra.intent, rank=rank, kind=kind))
    phase = SchemePhase(phase_name=resolved_call.phase, args=args)
    return SchemeDescriptor(name=resolved_call.scheme_name, language="Fortran", phases=[phase])


########################################################################
# Contiguity Detection and Buffer Injection
########################################################################

def _is_sliced_arg(arg: "ResolvedArg") -> bool:
    """Determine whether *arg* originates from a submdspan slice."""
    if arg.host_entry is None:
        return False
    host_rank = len(arg.host_entry.dimensions)
    scheme_rank = len(arg.scheme_dimensions)
    if scheme_rank < host_rank:
        return True
    if arg.host_entry.local_subscript:
        fixed_count = sum(1 for tok in arg.host_entry.local_subscript if tok.strip() != ':')
        if fixed_count > 0:
            return True
    return False


def needs_contiguous_pack(arg: "ResolvedArg", scheme_language: str) -> bool:
    """Determine whether *arg* requires ContiguousBuffer pack/unpack."""
    if not _is_sliced_arg(arg):
        return False
    lang = scheme_language.strip().lower()
    if lang.startswith("c"):
        return False
    return True


def emit_pack_unpack(arg: "ResolvedArg", scheme_language: str) -> Tuple[str, str]:
    """Generate pack/unpack C++ code for a sliced argument."""
    if not needs_contiguous_pack(arg, scheme_language):
        return ("", "")
    buf_name = "{}_buf".format(arg.scheme_local_name)
    view_name = "{}_view".format(arg.scheme_local_name)
    cpp_type = KIND_TYPE_MAP.get(arg.kind_scheme, "double")
    pre_lines = [
        "// Pack strided submdspan slice into contiguous buffer for Fortran",
        "auto {view} = {expr};".format(view=view_name, expr=arg.call_expr),
        "ccpp::ContiguousBuffer<{type}, decltype({view})> {buf};".format(
            type=cpp_type, view=view_name, buf=buf_name),
        "{buf}.pack({view});".format(buf=buf_name, view=view_name),
    ]
    pre_call_code = "\n".join(pre_lines)
    post_call_code = ""
    if arg.intent in ("out", "inout"):
        post_lines = [
            "// Unpack contiguous buffer back into strided submdspan",
            "{buf}.unpack({view});".format(buf=buf_name, view=view_name),
        ]
        post_call_code = "\n".join(post_lines)
    return (pre_call_code, post_call_code)


def generate_phase_body_with_packing(
    resolved_call: "ResolvedCall", scheme_language: str,
) -> str:
    """Generate a phase method body with pack/unpack wrapping as needed."""
    pre_sections: List[str] = []
    post_sections: List[str] = []
    for arg in resolved_call.args:
        pre, post = emit_pack_unpack(arg, scheme_language)
        if pre:
            pre_sections.append(pre)
        if post:
            post_sections.append(post)
    lines: List[str] = []
    if pre_sections:
        lines.append("// --- Pre-call packing ---")
        lines.extend(pre_sections)
        lines.append("")
    lines.append("// --- Fortran scheme call ---")
    lines.append("// {scheme}_{phase}(...);  // actual call site".format(
        scheme=resolved_call.scheme_name, phase=resolved_call.phase))
    if post_sections:
        lines.append("")
        lines.append("// --- Post-call unpacking ---")
        lines.extend(post_sections)
    return "\n".join(lines)


########################################################################
# Suite Factory Data Model and Emission
########################################################################

@dataclass
class PipelineItemDescriptor:
    """Describes one item in a group's pipeline for factory emission."""
    kind: str  # "scheme" or "subcycle"
    scheme_name: str = ""
    loop_count: str = "1"
    children: List["PipelineItemDescriptor"] = field(default_factory=list)


@dataclass
class GroupDescriptor:
    """Describes one group in a suite for factory emission."""
    name: str
    items: List[PipelineItemDescriptor] = field(default_factory=list)


@dataclass
class SuiteFactoryDescriptor:
    """Describes a full suite structure for factory emission."""
    suite_name: str
    groups: List[GroupDescriptor] = field(default_factory=list)
    init_scheme: str = ""
    final_scheme: str = ""
    all_scheme_names: List[str] = field(default_factory=list)


def _collect_scheme_names_from_items(
    items: List[PipelineItemDescriptor],
) -> List[str]:
    """Recursively collect all scheme names from pipeline items."""
    names: List[str] = []
    for item in items:
        if item.kind == "scheme":
            names.append(item.scheme_name)
        elif item.kind == "subcycle":
            names.extend(_collect_scheme_names_from_items(item.children))
    return names


#: Non-run phases in chronological order for deterministic ordering.
_NON_RUN_PHASES: Tuple[str, ...] = (
    "init", "timestep_init", "timestep_final", "final",
)


def _build_suite_factory_descriptor_from_resolution(
    suite_resolution: "SuiteResolution",
) -> SuiteFactoryDescriptor:
    """Build a SuiteFactoryDescriptor from a SuiteResolution object.

    The 'run' phase call list forms the base of each group's pipeline.
    Schemes that implement ONLY non-run phases (e.g. GFS_time_vary_pre)
    are appended as flat pipeline items so SchemeElement dispatches them
    for init/timestep_init/timestep_final/final phases.
    """
    from generator.suite_resolver import ResolvedCall, ResolvedSubcycle

    def _items_from_phase_list(phase_items: list) -> List[PipelineItemDescriptor]:
        result: List[PipelineItemDescriptor] = []
        for pi in phase_items:
            if isinstance(pi, ResolvedCall):
                result.append(PipelineItemDescriptor(
                    kind="scheme", scheme_name=pi.scheme_name))
            elif isinstance(pi, ResolvedSubcycle):
                children = _items_from_phase_list(pi.calls)
                result.append(PipelineItemDescriptor(
                    kind="subcycle", loop_count=str(pi.loop), children=children))
        return result

    groups: List[GroupDescriptor] = []
    for rg in suite_resolution.groups:
        run_items = rg.phase_calls.get("run", [])
        items = _items_from_phase_list(run_items)
        covered = set(_collect_scheme_names_from_items(items))
        for phase_name in _NON_RUN_PHASES:
            for pi in rg.phase_calls.get(phase_name, []):
                if isinstance(pi, ResolvedCall) and pi.scheme_name not in covered:
                    covered.add(pi.scheme_name)
                    items.append(PipelineItemDescriptor(
                        kind="scheme", scheme_name=pi.scheme_name))
        groups.append(GroupDescriptor(name=rg.group_name, items=items))

    all_names: List[str] = []
    for g in groups:
        all_names.extend(_collect_scheme_names_from_items(g.items))
    init_scheme = ""
    if suite_resolution.suite_init_call is not None:
        init_scheme = suite_resolution.suite_init_call.scheme_name
    final_scheme = ""
    if suite_resolution.suite_final_call is not None:
        final_scheme = suite_resolution.suite_final_call.scheme_name
    if init_scheme:
        all_names.append(init_scheme)
    if final_scheme:
        all_names.append(final_scheme)
    seen: set = set()
    unique_names: List[str] = []
    for n in all_names:
        if n not in seen:
            seen.add(n)
            unique_names.append(n)

    return SuiteFactoryDescriptor(
        suite_name=suite_resolution.suite_name, groups=groups,
        init_scheme=init_scheme, final_scheme=final_scheme,
        all_scheme_names=unique_names)


def _emit_pipeline_items(
    items: List[PipelineItemDescriptor], indent: str
) -> List[str]:
    """Emit C++ code that constructs PipelineElement objects for *items*."""
    lines: List[str] = []
    for i, item in enumerate(items):
        if item.kind == "scheme":
            lines.append(
                "{}elements.push_back(std::make_unique<ccpp::SchemeElement>("
                "schemes_[\"{}\"].get()));".format(indent, item.scheme_name))
        elif item.kind == "subcycle":
            vec_name = "subcycle_{}_elements".format(i)
            lines.append(
                "{}std::vector<std::unique_ptr<ccpp::PipelineElement>> {};".format(
                    indent, vec_name))
            child_lines = _emit_subcycle_children(item.children, indent, vec_name)
            lines.extend(child_lines)
            lines.append(
                "{}elements.push_back(std::make_unique<ccpp::SubcycleElement>("
                "{}, std::move({})));".format(indent, item.loop_count, vec_name))
    return lines


def _emit_subcycle_children(
    items: List[PipelineItemDescriptor], indent: str, vec_name: str
) -> List[str]:
    """Emit C++ code that pushes children into a named vector variable."""
    lines: List[str] = []
    for j, child in enumerate(items):
        if child.kind == "scheme":
            lines.append(
                "{}{}.push_back(std::make_unique<ccpp::SchemeElement>("
                "schemes_[\"{}\"].get()));".format(indent, vec_name, child.scheme_name))
        elif child.kind == "subcycle":
            nested_vec = "{}_{}_elements".format(vec_name, j)
            lines.append(
                "{}std::vector<std::unique_ptr<ccpp::PipelineElement>> {};".format(
                    indent, nested_vec))
            nested_lines = _emit_subcycle_children(child.children, indent, nested_vec)
            lines.extend(nested_lines)
            lines.append(
                "{}{}.push_back(std::make_unique<ccpp::SubcycleElement>("
                "{}, std::move({})));".format(indent, vec_name, child.loop_count, nested_vec))
    return lines


def _build_suite_factory_lines(descriptor: SuiteFactoryDescriptor) -> List[str]:
    """Build the C++ source lines for a suite factory file."""
    lines: List[str] = []
    lines.append("// Auto-generated by capgen-ng C++ emitter — suite factory for '{}'".format(
        descriptor.suite_name))
    lines.append("// DO NOT EDIT — regenerated from suite XML definitions.")
    lines.append("")
    includes = ["<memory>", "<string>", "<vector>", "<unordered_map>",
                "ccpp_suite.hpp", "ccpp_group.hpp", "ccpp_pipeline_element.hpp",
                "ccpp_scheme_base.hpp", "ccpp_framework_orchestrator.hpp"]
    for scheme_name in descriptor.all_scheme_names:
        includes.append("scheme_proxy_{}.hpp".format(scheme_name.lower()))
    lines.append(generate_includes(includes))
    lines.append("")
    lines.append("namespace ccpp {")
    lines.append("namespace generated {")
    lines.append("")
    factory_class = "SuiteFactory_{}".format(descriptor.suite_name)
    lines.append("class {} {{".format(factory_class))
    lines.append("public:")
    lines.append("    std::unique_ptr<ccpp::Suite> create() {")
    indent = "        "
    lines.append("")
    lines.append("{}// Construct scheme instances".format(indent))
    for scheme_name in descriptor.all_scheme_names:
        proxy_class = "SchemeProxy_{}".format(scheme_name)
        lines.append('{}schemes_["{}"] = std::make_unique<{}>();'.format(
            indent, scheme_name, proxy_class))
    lines.append("")
    lines.append("{}// Build groups".format(indent))
    lines.append("{}std::vector<std::unique_ptr<ccpp::Group>> groups;".format(indent))
    lines.append("")
    for group in descriptor.groups:
        lines.append("{}// Group: {}".format(indent, group.name))
        lines.append("{}{{".format(indent))
        inner_indent = indent + "    "
        lines.append("{}std::vector<std::unique_ptr<ccpp::PipelineElement>> elements;".format(
            inner_indent))
        item_lines = _emit_pipeline_items(group.items, inner_indent)
        lines.extend(item_lines)
        lines.append('{}groups.push_back(std::make_unique<ccpp::Group>("{}", '
                     "std::move(elements)));".format(inner_indent, group.name))
        lines.append("{}}}".format(indent))
        lines.append("")
    lines.append("{}// Construct suite".format(indent))
    init_ptr = "nullptr"
    final_ptr = "nullptr"
    if descriptor.init_scheme:
        init_ptr = 'schemes_["{}"].get()'.format(descriptor.init_scheme)
    if descriptor.final_scheme:
        final_ptr = 'schemes_["{}"].get()'.format(descriptor.final_scheme)
    lines.append('{}return std::make_unique<ccpp::Suite>("{}", std::move(groups), '
                 "{}, {});".format(indent, descriptor.suite_name, init_ptr, final_ptr))
    lines.append("    }")
    lines.append("")
    lines.append("private:")
    lines.append("    std::unordered_map<std::string, "
                 "std::unique_ptr<ccpp::SchemeBase>> schemes_;")
    lines.append("};")
    lines.append("")
    lines.append("/// Create a fully-constructed Suite for '{}'.".format(descriptor.suite_name))
    lines.append("std::unique_ptr<ccpp::Suite> create_suite_{}() {{".format(
        descriptor.suite_name))
    lines.append("    {} factory;".format(factory_class))
    lines.append("    return factory.create();")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace generated")
    lines.append("} // namespace ccpp")
    lines.append("")
    # Self-registering static initializer
    lines.append("namespace {")
    lines.append("struct RegisterSuiteFactory_{} {{".format(descriptor.suite_name))
    lines.append("    RegisterSuiteFactory_{}() {{".format(descriptor.suite_name))
    lines.append('        ccpp::FrameworkOrchestrator::register_suite_factory('
                 '"{}", &ccpp::generated::create_suite_{});'.format(
                     descriptor.suite_name, descriptor.suite_name))
    lines.append("    }")
    lines.append("};")
    lines.append("const RegisterSuiteFactory_{0} g_register_suite_factory_{0};".format(
        descriptor.suite_name))
    lines.append("} // anonymous namespace")
    return lines


def build_suite_factory_from_descriptor(
    descriptor: SuiteFactoryDescriptor, output_dir: str,
) -> str:
    """Emit a suite factory source file from a SuiteFactoryDescriptor."""
    lines = _build_suite_factory_lines(descriptor)
    filename = "suite_factory_{}.cpp".format(descriptor.suite_name.lower())
    filepath = os.path.join(output_dir, filename)
    os.makedirs(output_dir, exist_ok=True)
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    return filepath


########################################################################
# Host Accessor Emission
########################################################################

def _accessor_function_name(standard_name: str) -> str:
    """Derive a C++ accessor function name from a CCPP standard name.

    >>> _accessor_function_name('air_temperature')
    'get_air_temperature'
    """
    safe = ""
    for ch in standard_name:
        safe += ch if (ch.isalnum() or ch == '_') else '_'
    return "get_{}".format(safe)


def _mdspan_return_type(cpp_type: str, rank: int) -> str:
    """Return the mdspan return type string for a given C++ type and rank.

    >>> _mdspan_return_type('double', 2)
    'mdspan_left<double, std::dextents<int, 2>>'
    """
    return "mdspan_left<{}, std::dextents<int, {}>>".format(cpp_type, rank)


def _build_host_accessor_lines(
    host_vars: List["HostVarAccessor"], kind_map: Dict[str, str],
) -> List[str]:
    """Build lines for the host_accessor.hpp header."""
    lines: List[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "ccpp_mdspan.hpp"')
    lines.append("#include <array>")
    lines.append("#include <cstddef>")
    lines.append("")
    lines.append("namespace ccpp {")
    lines.append("")
    for var in host_vars:
        if var.rank == 0:
            continue
        cpp_type = kind_map.get(var.kind, "double")
        func_name = _accessor_function_name(var.standard_name)
        return_type = _mdspan_return_type(cpp_type, var.rank)
        lines.append("/// Access host variable '{}' as a {}-D mdspan view.".format(
            var.standard_name, var.rank))
        const_qual = "const " if var.intent == "in" else ""
        lines.append("inline auto {}({}void* data, "
                     "const std::array<int, {}>& dims) -> {} {{".format(
                         func_name, const_qual, var.rank, return_type))
        lines.append("    auto* typed_ptr = static_cast<{}{}*>(data);".format(
            const_qual, cpp_type))
        lines.append("    return make_mdspan_left<{}{}, {}>(typed_ptr, dims);".format(
            const_qual + cpp_type if const_qual else cpp_type, "", var.rank))
        lines.append("}")
        lines.append("")
    lines.append("} // namespace ccpp")
    return lines


def _host_var_accessor_from_resolved_arg(arg: "ResolvedArg") -> "HostVarAccessor":
    """Convert a ResolvedArg to a HostVarAccessor for host accessor emission."""
    rank = len(arg.scheme_dimensions) if arg.scheme_dimensions else 0
    kind = arg.kind_host if arg.kind_host else (arg.kind_scheme if arg.kind_scheme else "REAL64")
    dims = list(arg.scheme_dimensions) if arg.scheme_dimensions else []
    return HostVarAccessor(
        standard_name=arg.standard_name, local_name=arg.scheme_local_name,
        kind=kind, rank=rank, dimensions=dims, intent=arg.intent)


def emit_host_accessor_from_descriptors(
    host_vars: List["HostVarAccessor"], output_dir: str,
    kind_map: Optional[Dict[str, str]] = None,
) -> str:
    """Emit host_accessor.hpp from a list of HostVarAccessor descriptors."""
    km = kind_map if kind_map is not None else dict(KIND_TYPE_MAP)
    lines = _build_host_accessor_lines(host_vars, km)
    filename = "host_accessor.hpp"
    filepath = os.path.join(output_dir, filename)
    os.makedirs(output_dir, exist_ok=True)
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    return filepath


########################################################################
# CppEmitter Class
########################################################################

class CppEmitter:
    """Driver class for emitting C++ source artifacts from resolved suites."""

    def __init__(self, suite_resolution: "SuiteResolution",
                 kind_map: Dict[str, str] | None = None) -> None:
        self.suite_resolution = suite_resolution
        self.kind_map: Dict[str, str] = (
            kind_map if kind_map is not None else dict(KIND_TYPE_MAP))

    def emit_proxy_header(self, resolved_call: "ResolvedCall", output_dir: str) -> str:
        """Emit a C++ proxy wrapper header for a Fortran scheme."""
        descriptor = _scheme_descriptor_from_resolved_call(resolved_call)
        lines = _build_proxy_header_lines(descriptor, self.kind_map)
        filename = "scheme_proxy_{}.hpp".format(descriptor.name.lower())
        filepath = os.path.join(output_dir, filename)
        os.makedirs(output_dir, exist_ok=True)
        with open(filepath, "w") as f:
            f.write("\n".join(lines) + "\n")
        return filepath

    def emit_proxy_source(self, resolved_call: "ResolvedCall", output_dir: str) -> str:
        """Emit the C++ proxy wrapper source for a Fortran scheme."""
        descriptor = _scheme_descriptor_from_resolved_call(resolved_call)
        lines = _build_proxy_source_lines(descriptor, self.kind_map)
        filename = "scheme_proxy_{}.cpp".format(descriptor.name.lower())
        filepath = os.path.join(output_dir, filename)
        os.makedirs(output_dir, exist_ok=True)
        with open(filepath, "w") as f:
            f.write("\n".join(lines) + "\n")
        return filepath

    def emit_suite_factory(self, output_dir: str) -> str:
        """Emit the suite factory source that constructs the pipeline tree."""
        descriptor = _build_suite_factory_descriptor_from_resolution(
            self.suite_resolution)
        return build_suite_factory_from_descriptor(descriptor, output_dir)

    def emit_host_accessor(self, host_vars: "List[ResolvedArg]", output_dir: str) -> str:
        """Emit the host accessor header with typed mdspan accessors."""
        accessors: List[HostVarAccessor] = []
        for arg in host_vars:
            accessors.append(_host_var_accessor_from_resolved_arg(arg))
        lines = _build_host_accessor_lines(accessors, self.kind_map)
        filename = "host_accessor.hpp"
        filepath = os.path.join(output_dir, filename)
        os.makedirs(output_dir, exist_ok=True)
        with open(filepath, "w") as f:
            f.write("\n".join(lines) + "\n")
        return filepath


########################################################################
# Standalone Emission Functions
########################################################################

def emit_proxy_header_from_descriptor(
    scheme: SchemeDescriptor, output_dir: str,
    kind_map: Optional[Dict[str, str]] = None,
) -> str:
    """Emit a proxy wrapper header from a SchemeDescriptor."""
    km = kind_map if kind_map is not None else dict(KIND_TYPE_MAP)
    lines = _build_proxy_header_lines(scheme, km)
    filename = "scheme_proxy_{}.hpp".format(scheme.name.lower())
    filepath = os.path.join(output_dir, filename)
    os.makedirs(output_dir, exist_ok=True)
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    return filepath


def emit_proxy_source_from_descriptor(
    scheme: SchemeDescriptor, output_dir: str,
    kind_map: Optional[Dict[str, str]] = None,
) -> str:
    """Emit a proxy wrapper source from a SchemeDescriptor."""
    km = kind_map if kind_map is not None else dict(KIND_TYPE_MAP)
    lines = _build_proxy_source_lines(scheme, km)
    filename = "scheme_proxy_{}.cpp".format(scheme.name.lower())
    filepath = os.path.join(output_dir, filename)
    os.makedirs(output_dir, exist_ok=True)
    with open(filepath, "w") as f:
        f.write("\n".join(lines) + "\n")
    return filepath
