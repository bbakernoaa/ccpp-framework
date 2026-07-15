"""Unit tests for generator.cpp_emitter."""

import os
import tempfile
import unittest

from generator.cpp_emitter import (
    KIND_TYPE_MAP,
    PHASE_METHOD_MAP,
    CppEmitter,
    GroupDescriptor,
    HostVarAccessor,
    PipelineItemDescriptor,
    SchemeArg,
    SchemeDescriptor,
    SchemePhase,
    SuiteFactoryDescriptor,
    _accessor_function_name,
    _build_host_accessor_lines,
    _build_proxy_header_lines,
    _build_proxy_source_lines,
    _build_suite_factory_lines,
    _collect_scheme_names_from_items,
    _emit_pipeline_items,
    _extern_c_param,
    _fortran_bind_c_symbol,
    _cpp_type_for_arg,
    _mdspan_return_type,
    build_suite_factory_from_descriptor,
    emit_host_accessor_from_descriptors,
    emit_proxy_header_from_descriptor,
    emit_proxy_source_from_descriptor,
    fortran_kind_to_cpp_type,
    generate_extern_c_close,
    generate_extern_c_open,
    generate_header_guard,
    generate_includes,
    generate_namespace_close,
    generate_namespace_open,
)


class TestKindTypeMap(unittest.TestCase):
    def test_real64_maps_to_double(self):
        self.assertEqual(fortran_kind_to_cpp_type("REAL64"), "double")

    def test_kind_phys_maps_to_double(self):
        self.assertEqual(fortran_kind_to_cpp_type("kind_phys"), "double")

    def test_int32_maps_to_int32(self):
        self.assertEqual(fortran_kind_to_cpp_type("INT32"), "std::int32_t")

    def test_unknown_kind_raises(self):
        with self.assertRaises(KeyError):
            fortran_kind_to_cpp_type("UNKNOWN_KIND")


class TestCodeGenUtilities(unittest.TestCase):
    def test_header_guard(self):
        top, bottom = generate_header_guard("my_header.hpp")
        self.assertIn("#ifndef MY_HEADER_HPP_", top)
        self.assertIn("#define MY_HEADER_HPP_", top)
        self.assertIn("#endif // MY_HEADER_HPP_", bottom)

    def test_namespace_open(self):
        self.assertEqual(generate_namespace_open("ccpp"), "namespace ccpp {")

    def test_namespace_close(self):
        self.assertEqual(generate_namespace_close("ccpp"), "} // namespace ccpp")

    def test_includes_system(self):
        result = generate_includes(["<cstdint>"])
        self.assertEqual(result, "#include <cstdint>")

    def test_includes_project(self):
        result = generate_includes(["ccpp_scheme_base.hpp"])
        self.assertEqual(result, '#include "ccpp_scheme_base.hpp"')

    def test_extern_c(self):
        self.assertEqual(generate_extern_c_open(), 'extern "C" {')
        self.assertEqual(generate_extern_c_close(), '} // extern "C"')


class TestFortranBindCSymbol(unittest.TestCase):
    def test_simple_scheme(self):
        self.assertEqual(_fortran_bind_c_symbol("mp_thompson", "run"), "mp_thompson_run")

    def test_uppercase_lowercased(self):
        self.assertEqual(_fortran_bind_c_symbol("RRTMGP_SW", "init"), "rrtmgp_sw_init")


class TestProxyEmission(unittest.TestCase):
    def _make_scheme(self):
        args = [
            SchemeArg("air_temperature", "T", "inout", 2, "REAL64"),
            SchemeArg("ccpp_error_code", "errflg", "out", 0, "INT32"),
            SchemeArg("ccpp_error_message", "errmsg", "out", 0, "character"),
        ]
        return SchemeDescriptor("mp_thompson", "Fortran", [SchemePhase("run", args)])

    def test_header_has_class_deriving_from_scheme_base(self):
        lines = _build_proxy_header_lines(self._make_scheme(), KIND_TYPE_MAP)
        content = "\n".join(lines)
        self.assertIn("class SchemeProxy_mp_thompson : public SchemeBase {", content)
        self.assertIn("phase_run", content)

    def test_source_has_extern_c_and_fortran_call(self):
        lines = _build_proxy_source_lines(self._make_scheme(), KIND_TYPE_MAP)
        content = "\n".join(lines)
        self.assertIn('extern "C" {', content)
        self.assertIn("void mp_thompson_run(", content)
        self.assertIn("SchemeError SchemeProxy_mp_thompson::phase_run(", content)
        self.assertIn("err.code = errflg;", content)

    def test_emit_proxy_files_created(self):
        scheme = self._make_scheme()
        with tempfile.TemporaryDirectory() as tmpdir:
            h = emit_proxy_header_from_descriptor(scheme, tmpdir)
            s = emit_proxy_source_from_descriptor(scheme, tmpdir)
            self.assertTrue(os.path.exists(h))
            self.assertTrue(os.path.exists(s))
            self.assertEqual(os.path.basename(h), "scheme_proxy_mp_thompson.hpp")
            self.assertEqual(os.path.basename(s), "scheme_proxy_mp_thompson.cpp")


