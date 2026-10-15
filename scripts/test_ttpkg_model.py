"""tt030-yum-gui FR-1/FR-2: the ttpkg C model's own tests, and rpmvercmp.tsv against BOTH update.awk and the C port
(one vector file, so the two implementations cannot drift).  pytest scripts/test_ttpkg_model.py"""
import os
import subprocess

R = os.path.join(os.path.dirname(__file__), "..")
SRC = os.path.join(R, "src", "ttpkg")
VEC = os.path.join(R, "sam-yum-tt", "src", "rpmvercmp.tsv")
DRV = '#include <stdio.h>\n#include "pkgmodel.h"\nint main(void){char a[128],b[128];' \
      'while(scanf("%127s %127s",a,b)==2)printf("%d\\n",pkg_vercmp(a,b));return 0;}\n'


def _rows():
    with open(VEC) as fh:
        return [line.rstrip("\n").split("\t") for line in fh if not line.startswith("#")]


def test_model_unit_tests(tmp_path):
    exe = tmp_path / "t"
    subprocess.run(["gcc", "-std=c99", "-Wall", "-Werror", "-o", exe, f"{SRC}/test_pkgmodel.c", f"{SRC}/pkgmodel.c"],
                   check=True)
    subprocess.run([exe], check=True)


def test_vectors_agree_with_awk_and_c(tmp_path):
    rows = _rows()
    assert len(rows) >= 30
    pairs = "".join(f"{a} {b}\n" for a, b, _ in rows)
    awk, keep = "", False                            # update.awk's function definitions only, not its rules
    with open(f"{R}/sam-yum-tt/src/update.awk") as fh:
        for line in fh:
            keep = keep or line.startswith("function")
            awk += line if keep else ""
            keep = keep and not line.startswith("}")
    got_awk = subprocess.run(["awk", awk + "\n{print vercmp($1,$2)}"], input=pairs, text=True,
                             capture_output=True, check=True).stdout.split()
    (tmp_path / "d.c").write_text(DRV)
    subprocess.run(["gcc", "-std=c99", "-I", SRC, "-o", tmp_path / "d", tmp_path / "d.c", f"{SRC}/pkgmodel.c"], check=True)
    got_c = subprocess.run([tmp_path / "d"], input=pairs, text=True, capture_output=True, check=True).stdout.split()
    want = [e for _, _, e in rows]
    assert got_awk == want and got_c == want
