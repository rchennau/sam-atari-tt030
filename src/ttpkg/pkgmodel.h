#ifndef PKGMODEL_H
#define PKGMODEL_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PKG_NOT_INSTALLED = 0,
    PKG_INSTALLED = 1,
    PKG_NEWER = 2,
    PKG_LOCAL_ONLY = 3
};

struct pkg {
    char name[64];
    char ver[64];
    char rel[32];
    char *provides;   /* heap, exact size: netpbm-progs' command list is 1,976 bytes; fixed */
    char *cmds;       /* buffers that hold it for 442 packages would break NFR-4's 1 MB */
    long size;
    int state;
    char inst[96];
};

int pkg_vercmp(const char *a, const char *b);
int pkg_load_index(const char *path, struct pkg *out, int max);
int pkg_apply_installed(const char *path, struct pkg *p, int n, int max);
int pkg_match(const struct pkg *p, const char *pat);

#ifdef __cplusplus
}
#endif

/* Release provides/cmds of the first n entries. */
void pkg_free(struct pkg *p, int n);

#endif /* PKGMODEL_H */
