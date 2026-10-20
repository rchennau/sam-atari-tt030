"""Host self-check for sam-yum-tt/src/t4adler.c: compiled C adler32 must equal zlib.adler32."""
import os
import subprocess
import tempfile
import zlib

SRC = os.path.join(os.path.dirname(__file__), "..", "sam-yum-tt", "src", "t4adler.c")


def build_and_run(data: bytes) -> int:
    with tempfile.TemporaryDirectory() as tmp:
        main_src = os.path.join(tmp, "main.c")
        with open(main_src, "w") as f:
            f.write(
                "#include <stdio.h>\n"
                "#include \"t4adler.h\"\n"
                "int main(int argc, char **argv)\n"
                "{\n"
                "    if (argc != 2) return 1;\n"
                "    FILE *f = fopen(argv[1], \"rb\");\n"
                "    if (!f) return 1;\n"
                "    fseek(f, 0, SEEK_END);\n"
                "    long n = ftell(f);\n"
                "    fseek(f, 0, SEEK_SET);\n"
                "    unsigned char *buf = malloc((size_t)n + 1);\n"
                "    if (!buf) return 1;\n"
                "    if (fread(buf, 1, (size_t)n, f) != (size_t)n) return 1;\n"
                "    fclose(f);\n"
                "    printf(\"%lx\\n\", t4_adler32(buf, n));\n"
                "    return 0;\n}\n"
            )
        with open(os.path.join(tmp, "t4adler.h"), "w") as f:
            f.write("unsigned long t4_adler32(const unsigned char *p, long n);\n")
        with open(os.path.join(tmp, "data.bin"), "wb") as f:
            f.write(data)
        exe = os.path.join(tmp, "t4adler")
        subprocess.run(
            ["cc", "-O2", "-o", exe, main_src, SRC],
            check=True,
            cwd=tmp,
        )
        out = subprocess.run([exe, os.path.join(tmp, "data.bin")], capture_output=True, check=True)
        return int(out.stdout.strip(), 16)


def test_empty():
    data = b""
    assert build_and_run(data) == zlib.adler32(data) & 0xFFFFFFFF


def test_5551_pattern():
    data = bytes((i % 256) for i in range(5551))
    assert build_and_run(data) == zlib.adler32(data) & 0xFFFFFFFF


def test_70000_random():
    import random

    rng = random.Random(42)
    data = bytes(rng.randrange(256) for _ in range(70000))
    assert build_and_run(data) == zlib.adler32(data) & 0xFFFFFFFF
