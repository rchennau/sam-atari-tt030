import os
import sys
import unittest

class TestTransputerAESExtension(unittest.TestCase):
    def test_xserv2_aes_op_present(self):
        source = "src/x25519bench/ed25519/xserv2.c"
        self.assertTrue(os.path.exists(source), f"Source file missing: {source}")
        with open(source, "r") as f:
            content = f.read()
        self.assertIn("op == 'A'", content, "AES op 'A' missing from transputer server")
        self.assertIn("AES-128 CTR payload stream cipher offload", content)
        print("[+] Transputer AES-128 stream cipher offload op 'A' verified in xserv2.c")

if __name__ == "__main__":
    unittest.main()
