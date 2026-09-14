import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class TtrpgCliTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ttrpg cli ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.schema = self.write("root.ttrpg", "class TExample { int Value = 1; }\n")
        self.header = self.root / "generated include" / "root.h"
        self.source = self.root / "generated src" / "root.cpp"
        self.depfile = self.root / "dependencies" / "root.d"

    def write(self, relative, contents):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
        return path

    def command(self, arguments, success=True, cwd=None):
        result = subprocess.run(
            [str(argument) for argument in arguments],
            cwd=cwd or self.root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=60,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def generate(self, success=True):
        return self.command(
            [CODEGEN, "--schema", self.schema, "--out-h", self.header,
             "--out-cpp", self.source, "--depfile", self.depfile],
            success=success,
        )

    def test_generates_both_outputs_and_depfile(self):
        self.generate()
        self.assertIn("class TExample", self.header.read_text())
        self.assertIn("TExample::FromJson", self.source.read_text())
        self.assertIn("root.ttrpg", self.depfile.read_text())
        self.assertFalse(list(self.root.rglob("*.ttrpg-*")))

    def test_check_has_no_filesystem_side_effects(self):
        before = set(self.root.rglob("*"))
        self.command([CODEGEN, "--schema", self.schema, "--check"])
        self.assertEqual(set(self.root.rglob("*")), before)

    def test_check_rejects_invalid_imported_module(self):
        self.write("invalid.ttrpg", "class TInvalid { int Value = Missing + 1; }\n")
        self.schema.write_text('import "invalid.ttrpg";\nclass TExample { int Value; }\n')
        result = self.command([CODEGEN, "--schema", self.schema, "--check"], success=False)
        self.assertIn("Missing", result.stderr)
        self.assertFalse(self.header.exists())

    def test_check_rejects_invalid_imported_grants(self):
        for invalid in (
            "feature Training { grant Missing; }",
            "feature A { grant B; } feature B { grant A; }",
            "class TProvider { int Value; grant @self.Value; }",
        ):
            with self.subTest(invalid=invalid):
                self.write("invalid.ttrpg", invalid)
                self.schema.write_text('import "invalid.ttrpg"; class TExample { int Value; }')
                self.command([CODEGEN, "--schema", self.schema, "--check"], success=False)
                self.assertFalse(self.header.exists())

    def test_imported_activities_use_canonical_export_names(self):
        self.write("actor.ttrpg", "GameEntity TActor { int Value = 1; }")
        library = self.write("library.ttrpg", 'import "actor.ttrpg"; activity int Increment for TActor(int Value) { return Value + 1; }')
        (self.root / "alias.ttrpg").symlink_to(library)
        self.schema.write_text('import "alias.ttrpg"; action Use for TActor { let Value = Increment(1); @self.Value = Value; }')
        self.command([CODEGEN, "--schema", self.schema, "--check"])
        self.generate()
        self.assertIn("::RunLibraryIncrement(Context", self.source.read_text())
        self.assertIn("library.h", self.header.read_text())

    def test_source_choice_imports_and_escape_diagnostics(self):
        self.write("items.ttrpg", "class TItem { int Size = 2; query TItem Identity() { return @self; } } GameEntity TActor { collection<TItem> Items; }")
        self.schema.write_text('import "items.ttrpg"; action Use for TActor { choose TItem Item from @self.Items; let Value = Item.Size; }')
        self.command([CODEGEN, "--schema", self.schema, "--check"])
        self.generate()
        self.assertIn("SelectActionSource<::TItem>", self.source.read_text())
        before = [path.read_bytes() for path in (self.header, self.source, self.depfile)]
        self.schema.write_text('import "items.ttrpg"; action Use for TActor { choose TItem Item from @self.Items; let Copy = Item.Identity(); }')
        result = self.generate(success=False)
        self.assertIn("cannot be materialized", result.stderr)
        self.assertEqual([path.read_bytes() for path in (self.header, self.source, self.depfile)], before)

    def test_recursive_activities_preserve_generated_outputs(self):
        self.generate()
        before = [path.read_bytes() for path in (self.header, self.source, self.depfile)]
        self.schema.write_text("GameEntity TActor {} activity void First for TActor() { Second(); } activity void Second for TActor() { First(); }")
        result = self.generate(success=False)
        self.assertIn("recursive activity calls", result.stderr)
        self.assertEqual([path.read_bytes() for path in (self.header, self.source, self.depfile)], before)

    def test_parameterized_factories_import_types_and_preserve_outputs_on_errors(self):
        self.write("items.ttrpg", "class TItem { int Amount = 2; } feature First; feature Second; GameEntity TActor { collection<TItem> Items; collection<Feature> Features; }")
        self.schema.write_text('import "items.ttrpg"; TActor Fighter(collection<TItem> Equipment = [TItem {}], choice Feature Feat from [First, Second] = First) { Items = Equipment; Features = [Feat]; }')
        self.command([CODEGEN, "--schema", self.schema, "--check"])
        self.generate()
        self.assertIn("TFighterOptions", self.header.read_text())
        self.assertIn("items.h", self.header.read_text())
        self.assertIn("::TActor::ResolveInitialValues", self.source.read_text())
        self.assertIn("DescribeFighter", self.source.read_text())
        before = [path.read_bytes() for path in (self.header, self.source, self.depfile)]
        self.schema.write_text('import "items.ttrpg"; TActor Fighter(choice Feature Feat from [First] = First, Feature First = Second) {}')
        result = self.generate(success=False)
        self.assertIn("must precede", result.stderr)
        self.assertEqual([path.read_bytes() for path in (self.header, self.source, self.depfile)], before)

    def test_imported_parameterized_values_and_generated_name_collisions(self):
        self.write("items.ttrpg", "class TItem { int Amount = 2; } TItem Configured(int Amount = 3) { Amount = Amount; }")
        self.schema.write_text('import "items.ttrpg"; GameEntity TActor { collection<TItem> Items; } TActor Fighter(collection<TItem> Equipment = [Configured]) { Items = Equipment; }')
        self.command([CODEGEN, "--schema", self.schema, "--check"])
        self.generate()
        self.assertIn("::CreateConfigured()", self.source.read_text())
        self.schema.write_text('import "items.ttrpg"; class TConfiguredOptions {}')
        result = self.command([CODEGEN, "--schema", self.schema, "--check"], success=False)
        self.assertIn("constructor generated name collision", result.stderr)

    def test_invalid_grants_preserve_generated_outputs(self):
        self.generate()
        paths = [self.header, self.source, self.depfile]
        before = [(path.read_bytes(), path.stat().st_mtime_ns) for path in paths]
        self.schema.write_text("feature Training { grant Missing; }")
        self.generate(success=False)
        self.assertEqual([(path.read_bytes(), path.stat().st_mtime_ns) for path in paths], before)

    def test_invalid_implementation_does_not_leave_partial_header(self):
        self.schema.write_text("class TInvalid { int Value = Missing + 1; }\n")
        result = self.generate(success=False)
        self.assertIn("Missing", result.stderr)
        self.assertFalse(self.header.parent.exists())
        self.assertFalse(self.source.parent.exists())
        self.assertFalse(self.depfile.parent.exists())

    def test_validation_error_preserves_existing_outputs(self):
        self.generate()
        paths = [self.header, self.source, self.depfile]
        before = [(path.read_bytes(), path.stat().st_mtime_ns) for path in paths]
        self.schema.write_text("class TInvalid { int Value = Missing + 1; }\n")
        self.generate(success=False)
        after = [(path.read_bytes(), path.stat().st_mtime_ns) for path in paths]
        self.assertEqual(after, before)

    def test_unwritable_output_preserves_other_outputs(self):
        self.generate()
        header = self.header.read_bytes()
        depfile = self.depfile.read_bytes()
        self.source.unlink()
        self.source.mkdir()
        self.generate(success=False)
        self.assertEqual(self.header.read_bytes(), header)
        self.assertEqual(self.depfile.read_bytes(), depfile)
        self.assertTrue(self.source.is_dir())
        self.assertFalse(list(self.root.rglob("*.ttrpg-*")))

    def test_relative_outputs_need_no_parent_directory(self):
        self.command([CODEGEN, "--schema", "root.ttrpg", "--out-h", "root.h",
                      "--out-cpp", "root.cpp", "--depfile", "root.d"])
        self.assertTrue((self.root / "root.h").is_file())
        self.assertTrue((self.root / "root.cpp").is_file())
        self.assertTrue((self.root / "root.d").is_file())

    def test_rejects_aliasing_outputs_and_schema_overwrite(self):
        arguments = [CODEGEN, "--schema", self.schema, "--out-h", "same.h",
                     "--out-cpp", self.root / "same.h"]
        result = self.command(arguments, success=False)
        self.assertIn("distinct", result.stderr)
        before = self.schema.read_bytes()
        self.command([CODEGEN, "--schema", self.schema, "--out-h", self.schema,
                      "--out-cpp", "root.cpp"], success=False)
        self.assertEqual(self.schema.read_bytes(), before)
        self.assertFalse((self.root / "same.h").exists())

    def test_check_rejects_output_arguments(self):
        result = self.command([CODEGEN, "--schema", self.schema, "--check",
                               "--out-h", self.header], success=False)
        self.assertIn("--check cannot", result.stderr)
        self.assertFalse(self.header.exists())

    def test_hardlink_to_schema_is_rejected_without_changing_input(self):
        self.header = self.root / "schema_alias.h"
        self.header.hardlink_to(self.schema)
        before = self.schema.read_bytes()
        result = self.generate(success=False)
        self.assertIn("overwrite a schema", result.stderr)
        self.assertEqual(self.schema.read_bytes(), before)
        self.assertEqual(self.header.read_bytes(), before)
        self.assertFalse(self.source.exists())

    def test_hardlinked_output_paths_are_rejected(self):
        self.header = self.write("root.h", "previous output")
        self.source = self.root / "root.cpp"
        self.source.hardlink_to(self.header)
        result = self.generate(success=False)
        self.assertIn("distinct", result.stderr)
        self.assertEqual(self.header.read_text(), "previous output")
        self.assertEqual(self.source.read_text(), "previous output")

    def test_symlink_to_imported_schema_is_rejected(self):
        imported = self.write("imported.ttrpg", "enum EImported { Value, }\n")
        self.schema.write_text('import "imported.ttrpg";\n')
        self.header = self.root / "input_alias.h"
        self.header.symlink_to(imported)
        before = imported.read_bytes()
        result = self.generate(success=False)
        self.assertIn("overwrite a schema", result.stderr)
        self.assertEqual(imported.read_bytes(), before)
        self.assertTrue(self.header.is_symlink())
        self.assertFalse(self.source.exists())

    def test_primary_schema_symlink_uses_canonical_module_identity(self):
        alias = self.root / "source_alias.ttrpg"
        alias.symlink_to(self.schema)
        self.schema = alias
        self.generate()
        self.assertIn("class TExample", self.header.read_text())
        self.assertIn("root.ttrpg", self.depfile.read_text())

    def test_imported_symlink_uses_canonical_ability_registration(self):
        imported = self.write("definitions.ttrpg", "action Strike;\nfeature Training { grant Strike; }\n")
        alias = self.root / "alias.ttrpg"
        alias.symlink_to(imported)
        self.schema.write_text('import "alias.ttrpg";\nGameEntity TExample { grant Training; }\n')
        self.generate()
        self.assertIn("definitions.h", self.header.read_text())
        self.assertNotIn("alias.h", self.header.read_text())
        self.assertIn("RegisterDefinitionsAbilities(catalog)", self.source.read_text())
        self.assertNotIn("RegisterAliasAbilities", self.source.read_text())
        self.assertIn("definitions.Training", self.header.read_text())

    def test_nested_output_paths_are_rejected_before_creating_directories(self):
        self.header = self.root / "new_directory" / "root.h"
        self.source = self.header / "root.cpp"
        result = self.generate(success=False)
        self.assertIn("contain another output", result.stderr)
        self.assertFalse(self.header.parent.exists())

    def test_depfile_has_unique_transitive_paths_with_escaping(self):
        self.write("nested rules/leaf#$.ttrpg", "enum ELeaf { First, }\n")
        self.write("left.ttrpg", 'import "nested rules/leaf#$.ttrpg";\n')
        self.write("right.ttrpg", 'import "nested rules/leaf#$.ttrpg";\n')
        self.schema.write_text('import "left.ttrpg";\nimport "right.ttrpg";\n')
        self.header = self.root / "output #$.h"
        self.generate()
        dependencies = self.depfile.read_text()
        self.assertIn("output\\ \\#$$.h", dependencies)
        self.assertIn("nested\\ rules/leaf\\#$$.ttrpg", dependencies)
        self.assertEqual(dependencies.count("leaf\\#$$.ttrpg"), 1)
        for name in ["root.ttrpg", "left.ttrpg", "right.ttrpg"]:
            self.assertEqual(dependencies.count(name), 1)

    def test_distinct_modules_with_same_stem_are_rejected(self):
        self.write("first/shared.ttrpg", "enum EFirst { First, }\n")
        self.write("second/shared.ttrpg", "enum ESecond { Second, }\n")
        self.schema.write_text(
            'import "first/shared.ttrpg";\nimport "second/shared.ttrpg";\n')
        result = self.command([CODEGEN, "--schema", self.schema, "--check"], success=False)
        self.assertIn("module stem 'shared'", result.stderr)
        self.assertIn("first/shared.ttrpg", result.stderr)
        self.assertIn("second/shared.ttrpg", result.stderr)

    def test_cmake_rebuilds_after_transitive_import_changes(self):
        leaf = self.write("leaf.ttrpg", "enum ELeaf { First, }\n")
        self.write("middle.ttrpg", 'import "leaf.ttrpg";\n')
        self.schema.write_text('import "middle.ttrpg";\nclass TExample { int Value; }\n')
        self.write("CMakeLists.txt", f"""cmake_minimum_required(VERSION 3.20)
project(TtrpgCliIntegration NONE)
add_executable(ttrpg_codegen IMPORTED GLOBAL)
set_target_properties(ttrpg_codegen PROPERTIES IMPORTED_LOCATION [=[{CODEGEN}]=])
include([=[{HELPER}]=])
ttrpg_generate(SCHEMA root.ttrpg HEADER generated/root.h SOURCE generated/root.cpp)
add_custom_target(generate ALL DEPENDS
    "${{CMAKE_CURRENT_BINARY_DIR}}/generated/root.h"
    "${{CMAKE_CURRENT_BINARY_DIR}}/generated/root.cpp")
""")
        build = self.root / "build dir"
        self.command([CMAKE, "-S", self.root, "-B", build, "-G", "Unix Makefiles"])
        self.command([CMAKE, "--build", build])
        header = build / "generated/root.h"
        source = build / "generated/root.cpp"
        before = [path.stat().st_mtime_ns for path in [header, source]]
        self.command([CMAKE, "--build", build])
        self.assertEqual([path.stat().st_mtime_ns for path in [header, source]], before)
        leaf.write_text("enum ELeaf { First, Second, }\n")
        self.command([CMAKE, "--build", build])
        for path, previous_time in zip([header, source], before):
            self.assertGreater(path.stat().st_mtime_ns, previous_time)

    def test_cmake_sets_generated_properties_in_consuming_target_directory(self):
        self.write("CMakeLists.txt", f"""cmake_minimum_required(VERSION 3.27)
project(TtrpgCliTargetProperties NONE)
add_executable(ttrpg_codegen IMPORTED GLOBAL)
set_target_properties(ttrpg_codegen PROPERTIES IMPORTED_LOCATION [=[{CODEGEN}]=])
include([=[{HELPER}]=])
add_custom_target(schema_consumer)
add_subdirectory(codegen)
get_source_file_property(SKIP_GENERATED_LINT
    "${{CMAKE_CURRENT_BINARY_DIR}}/codegen/generated/root.cpp" SKIP_LINTING)
if(NOT SKIP_GENERATED_LINT)
    message(FATAL_ERROR "Generated source does not skip linting in target directory")
endif()
""")
        self.write("codegen/CMakeLists.txt", """ttrpg_generate(
    TARGET schema_consumer
    SCHEMA ../root.ttrpg
    HEADER generated/root.h
    SOURCE generated/root.cpp
)
""")
        self.command([CMAKE, "-S", self.root, "-B", self.root / "build dir",
                      "-G", "Unix Makefiles"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--codegen", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--helper", type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    CODEGEN = arguments.codegen.resolve()
    CMAKE = arguments.cmake.resolve()
    HELPER = arguments.helper.resolve()
    unittest.main(argv=[__file__, *remaining])
