# near.awk — near matches for a package name the mirror does not have (yum install, tt030-rpm-pipeline).
#
#   awk -F'\t' -v WANT="top vi" -f near.awk index.tsv
#
# Output, per wanted name, best first, at most 8:
#   NEAR <want> <name> <version-release> cmd|name|spelling
#     cmd       the package ships a command (or provides) exactly <want>   (top -> pstop)
#     name      one name contains the other                                 (vi -> vim)
#     spelling  edit distance <= 2 (1 for names under 4 chars)              (lses -> less)
# Substring and spelling need >= 3 chars: `ls` would otherwise match every *utils package.
# Nothing printed for a want with no near match: the caller falls through to build-on-miss.
# mawk has no asort: the index is already sorted by name, so each rank keeps index order.
/^#/ { next }
{ nm[++n] = $1; vr[n] = $2 "-" $3; cm[n] = "," $10 "," $9 "," }
function lev(a, b,   la, lb, i, j, c, d, prev, cur) {
    la = length(a); lb = length(b)
    for (j = 0; j <= lb; j++) prev[j] = j
    for (i = 1; i <= la; i++) {
        cur[0] = i
        for (j = 1; j <= lb; j++) {
            c = prev[j - 1] + (substr(a, i, 1) != substr(b, j, 1))
            d = prev[j] + 1; if (d < c) c = d
            d = cur[j - 1] + 1; if (d < c) c = d
            cur[j] = c
        }
        for (j = 0; j <= lb; j++) prev[j] = cur[j]
    }
    return prev[lb]
}
function rank(w, k,   lw, d) {
    if (nm[k] == w) return -1
    if (index(cm[k], "," w ",") || index(cm[k], "/" w ",")) return 0
    lw = length(w)
    if (lw >= 3 && index(nm[k], w)) return 1
    if (length(nm[k]) >= 3 && index(w, nm[k])) return 1
    d = length(nm[k]) - lw; if (d < 0) d = -d
    if (lw >= 3 && d <= 2 && lev(w, nm[k]) <= (lw < 4 ? 1 : 2)) return 2
    return -1
}
END {
    split("cmd name spelling", why, " ")
    nw = split(WANT, w, " ")
    for (i = 1; i <= nw; i++) {
        for (k = 1; k <= n; k++) r[k] = rank(w[i], k)
        shown = 0
        for (q = 0; q <= 2 && shown < 8; q++)
            for (k = 1; k <= n && shown < 8; k++)
                if (r[k] == q) { print "NEAR " w[i] " " nm[k] " " vr[k] " " why[q + 1]; shown++ }
    }
}