class TestSuiteFactoryDescriptor(unittest.TestCase):
    def test_basic_construction(self):
        d = SuiteFactoryDescriptor(suite_name="test", all_scheme_names=["a"])
        self.assertEqual(d.suite_name, "test")
        self.assertEqual(d.init_scheme, "")
        self.assertEqual(d.final_scheme, "")

    def test_collect_scheme_names_flat(self):
        items = [
            PipelineItemDescriptor(kind="scheme", scheme_name="a"),
            PipelineItemDescriptor(kind="scheme", scheme_name="b"),
        ]
        self.assertEqual(_collect_scheme_names_from_items(items), ["a", "b"])

    def test_collect_scheme_names_nested(self):
        items = [
            PipelineItemDescriptor(kind="subcycle", loop_count="2", children=[
                PipelineItemDescriptor(kind="scheme", scheme_name="inner"),
            ]),
        ]
        self.assertEqual(_collect_scheme_names_from_items(items), ["inner"])


class TestBuildSuiteFactoryLines(unittest.TestCase):
    def _make_simple_descriptor(self):
        items = [
            PipelineItemDescriptor(kind="scheme", scheme_name="scheme_a"),
            PipelineItemDescriptor(kind="scheme", scheme_name="scheme_b"),
        ]
        groups = [GroupDescriptor(name="physics", items=items)]
        return SuiteFactoryDescriptor(
            suite_name="FV3_test", groups=groups,
            all_scheme_names=["scheme_a", "scheme_b"])

    def _make_init_final_descriptor(self):
        items = [PipelineItemDescriptor(kind="scheme", scheme_name="scheme_a")]
        groups = [GroupDescriptor(name="main", items=items)]
        return SuiteFactoryDescriptor(
            suite_name="FV3_full", groups=groups,
            init_scheme="init_handler", final_scheme="final_handler",
            all_scheme_names=["scheme_a", "init_handler", "final_handler"])

    def test_unique_ptr_schemes_map(self):
        """schemes_ must use unique_ptr<SchemeBase>, not SchemeBase by value."""
        lines = _build_suite_factory_lines(self._make_simple_descriptor())
        content = "\n".join(lines)
        self.assertIn(
            "std::unordered_map<std::string, "
            "std::unique_ptr<ccpp::SchemeBase>> schemes_;", content)

    def test_make_unique_construction(self):
        """Scheme instances must be heap-allocated via make_unique."""
        lines = _build_suite_factory_lines(self._make_simple_descriptor())
        content = "\n".join(lines)
        self.assertIn("std::make_unique<SchemeProxy_scheme_a>()", content)
        self.assertIn("std::make_unique<SchemeProxy_scheme_b>()", content)

    def test_get_pointer_extraction(self):
        """SchemeElement construction uses .get() on the unique_ptr."""
        lines = _build_suite_factory_lines(self._make_simple_descriptor())
        content = "\n".join(lines)
        self.assertIn('schemes_["scheme_a"].get()', content)
        self.assertIn('schemes_["scheme_b"].get()', content)

    def test_init_final_scheme_uses_get(self):
        """Suite init/final scheme pointers use .get() not & on map entry."""
        lines = _build_suite_factory_lines(self._make_init_final_descriptor())
        content = "\n".join(lines)
        self.assertIn('schemes_["init_handler"].get()', content)
        self.assertIn('schemes_["final_handler"].get()', content)

    def test_self_registering_static_initializer(self):
        """Factory emits RegisterSuiteFactory_ static initializer."""
        lines = _build_suite_factory_lines(self._make_simple_descriptor())
        content = "\n".join(lines)
        self.assertIn('#include "ccpp_framework_orchestrator.hpp"', content)
        self.assertIn("RegisterSuiteFactory_FV3_test", content)
        self.assertIn("register_suite_factory(", content)
        self.assertIn('"FV3_test"', content)
        self.assertIn("&ccpp::generated::create_suite_FV3_test", content)

    def test_includes_proxy_headers(self):
        lines = _build_suite_factory_lines(self._make_simple_descriptor())
        content = "\n".join(lines)
        self.assertIn('#include "scheme_proxy_scheme_a.hpp"', content)
        self.assertIn('#include "scheme_proxy_scheme_b.hpp"', content)

    def test_group_constructed(self):
        lines = _build_suite_factory_lines(self._make_simple_descriptor())
        content = "\n".join(lines)
        self.assertIn('"physics"', content)
        self.assertIn("make_unique<ccpp::Group>", content)

    def test_subcycle_element_created(self):
        items = [
            PipelineItemDescriptor(kind="subcycle", loop_count="3", children=[
                PipelineItemDescriptor(kind="scheme", scheme_name="rad"),
            ]),
        ]
        groups = [GroupDescriptor(name="radiation", items=items)]
        d = SuiteFactoryDescriptor(suite_name="FV3_rad", groups=groups,
                                   all_scheme_names=["rad"])
        lines = _build_suite_factory_lines(d)
        content = "\n".join(lines)
        self.assertIn("make_unique<ccpp::SubcycleElement>", content)
        self.assertIn("3", content)

    def test_file_written(self):
        d = self._make_simple_descriptor()
        with tempfile.TemporaryDirectory() as tmpdir:
            path = build_suite_factory_from_descriptor(d, tmpdir)
            self.assertTrue(os.path.exists(path))
            self.assertEqual(os.path.basename(path), "suite_factory_fv3_test.cpp")


