# Timing probe for FR-3 on the TT: parse index.tsv, rpmvercmp every package against its
# predecessor, then the transitive Requires closure of ROOT via the Provides map.
#   awk -F'\t' -v ROOT=pkg414 -f closure_probe.awk index.tsv
function seg(s, a,   n) { n = 0; while (match(s, /[0-9]+|[A-Za-z]+/)) { a[++n] = substr(s, RSTART, RLENGTH); s = substr(s, RSTART + RLENGTH) } return n }
function vercmp(x, y,   a, b, na, nb, i, dx, dy) {
    if (x == y) return 0
    na = seg(x, a); nb = seg(y, b)
    for (i = 1; i <= na && i <= nb; i++) {
        dx = a[i] ~ /^[0-9]/; dy = b[i] ~ /^[0-9]/
        if (dx != dy) return dx ? 1 : -1
        if (dx) { if (a[i] + 0 != b[i] + 0) return a[i] + 0 > b[i] + 0 ? 1 : -1 }
        else if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1
    }
    return (na > nb) - (na < nb)
}
{ req[$1] = $8; n = split($9, p, ","); for (i = 1; i <= n; i++) prov[p[i]] = $1
  if (NR > 1) cmps += vercmp($2, prev) != 0; prev = $2 }
END {
    todo[ROOT] = 1; got = 0
    while (1) {
        moved = 0
        for (k in todo) {
            delete todo[k]; if (k in have) continue
            have[k] = 1; got++; moved = 1
            n = split(req[k], r, ",")
            for (i = 1; i <= n; i++) if (r[i] != "") { q = (r[i] in prov) ? prov[r[i]] : ""; if (q == "") miss[r[i]] = 1; else if (!(q in have)) todo[q] = 1 }
        }
        if (!moved) break
    }
    m = 0; for (k in miss) m++
    print "rows", NR, "vercmp_nonzero", cmps, "closure", got, "unresolved", m
}
