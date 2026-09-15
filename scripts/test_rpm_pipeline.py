#!/usr/bin/env python3
"""
test_rpm_pipeline.py — Integration test harness for Atari TT030 SpareMiNT RPM DevOps pipeline
"""

import os
import sys
import subprocess
import unittest

class TestRPMPipeline(unittest.TestCase):
    def test_spec_template_exists(self):
        spec_path = "staging/rpm/templates/sparemint-package.spec"
        self.assertTrue(os.path.exists(spec_path), f"Spec template missing: {spec_path}")
        with open(spec_path, "r") as f:
            content = f.read()
        self.assertIn("BuildArch:      m68kmint", content)
        self.assertIn("m68030", content)

    def test_rpm_builder_execution(self):
        builder_path = "scripts/sam_tt030_rpm_builder.py"
        self.assertTrue(os.path.exists(builder_path), f"Builder script missing: {builder_path}")
        
        # Test build dry run
        dummy_src = "src/x25519bench/sam_scp_tt.c"
        cmd = [sys.executable, builder_path, "--build", dummy_src, "--name", "test_pkg", "--version", "1.0.0"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"RPM builder failed: {res.stderr}")
        self.assertTrue(os.path.exists("build/rpms/test_pkg-1.0.0-1.m68kmint.rpm"))

    def test_rca_document_exists(self):
        rca_path = "docs/rca/2026-09-15-sparemint-rpm-db-empty.md"
        self.assertTrue(os.path.exists(rca_path), f"5-Why RCA document missing: {rca_path}")
        with open(rca_path, "r") as f:
            content = f.read()
        self.assertIn("RCA — SpareMiNT RPM Database Empty on Atari TT030", content)
        self.assertIn("Five Whys", content)

if __name__ == "__main__":
    unittest.main()
