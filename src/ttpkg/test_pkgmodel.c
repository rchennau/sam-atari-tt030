#include "pkgmodel.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_vercmp_awk_pairs(void) {
    /* 1. 1.0 vs 1.0.1: -1 */
    assert(pkg_vercmp("1.0", "1.0.1") == -1);

    /* 2. 1.0.1 vs 1.0: 1 */
    assert(pkg_vercmp("1.0.1", "1.0") == 1);

    /* 3. 2 vs 10: -1 */
    assert(pkg_vercmp("2", "10") == -1);

    /* 4. 10 vs 2: 1 */
    assert(pkg_vercmp("10", "2") == 1);

    /* 5. 1.0a vs 1.0: 1 */
    assert(pkg_vercmp("1.0a", "1.0") == 1);

    /* 6. 1.0 vs 1.0a: -1 */
    assert(pkg_vercmp("1.0", "1.0a") == -1);

    /* 7. Release-only difference (older vs newer): 1.0-1 vs 1.0-2: -1 */
    assert(pkg_vercmp("1.0-1", "1.0-2") == -1);

    /* 8. Release-only difference (newer vs older): 1.0-2 vs 1.0-1: 1 */
    assert(pkg_vercmp("1.0-2", "1.0-1") == 1);

    /* Additional pairs derived from update.awk */
    assert(pkg_vercmp("1.0-1", "1.0-1") == 0);
    assert(pkg_vercmp("1.0-1a", "1.0-1b") == -1);
    assert(pkg_vercmp("1.0.1", "1.0.a") == 1);
    assert(pkg_vercmp("1.0.a", "1.0.1") == -1);
}

int main(void) {
    const char *idx_file = "/tmp/test_pkgmodel_index.tsv";
    const char *inst_file = "/tmp/test_pkgmodel_installed.txt";

    /* Test 1: pkg_vercmp test cases matching update.awk */
    test_vercmp_awk_pairs();

    /* Write index fixture: 3 packages, one with provides and commands */
    FILE *f_idx = fopen(idx_file, "w");
    assert(f_idx != NULL);
    fprintf(f_idx, "#serial\t100\n");
    fprintf(f_idx, "bash\t2.05b\t1\tm68k\tbuilt/bash-2.05b-1.m68k.rpm\t102400\tsha256_bash\tglibc\tsh,bash\tbash,sh\n");
    fprintf(f_idx, "vim\t8.2\t2\tm68k\tbuilt/vim-8.2-2.m68k.rpm\t204800\tsha256_vim\tncurses\tvi\tvim,vi\n");
    fprintf(f_idx, "less\t590\t1\tm68k\tbuilt/less-590-1.m68k.rpm\t51200\tsha256_less\tncurses\t\t\n");
    fclose(f_idx);

    /* Write installed fixture:
     * - bash: same version (2.05b-1)
     * - vim: older version (8.2-1 vs mirror 8.2-2)
     * - tar: name not in index (1.34-1)
     * (less is not in installed list)
     */
    FILE *f_inst = fopen(inst_file, "w");
    assert(f_inst != NULL);
    fprintf(f_inst, "bash\t2.05b-1\n");
    fprintf(f_inst, "vim\t8.2-1\n");
    fprintf(f_inst, "tar\t1.34-1\n");
    fclose(f_inst);

    /* Load index */
    struct pkg pkgs[16];
    int count = pkg_load_index(idx_file, pkgs, 16);
    assert(count == 3);

    /* Check initial state of loaded packages */
    assert(strcmp(pkgs[0].name, "bash") == 0);
    assert(strcmp(pkgs[0].ver, "2.05b") == 0);
    assert(strcmp(pkgs[0].rel, "1") == 0);
    assert(pkgs[0].size == 102400);
    assert(strcmp(pkgs[0].provides, "sh,bash") == 0);
    assert(strcmp(pkgs[0].cmds, "bash,sh") == 0);
    assert(pkgs[0].state == PKG_NOT_INSTALLED);
    assert(pkgs[0].inst[0] == '\0');

    assert(strcmp(pkgs[1].name, "vim") == 0);
    assert(strcmp(pkgs[1].ver, "8.2") == 0);
    assert(strcmp(pkgs[1].rel, "2") == 0);
    assert(pkgs[1].size == 204800);
    assert(strcmp(pkgs[1].provides, "vi") == 0);
    assert(strcmp(pkgs[1].cmds, "vim,vi") == 0);
    assert(pkgs[1].state == PKG_NOT_INSTALLED);
    assert(pkgs[1].inst[0] == '\0');

    assert(strcmp(pkgs[2].name, "less") == 0);
    assert(strcmp(pkgs[2].ver, "590") == 0);
    assert(strcmp(pkgs[2].rel, "1") == 0);
    assert(pkgs[2].size == 51200);
    assert(pkgs[2].provides[0] == '\0');
    assert(pkgs[2].cmds[0] == '\0');
    assert(pkgs[2].state == PKG_NOT_INSTALLED);
    assert(pkgs[2].inst[0] == '\0');

    /* Apply installed list */
    int total = pkg_apply_installed(inst_file, pkgs, count, 16);
    assert(total == 4);

    /* Assert all 4 states */
    /* 1. PKG_INSTALLED: bash has same version as mirror */
    assert(pkgs[0].state == PKG_INSTALLED);
    assert(strcmp(pkgs[0].inst, "2.05b-1") == 0);

    /* 2. PKG_NEWER: vim mirror 8.2-2 > installed 8.2-1 */
    assert(pkgs[1].state == PKG_NEWER);
    assert(strcmp(pkgs[1].inst, "8.2-1") == 0);

    /* 3. PKG_NOT_INSTALLED: less is not installed */
    assert(pkgs[2].state == PKG_NOT_INSTALLED);
    assert(pkgs[2].inst[0] == '\0');

    /* 4. PKG_LOCAL_ONLY: tar is installed but not in index */
    assert(pkgs[3].state == PKG_LOCAL_ONLY);
    assert(strcmp(pkgs[3].name, "tar") == 0);
    assert(strcmp(pkgs[3].inst, "1.34-1") == 0);
    assert(strcmp(pkgs[3].ver, "1.34") == 0);
    assert(strcmp(pkgs[3].rel, "1") == 0);

    /* Assert pkg_match on empty, name, provides, cmds */
    /* Empty pattern matches everything */
    assert(pkg_match(&pkgs[0], "") == 1);
    assert(pkg_match(&pkgs[0], NULL) == 1);

    /* Match on name */
    assert(pkg_match(&pkgs[0], "bash") == 1);
    assert(pkg_match(&pkgs[0], "bas") == 1);
    assert(pkg_match(&pkgs[1], "vi") == 1);

    /* Match on provides */
    assert(pkg_match(&pkgs[0], "sh") == 1);
    assert(pkg_match(&pkgs[1], "vi") == 1);

    /* Match on cmds */
    assert(pkg_match(&pkgs[0], "bash") == 1);
    assert(pkg_match(&pkgs[1], "vim") == 1);

    /* Non-matching pattern */
    assert(pkg_match(&pkgs[2], "xyz") == 0);
    assert(pkg_match(&pkgs[0], "Bash") == 0); /* case-sensitive check */

    /* Clean up fixture files */
    unlink(idx_file);
    unlink(inst_file);

    printf("ALL TESTS PASSED\n");
    return 0;
}
