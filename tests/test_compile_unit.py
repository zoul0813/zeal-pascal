"""Source preparation and failure-safe publication for the emulator unit compiler."""
import importlib.util
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('compile_unit', Path(__file__).resolve().parents[1] / 'tools/compile-unit.py')
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)


class CompileUnit(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def test_nested_includes_preserve_literals_switches_and_license(self):
        source = self.write('Root.pas', "{ SPDX-License-Identifier: MIT }\nunit Sample;\n{$I types.inc}\nimplementation\nend.\n")
        self.write('types.inc', "interface\n(*$I nested/values.inc*)\n{$I-}\n")
        self.write('nested/values.inc', "{ ordinary comment }\nconst S = '{$I nonexistent.inc}';\n")
        text, code = compiler.prepare_source(source, compact=True)
        self.assertIn('SPDX-License-Identifier: MIT', text)
        self.assertIn("S = '{$I nonexistent.inc}'", text)
        self.assertIn('{$I-}', text)
        self.assertNotIn('ordinary comment', text)
        self.assertNotIn('types.inc', text)
        self.assertRegex(code, r'\s*unit Sample;')

    def test_cycle_and_missing_include_fail_before_compilation(self):
        source = self.write('Root.pas', '{$I loop.inc}')
        self.write('loop.inc', '{$I Root.pas}')
        with self.assertRaisesRegex(ValueError, 'Include cycle'):
            compiler.prepare_source(source)
        source.write_text('{$I missing.inc}')
        with self.assertRaises(FileNotFoundError):
            compiler.prepare_source(source)

    def test_compaction_does_not_merge_tokens_or_change_quoted_apostrophes(self):
        source = self.write('Root.pas', "unit{comment}Sample;\nconst S='it''s {literal}';\n")
        text, _ = compiler.prepare_source(source, compact=True)
        self.assertEqual(text, "unit Sample;\nconst S='it''s {literal}';\n")

    def test_failed_publish_preserves_existing_output_and_removes_temporary(self):
        output = self.write('SAMPLE.ZPU', 'previous unit')
        with patch.object(compiler.os, 'replace', side_effect=OSError('disk error')):
            with self.assertRaises(OSError):
                compiler.publish([(output, b'new unit')])
        self.assertEqual(output.read_bytes(), b'previous unit')
        self.assertEqual(list(self.root.iterdir()), [output])

    def test_rejects_nonunit_and_source_overwrite_without_launching_emulator(self):
        output = self.write('SAMPLE.ZPU', 'previous unit')
        source = self.write('Root.pas', 'begin end.')
        with self.assertRaisesRegex(ValueError, 'unit declaration'):
            compiler.compile_unit(source, output)
        self.assertEqual(output.read_bytes(), b'previous unit')
        source.write_text('unit Sample; interface implementation end.')
        with self.assertRaisesRegex(ValueError, 'overwrite the input source'):
            compiler.compile_unit(source, output, source_output=source)

    def test_failed_second_publication_restores_both_existing_outputs(self):
        unit = self.write('SAMPLE.ZPU', 'old unit')
        source = self.write('SAMPLE.PAS', 'old source')
        replace = compiler.os.replace
        calls = []

        def fail_second(old, new):
            calls.append(new)
            if len(calls) == 2:
                raise OSError('source publication failed')
            return replace(old, new)

        with patch.object(compiler.os, 'replace', side_effect=fail_second):
            with self.assertRaises(OSError):
                compiler.publish([(unit, b'new unit'), (source, b'new source')])
        self.assertEqual(unit.read_bytes(), b'old unit')
        self.assertEqual(source.read_bytes(), b'old source')
        self.assertEqual(set(self.root.iterdir()), {unit, source})

    def test_failed_second_publication_removes_new_first_output(self):
        unit = self.root / 'SAMPLE.ZPU'
        source = self.write('SAMPLE.PAS', 'old source')
        replace = compiler.os.replace

        def fail_source(old, new):
            if new == source:
                raise OSError('source publication failed')
            return replace(old, new)

        with patch.object(compiler.os, 'replace', side_effect=fail_source):
            with self.assertRaises(OSError):
                compiler.publish([(unit, b'new unit'), (source, b'new source')])
        self.assertFalse(unit.exists())
        self.assertEqual(source.read_bytes(), b'old source')

    def test_interrupted_second_publication_restores_original_unit(self):
        unit = self.write('SAMPLE.ZPU', 'old unit')
        source = self.write('SAMPLE.PAS', 'old source')
        replace = compiler.os.replace
        calls = 0

        def interrupt_second(old, new):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise KeyboardInterrupt()
            return replace(old, new)

        with patch.object(compiler.os, 'replace', side_effect=interrupt_second):
            with self.assertRaises(KeyboardInterrupt):
                compiler.publish([(unit, b'new unit'), (source, b'new source')])
        self.assertEqual(unit.read_bytes(), b'old unit')
        self.assertEqual(source.read_bytes(), b'old source')

    def test_failed_rollback_retains_original_for_recovery(self):
        unit = self.write('SAMPLE.ZPU', 'old unit')
        source = self.write('SAMPLE.PAS', 'old source')
        replace = compiler.os.replace
        count = 0

        def fail_after_first(old, new):
            nonlocal count
            count += 1
            if count > 1:
                raise OSError('storage failed')
            return replace(old, new)

        with patch.object(compiler.os, 'replace', side_effect=fail_after_first):
            with self.assertRaisesRegex(OSError, 'original retained at'):
                compiler.publish([(unit, b'new unit'), (source, b'new source')])
        recovery = [p for p in self.root.iterdir() if p not in (unit, source)]
        self.assertEqual(len(recovery), 1)
        self.assertEqual(recovery[0].read_bytes(), b'old unit')
        self.assertEqual(source.read_bytes(), b'old source')

    def test_native_include_spelling_and_default_extension(self):
        source = self.write('Root.pas', 'unit Sample; {$Iexports} implementation end.')
        self.write('exports.PAS', 'interface')
        prepared, _ = compiler.prepare_source(source)
        self.assertIn('interface', prepared)
        self.assertNotIn('$Iexports', prepared)

    def test_rejects_wrong_output_filename_before_emulator_launch(self):
        source = self.write('Root.pas', 'unit Sample; interface implementation end.')
        with self.assertRaisesRegex(ValueError, 'Output filename must be SAMPLE.ZPU'):
            compiler.compile_unit(source, self.root / 'OTHER.ZPU')

    def test_emulator_failures_preserve_output_and_dependency_precedence(self):
        source = self.write('source/Root.pas', 'unit Sample; interface implementation end.')
        output = self.write('out/SAMPLE.ZPU', 'previous output')
        self.write('source/dep.zpu', 'local dependency')
        self.write('extra/DEP.ZPU', 'explicit dependency')
        self.write('extra/OTHER.ZPU', 'explicit other')
        self.write('build/DEP.ZPU', 'installed dependency')
        self.write('build/OTHER.ZPU', 'installed other')
        self.write('build/SAMPLE.ZPU', 'stale self')
        for name in ('zpascal.bin', 'zpascal.zpl', 'zpascal.map', 'SYSFILE.ZPU'):
            self.write('build/' + name, 'prerequisite')
        emulator = self.write('emulator', 'prerequisite')
        rom = self.write('rom', 'prerequisite')
        for condition in ('input-error', 'missing', 'scratch', 'invalid'):
            with self.subTest(condition=condition):
                native = SimpleNamespace(EMULATOR=emulator, ROM=rom,
                                         new_run=lambda parent: self.root / condition,
                                         symbols=lambda name: {'_key_read': 1, '_work': 2},
                                         type_text=lambda value, **kw: [], program_launch=lambda: [],
                                         ImageStop=lambda *args: None)

                def prepare(folder, *, fixtures, **kwargs):
                    self.assertNotIn('SAMPLE.ZPU', fixtures)
                    self.assertEqual(fixtures['DEP.ZPU'], b'local dependency')
                    self.assertEqual(fixtures['OTHER.ZPU'], b'explicit other')
                    hostfs = folder / 'hostfs'
                    hostfs.mkdir(parents=True)
                    if condition == 'input-error':
                        (folder / 'input-error.json').write_text('{"message":"timeout"}')
                    elif condition != 'missing':
                        (hostfs / 'SAMPLE.ZPU').write_bytes(b'invalid candidate')
                        if condition == 'scratch':
                            (hostfs / 'SAMPLE.$PU').write_bytes(b'unfinished')
                    return [], {}

                native.prepare = prepare
                native.execute = lambda *args, **kwargs: None
                with patch.object(compiler, 'ROOT', self.root), patch.object(compiler, 'native', native):
                    with self.assertRaises(ValueError):
                        compiler.compile_unit(source, output, unit_paths=[self.root / 'extra'])
                self.assertEqual(output.read_bytes(), b'previous output')

    def test_rejects_truncated_or_corrupt_units(self):
        for data in (b'', b'ZPU1', bytes(60)):
            with self.subTest(data=data):
                with self.assertRaisesRegex(ValueError, 'header'):
                    compiler.validate_unit(data, 'SAMPLE')


if __name__ == '__main__':
    unittest.main()
