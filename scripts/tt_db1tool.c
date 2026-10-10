/* tt_db1tool dump FILE | tt_db1tool add FILE < "key\toffset\tfilenum" lines  (built by scripts/tt_rpmdb.py).
   rpm 3.0.6 index value = array of {u32 recOffset, u32 fileNumber}, big-endian (m68k native). */
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <db.h>
static unsigned be32(const unsigned char *p) { return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
int main(int argc, char **argv) {
    int add = argc == 3 && !strcmp(argv[1], "add");
    if (argc != 3 || (!add && strcmp(argv[1], "dump"))) { fprintf(stderr, "usage: db1tool dump|add FILE\n"); return 2; }
    DB *db = dbopen(argv[2], add ? O_RDWR : O_RDONLY, 0, DB_HASH, NULL);
    if (!db) { perror(argv[2]); return 1; }
    DBT k, d;
    int r;
    size_t i;
    if (!add) {
        for (r = db->seq(db, &k, &d, R_FIRST); r == 0; r = db->seq(db, &k, &d, R_NEXT)) {
            printf("%.*s\t", (int)k.size, (char *)k.data);
            for (i = 0; i + 8 <= d.size; i += 8)
                printf("%s%u:%u", i ? "," : "", be32((unsigned char *)d.data + i), be32((unsigned char *)d.data + i + 4));
            printf("%s\n", d.size % 8 ? "\t!BADLEN" : "");
        }
        return db->close(db) ? 1 : 0;
    }
    char line[4096]; unsigned n = 0;
    while (fgets(line, sizeof line, stdin)) {
        char *t1 = strchr(line, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (!t2) { fprintf(stderr, "bad line: %s", line); return 1; }
        *t1 = 0; unsigned off = strtoul(t1 + 1, NULL, 10), fn = strtoul(t2 + 1, NULL, 10);
        k.data = line; k.size = strlen(line);
        r = db->get(db, &k, &d, 0);
        if (r < 0) { perror("get"); return 1; }
        size_t old = r ? 0 : d.size;
        unsigned char *buf = malloc(old + 8);
        if (old) memcpy(buf, d.data, old);
        unsigned char *p = buf + old;
        p[0] = off >> 24; p[1] = off >> 16; p[2] = off >> 8; p[3] = off;
        p[4] = fn >> 24; p[5] = fn >> 16; p[6] = fn >> 8; p[7] = fn;
        d.data = buf; d.size = old + 8;
        if (db->put(db, &k, &d, 0)) { perror("put"); return 1; }
        free(buf); n++;
    }
    /* no db->sync(): db1 __put_page swaps the buffer in place on a foreign-endian host, so sync+close writes it twice */
    if (db->close(db)) { perror("close"); return 1; }
    fprintf(stderr, "%s: %u records added\n", argv[2], n);
    return 0;
}
