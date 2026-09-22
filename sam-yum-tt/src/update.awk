# update.awk — which installed packages have a newer version on the mirror (yum update, tt030-rpm-pipeline).
#   awk -F'\t' -v ONLY="pkg ..." -f update.awk INSTALLED index.tsv
# INSTALLED: "name<TAB>version<TAB>release" per line (rpm -qa --queryformat). Output like resolve.awk:
#   GET <name> <path> <sha256> <size>   for each package whose mirror version-release is newer.
# rpmvercmp as in closure_probe.awk (checked against the Python reference and on the TT, 2026-09-21).
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
BEGIN { n = split(ONLY, o, " "); for (i = 1; i <= n; i++) only[o[i]] = 1 }
FNR == NR { iv[$1] = $2; ir[$1] = $3; next }
/^#/ { next }
($1 in iv) && (!n || ($1 in only)) {
    c = vercmp($2, iv[$1]); if (c == 0) c = vercmp($3, ir[$1])
    if (c > 0) print "GET " $1 " " $5 " " $7 " " $6
}
