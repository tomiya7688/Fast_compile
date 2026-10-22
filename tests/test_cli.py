"""Golden and behavioral tests for the bootstrap CLI; standard library only."""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXECUTABLE = ROOT / "build" / "fastc"
GOLDEN = ROOT / "tests" / "golden"


class BootstrapCliTests(unittest.TestCase):
    def run_fastc(self, *args):
        return subprocess.run(
            [str(EXECUTABLE), *args], cwd=ROOT,
            capture_output=True, text=True, check=False,
        )

    def test_help_golden(self):
        result = self.run_fastc("--help")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, (GOLDEN / "help.stdout").read_text())
        self.assertEqual(result.stderr, "")

    def test_version_golden(self):
        result = self.run_fastc("--version")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, (GOLDEN / "version.stdout").read_text())
        self.assertEqual(result.stderr, "")

    def test_minimal_source_golden(self):
        result = self.run_fastc("--check-input", "tests/fixtures/minimal.fscm")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, (GOLDEN / "input.stdout").read_text())
        self.assertEqual(result.stderr, "")

    def test_empty_source(self):
        result = self.run_fastc("--check-input", "tests/fixtures/empty.fscm")
        self.assertEqual(result.returncode, 0)
        self.assertIn("read 0 bytes", result.stdout)

    def test_missing_file_is_error(self):
        result = self.run_fastc("--check-input", "tests/fixtures/missing.fscm")
        self.assertEqual(result.returncode, 3)
        self.assertIn("cannot open input file", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_wrong_extension_is_error(self):
        result = self.run_fastc("--check-input", "README.md")
        self.assertEqual(result.returncode, 3)
        self.assertIn("expected a .fscm file", result.stderr)

    def test_wrong_command_is_not_reported_as_compilation(self):
        for args in [(), ("tests/fixtures/minimal.fscm",), ("--check-input",),
                     ("--check-input", "x.fscm", "extra")]:
            with self.subTest(args=args):
                result = self.run_fastc(*args)
                self.assertEqual(result.returncode, 2)
                self.assertIn("unsupported or incomplete command", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_multiple_read_chunks(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "large.fscm"
            path.write_bytes(b"x" * 20001)
            result = self.run_fastc("--check-input", str(path))
            self.assertEqual(result.returncode, 0)
            self.assertIn("read 20001 bytes", result.stdout)
            self.assertEqual(result.stderr, "")

    def test_directory_cannot_be_loaded(self):
        with tempfile.TemporaryDirectory(suffix=".fscm") as directory:
            result = self.run_fastc("--check-input", directory)
            self.assertEqual(result.returncode, 3)
            self.assertIn("cannot read input file", result.stderr)


if __name__ == "__main__":
    unittest.main()
