"""t425_inventory.rule (tt030-t425-kernel-ports FR-1): the first matching rule decides, and every row
says which rule that was."""
import t425_inventory as I


def row(name, group="Applications/File", summary="", cmds="x"):
    return {"package": name, "group": group, "summary": summary, "cmds": cmds}


def test_rule_precedence():
    assert I.rule(row("uudeview", summary="Smart decoder"))[0] == "IO/INTERACTIVE"      # override wins
    assert I.rule(row("t425-compress"))[:2] == ("KERNEL", "deflate+bwt")               # already ported
    assert I.rule(row("zlib-devel"))[0] == "DATA/DOCS"                                 # headers before family
    assert I.rule(row("gmp", cmds=""))[:2] == ("KERNEL", "bignum")                     # family seed
    assert I.rule(row("fonts-x", group="Fonts/X", cmds=""))[0] == "DATA/DOCS"
    assert I.rule(row("foo", summary="audio thing"))[0] == "BORDERLINE"                # compute words, no family
    assert I.rule(row("bar", summary="a pager"))[0] == "IO/INTERACTIVE"