class TestPhaseAwareSuiteFactory(unittest.TestCase):
    """Tests for the phase-aware fix: schemes with only non-run phases
    must still appear in the emitted pipeline."""

    def test_non_run_only_scheme_included_in_pipeline(self):
        """A scheme that only implements init/timestep_init/final but NOT run
        (like GFS_time_vary_pre in SCM_GFS_v16's time_vary group) must still
        appear as a SchemeElement in the group's pipeline."""
        from generator.suite_resolver import ResolvedCall, ResolvedGroup, SuiteResolution
        from generator.cpp_emitter import _build_suite_factory_descriptor_from_resolution

        run_only = ResolvedCall(scheme_name="run_scheme", phase="run", args=[])
        init_only = ResolvedCall(scheme_name="init_only_scheme", phase="init", args=[])
        group = ResolvedGroup(
            group_name="time_vary",
            phase_calls={"run": [run_only], "init": [init_only]})
        resolution = SuiteResolution(suite_name="test", groups=[group])
        descriptor = _build_suite_factory_descriptor_from_resolution(resolution)

        # Both schemes must be in all_scheme_names
        self.assertIn("run_scheme", descriptor.all_scheme_names)
        self.assertIn("init_only_scheme", descriptor.all_scheme_names)
        # Both must appear in the group's pipeline items
        group_items = descriptor.groups[0].items
        scheme_names = _collect_scheme_names_from_items(group_items)
        self.assertIn("run_scheme", scheme_names)
        self.assertIn("init_only_scheme", scheme_names)

    def test_group_with_zero_run_schemes_still_has_pipeline(self):
        """A group where NO scheme implements 'run' at all must still produce
        a non-empty pipeline from non-run phase schemes."""
        from generator.suite_resolver import ResolvedCall, ResolvedGroup, SuiteResolution
        from generator.cpp_emitter import _build_suite_factory_descriptor_from_resolution

        init_a = ResolvedCall(scheme_name="scheme_a", phase="init", args=[])
        init_b = ResolvedCall(scheme_name="scheme_b", phase="init", args=[])
        group = ResolvedGroup(
            group_name="setup_only",
            phase_calls={"init": [init_a, init_b]})
        resolution = SuiteResolution(suite_name="setup_suite", groups=[group])
        descriptor = _build_suite_factory_descriptor_from_resolution(resolution)

        group_items = descriptor.groups[0].items
        scheme_names = _collect_scheme_names_from_items(group_items)
        self.assertEqual(scheme_names, ["scheme_a", "scheme_b"])

    def test_emitter_emit_suite_factory_with_non_run_only_schemes(self):
        """End-to-end test: CppEmitter.emit_suite_factory includes non-run
        schemes in the generated factory file."""
        from generator.suite_resolver import ResolvedCall, ResolvedGroup, SuiteResolution

        run_call = ResolvedCall(scheme_name="main_scheme", phase="run", args=[])
        init_call = ResolvedCall(scheme_name="setup_scheme", phase="init", args=[])
        group = ResolvedGroup(
            group_name="physics",
            phase_calls={"run": [run_call], "init": [init_call]})
        resolution = SuiteResolution(suite_name="mixed_suite", groups=[group])
        emitter = CppEmitter(suite_resolution=resolution)
        with tempfile.TemporaryDirectory() as tmpdir:
            path = emitter.emit_suite_factory(tmpdir)
            with open(path) as f:
                content = f.read()
            self.assertIn("SchemeProxy_main_scheme", content)
            self.assertIn("SchemeProxy_setup_scheme", content)
            self.assertIn('schemes_["setup_scheme"].get()', content)


