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


# FR-2: the window's filter (pkg_match) against yum's own search rule, taken from the shipped yum script so the
# two cannot drift. Patterns cover each column: name, provides-only (windowmanager, /bin/sh), command-only (ppmto),
# a broad hit (lib) and case-sensitivity (Magick vs magick).
FR2_PATTERNS = ["lib", "windowmanager", "/bin/sh", "ppmto", "Magick", "magick"]
FR2_DRV = '#include <stdio.h>\n#include <stdlib.h>\n#include "pkgmodel.h"\nstatic struct pkg p[2000];\n' \
          'int main(int c,char**v){int n=pkg_load_index(v[1],p,2000);for(int a=2;a<c;a++){printf("=%s\\n",v[a]);' \
          'for(int i=0;i<n;i++)if(pkg_match(&p[i],v[a]))printf("%s\\n",p[i].name);}return 0;}\n'


def _yum_search_awk():
    with open(f"{R}/sam-yum-tt/src/yum") as fh:
        src = fh.read()
    body = src[src.index("list|search)"):]
    start = body.index("'/^#/") + 1                  # the program string, after -F'\\t' and the -v options
    return body[start:body.index("' \\", start)]


def _sets(lines):
    out, cur = {}, None
    for line in lines:
        if line.startswith("="):
            cur = out.setdefault(line[1:], set())
        else:
            cur.add(line)
    return out


def test_filter_matches_yum_search(tmp_path):
    import pytest
    idx = os.path.join(os.environ.get("TT030_MIRROR", "/mnt/vault/tt030"), "index.tsv")
    if not os.path.exists(idx):
        pytest.skip(f"no mirror index at {idx}")
    prog = _yum_search_awk()
    want = {}
    for pat in FR2_PATTERNS:
        r = subprocess.run(["awk", "-F\t", "-v", f"pat={pat}", "-v", "s=search", prog, idx], text=True,
                           capture_output=True, check=True)
        want[pat] = {line.split("\t")[0] for line in r.stdout.splitlines()}
    (tmp_path / "d.c").write_text(FR2_DRV)
    subprocess.run(["gcc", "-std=c99", "-I", SRC, "-o", tmp_path / "d", tmp_path / "d.c", f"{SRC}/pkgmodel.c"], check=True)
    got = _sets(subprocess.run([tmp_path / "d", idx, *FR2_PATTERNS], text=True, capture_output=True,
                               check=True).stdout.splitlines())
    for pat in FR2_PATTERNS:
        assert want[pat], f"pattern {pat!r} hits nothing; it tests nothing"
        assert got.get(pat, set()) == want[pat], pat
    assert want["Magick"] != want["magick"]
