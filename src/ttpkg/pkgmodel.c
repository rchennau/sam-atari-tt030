#include "pkgmodel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEGS 128
#define SEG_STR_MAX 128

struct seg_item {
    char str[SEG_STR_MAX];
    int is_num;
};

static inline int is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline int is_ascii_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/*
 * seg(s, a) port from update.awk:
 *   function seg(s, a,   n) {
 *       n = 0;
 *       while (match(s, /[0-9]+|[A-Za-z]+/)) {
 *           a[++n] = substr(s, RSTART, RLENGTH);
 *           s = substr(s, RSTART + RLENGTH)
 *       }
 *       return n
 *   }
 */
static int seg(const char *s, struct seg_item *arr, int max_segs) {
    int n = 0;
    if (!s) return 0;
    const char *p = s;

    while (*p && n < max_segs) {
        while (*p && !is_ascii_digit(*p) && !is_ascii_alpha(*p)) {
            p++;
        }
        if (!*p) break;

        const char *start = p;
        int is_num = is_ascii_digit(*p);
        if (is_num) {
            while (*p && is_ascii_digit(*p)) {
                p++;
            }
        } else {
            while (*p && is_ascii_alpha(*p)) {
                p++;
            }
        }

        size_t len = (size_t)(p - start);
        if (len >= sizeof(arr[n].str)) {
            len = sizeof(arr[n].str) - 1;
        }
        memcpy(arr[n].str, start, len);
        arr[n].str[len] = '\0';
        arr[n].is_num = is_num;
        n++;
    }
    return n;
}

/*
 * vercmp(x, y) port from update.awk:
 *   function vercmp(x, y,   a, b, na, nb, i, dx, dy) {
 *       if (x == y) return 0
 *       na = seg(x, a); nb = seg(y, b)
 *       for (i = 1; i <= na && i <= nb; i++) {
 *           dx = a[i] ~ /^[0-9]/; dy = b[i] ~ /^[0-9]/
 *           if (dx != dy) return dx ? 1 : -1
 *           if (dx) { if (a[i] + 0 != b[i] + 0) return a[i] + 0 > b[i] + 0 ? 1 : -1 }
 *           else if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1
 *       }
 *       return (na > nb) - (na < nb)
 *   }
 */
int pkg_vercmp(const char *a, const char *b) {
    if (a == b) return 0;
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    if (strcmp(a, b) == 0) return 0;

    struct seg_item a_segs[MAX_SEGS];
    struct seg_item b_segs[MAX_SEGS];
    int na = seg(a, a_segs, MAX_SEGS);
    int nb = seg(b, b_segs, MAX_SEGS);

    int min_n = (na < nb) ? na : nb;
    for (int i = 0; i < min_n; i++) {
        int dx = a_segs[i].is_num;
        int dy = b_segs[i].is_num;
        if (dx != dy) {
            return dx ? 1 : -1;
        }
        if (dx) {
            const char *pa = a_segs[i].str;
            const char *pb = b_segs[i].str;
            while (*pa == '0') pa++;
            while (*pb == '0') pb++;
            size_t lena = strlen(pa);
            size_t lenb = strlen(pb);
            if (lena != lenb) {
                return (lena > lenb) ? 1 : -1;
            }
            int cmp = strcmp(pa, pb);
            if (cmp != 0) {
                return (cmp > 0) ? 1 : -1;
            }
        } else {
            int cmp = strcmp(a_segs[i].str, b_segs[i].str);
            if (cmp != 0) {
                return (cmp > 0) ? 1 : -1;
            }
        }
    }
    return (na > nb) - (na < nb);
}

static void safe_copy(char *dst, const char *src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void split_tsv_line(char *line, char *cols[], int max_cols) {
    for (int i = 0; i < max_cols; i++) {
        cols[i] = "";
    }
    int col = 0;
    char *p = line;
    cols[col++] = p;
    while (*p && col < max_cols) {
        if (*p == '\t') {
            *p = '\0';
            cols[col++] = p + 1;
        }
        p++;
    }
}

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d ? d : (char *)"";   /* ponytail: out of memory reads as an empty field, never a crash */
}

