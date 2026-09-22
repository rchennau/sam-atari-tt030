# resolve.awk — dependency closure for the TT yum client (tt030-rpm-pipeline FR-3).
#
#   awk -F'\t' -v WANT="less vim" -f resolve.awk INSTALLED index.tsv
#
# INSTALLED: one installed package NAME per line (rpm -qa --queryformat '%{NAME}\n').
# index.tsv: name version release arch path size sha256 requires(csv) provides(csv).
# Output, one per line, for the shell to act on:
#   GET <name> <path> <sha256> <size>   fetch + install this package
#   NOTFOUND <name>                     a requested package is not on the mirror (FR-4 builds it)
#   UNRESOLVED <require> <needed-by>    nothing installed or on the mirror provides it
# A require is satisfied by an installed package providing it, a path that exists on disk, or a
# mirror package (which joins the closure). The index carries one (newest) entry per name, so no
# version compare happens here.
# ponytail: versioned requires are not in the index; add rpmvercmp when one is.
FNR == NR { inst[$1] = 1; next }
/^#/ { next }                                   # "#serial" header line
{
    ver[$1] = $2 "-" $3; path[$1] = $5; size[$1] = $6; sha[$1] = $7; req[$1] = $8
    n = split($9, p, ",")
    for (i = 1; i <= n; i++) if (!(p[i] in prov)) prov[p[i]] = $1
}
function satisfied_locally(r,   q) {
    if (r in prov && prov[r] in inst) return 1
    if (r in inst) return 1
    if (substr(r, 1, 1) == "/" && system("test -e '" r "'") == 0) return 1
    return 0
}
END {
    nw = split(WANT, w, " ")
    for (i = 1; i <= nw; i++) {
        if (w[i] in inst) continue
        if (!(w[i] in ver)) { print "NOTFOUND " w[i]; continue }
        todo[w[i]] = 1
    }
    while (1) {
        moved = 0
        for (k in todo) {
            delete todo[k]
            if (k in take) continue
            take[k] = 1; moved = 1
            n = split(req[k], r, ",")
            for (i = 1; i <= n; i++) {
                if (r[i] == "" || satisfied_locally(r[i])) continue
                if (r[i] in prov) { if (!(prov[r[i]] in take)) todo[prov[r[i]]] = 1 }
                else print "UNRESOLVED " r[i] " " k
            }
        }
        if (!moved) break
    }
    for (k in take) print "GET " k " " path[k] " " sha[k] " " size[k]
}