class TestHostAccessorEmission(unittest.TestCase):
    def test_accessor_function_name(self):
        self.assertEqual(_accessor_function_name("air_temperature"), "get_air_temperature")

    def test_mdspan_return_type(self):
        self.assertEqual(
            _mdspan_return_type("double", 2),
            "mdspan_left<double, std::dextents<int, 2>>")

    def test_build_host_accessor_lines(self):
        vars_list = [HostVarAccessor(
            standard_name="air_temperature", local_name="T",
            kind="REAL64", rank=2,
            dimensions=["horizontal_dimension", "vertical_layer_dimension"],
            intent="inout")]
        lines = _build_host_accessor_lines(vars_list, KIND_TYPE_MAP)
        content = "\n".join(lines)
        self.assertIn("#pragma once", content)
        self.assertIn("get_air_temperature", content)
        self.assertIn("make_mdspan_left", content)
        self.assertIn("void* data", content)

    def test_scalar_vars_skipped(self):
        vars_list = [HostVarAccessor(
            standard_name="scalar_val", local_name="x",
            kind="REAL64", rank=0, dimensions=[], intent="in")]
        lines = _build_host_accessor_lines(vars_list, KIND_TYPE_MAP)
        content = "\n".join(lines)
        self.assertNotIn("get_scalar_val", content)

    def test_emit_host_accessor_creates_file(self):
        vars_list = [HostVarAccessor(
            standard_name="temperature", local_name="T",
            kind="REAL64", rank=2,
            dimensions=["horizontal_dimension", "vertical_layer_dimension"],
            intent="inout")]
        with tempfile.TemporaryDirectory() as tmpdir:
            path = emit_host_accessor_from_descriptors(vars_list, tmpdir)
            self.assertTrue(os.path.exists(path))
            self.assertEqual(os.path.basename(path), "host_accessor.hpp")


class TestCppEmitterClass(unittest.TestCase):
    def test_construction_with_default_kind_map(self):
        emitter = CppEmitter(suite_resolution=None)
        self.assertEqual(emitter.kind_map["REAL64"], "double")

    def test_construction_with_custom_kind_map(self):
        emitter = CppEmitter(suite_resolution=None, kind_map={"MY": "my_t"})
        self.assertEqual(emitter.kind_map["MY"], "my_t")
        self.assertNotIn("REAL64", emitter.kind_map)

    def test_emit_host_accessor_creates_file(self):
        from unittest.mock import MagicMock
        mock_arg = MagicMock()
        mock_arg.standard_name = "air_temperature"
        mock_arg.scheme_local_name = "T"
        mock_arg.intent = "inout"
        mock_arg.scheme_dimensions = ["horizontal_dimension", "vertical_layer_dimension"]
        mock_arg.kind_host = "REAL64"
        mock_arg.kind_scheme = "REAL64"
        emitter = CppEmitter(suite_resolution=None)
        with tempfile.TemporaryDirectory() as tmpdir:
            path = emitter.emit_host_accessor([mock_arg], tmpdir)
            self.assertTrue(os.path.exists(path))
            with open(path) as f:
                content = f.read()
            self.assertIn("get_air_temperature", content)


if __name__ == "__main__":
    unittest.main(verbosity=2)