void pkg_free(struct pkg *p, int n) {
    for (int i = 0; i < n; i++) {
        if (p[i].provides && *p[i].provides) free(p[i].provides);
        if (p[i].cmds && *p[i].cmds) free(p[i].cmds);
        p[i].provides = p[i].cmds = NULL;
    }
}

int pkg_load_index(const char *path, struct pkg *out, int max) {
    if (!path || !out || max <= 0) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    char line[4096];
    int count = 0;

    while (count < max && fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        char *cols[10];
        split_tsv_line(line, cols, 10);

        memset(&out[count], 0, sizeof(struct pkg));
        safe_copy(out[count].name, cols[0], sizeof(out[count].name));
        safe_copy(out[count].ver, cols[1], sizeof(out[count].ver));
        safe_copy(out[count].rel, cols[2], sizeof(out[count].rel));
        out[count].size = cols[5][0] ? atol(cols[5]) : 0;
        out[count].provides = dupstr(cols[8]);
        out[count].cmds = dupstr(cols[9]);
        out[count].state = PKG_NOT_INSTALLED;
        out[count].inst[0] = '\0';

        count++;
    }

    fclose(fp);
    return count;
}

int pkg_apply_installed(const char *path, struct pkg *p, int n, int max) {
    if (!path || !p) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        char *tab = strchr(line, '\t');
        if (!tab) {
            continue;
        }
        *tab = '\0';
        char *name = line;
        char *inst_ver_rel = tab + 1;

        int found = -1;
        for (int i = 0; i < n; i++) {
            if (strcmp(p[i].name, name) == 0) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            char idx_vr[128];
            if (p[found].rel[0] != '\0') {
                snprintf(idx_vr, sizeof(idx_vr), "%s-%s", p[found].ver, p[found].rel);
            } else {
                snprintf(idx_vr, sizeof(idx_vr), "%s", p[found].ver);
            }

            if (pkg_vercmp(idx_vr, inst_ver_rel) > 0) {
                p[found].state = PKG_NEWER;
            } else {
                p[found].state = PKG_INSTALLED;
            }
            safe_copy(p[found].inst, inst_ver_rel, sizeof(p[found].inst));
        } else {
            if (n < max) {
                memset(&p[n], 0, sizeof(struct pkg));
                safe_copy(p[n].name, name, sizeof(p[n].name));
                safe_copy(p[n].inst, inst_ver_rel, sizeof(p[n].inst));

                char *dash = strrchr(inst_ver_rel, '-');
                if (dash) {
                    size_t vlen = (size_t)(dash - inst_ver_rel);
                    if (vlen >= sizeof(p[n].ver)) vlen = sizeof(p[n].ver) - 1;
                    memcpy(p[n].ver, inst_ver_rel, vlen);
                    p[n].ver[vlen] = '\0';
                    safe_copy(p[n].rel, dash + 1, sizeof(p[n].rel));
                } else {
                    safe_copy(p[n].ver, inst_ver_rel, sizeof(p[n].ver));
                    p[n].rel[0] = '\0';
                }

                p[n].state = PKG_LOCAL_ONLY;
                p[n].size = 0;
                p[n].provides = dupstr("");
                p[n].cmds = dupstr("");

                n++;
            }
        }
    }

    fclose(fp);
    return n;
}

int pkg_match(const struct pkg *p, const char *pat) {
    if (!pat || pat[0] == '\0') {
        return 1;
    }
    if (!p) {
        return 0;
    }
    if (strstr(p->name, pat) != NULL) return 1;
    if (strstr(p->provides, pat) != NULL) return 1;
    if (strstr(p->cmds, pat) != NULL) return 1;
    return 0;
}
