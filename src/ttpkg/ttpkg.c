#include <gem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pkgmodel.h"

#ifndef WF_VSLPOSN
#define WF_VSLPOSN WF_VSLIDE
#endif

#define MAX_PKGS 1000

enum {
    FILTER_ALL = 0,
    FILTER_INSTALLED = 1,
    FILTER_UPDATES = 2,
    FILTER_NOT_INSTALLED = 3
};

static struct pkg pkgs[MAX_PKGS];
static int total_pkgs = 0;
static int n_installed = 0;
static int n_newer = 0;
static int installed_unknown = 0;

static int visible_indices[MAX_PKGS];
static int visible_count = 0;
static int current_filter = FILTER_ALL;
static char find_text[64];

static short win_handle = -1;
static short vdi_handle = 0;
static short char_w = 8, char_h = 16, box_w = 8, box_h = 16;
static short row_height = 16;
static short visible_rows = 1;
static int scroll_top = 0;
static int selected_index = -1;

static char win_title[] = "TTPkg";
static char info_text[128];

static const char *filter_labels[4] = {
    " All ",
    " Installed ",
    " Updates ",
    " Not installed "
};

static GRECT btn_rect[4];
static short find_prompt_x = 0;

static void update_info_line(void)
{
    if (installed_unknown) {
        snprintf(info_text, sizeof(info_text),
                 "%d of %d packages  |  installed set unknown",
                 visible_count, total_pkgs);
    } else {
        snprintf(info_text, sizeof(info_text),
                 "%d of %d packages  |  %d installed  |  %d updates",
                 visible_count, total_pkgs, n_installed, n_newer);
    }
    if (win_handle > 0) {
        wind_set_str(win_handle, WF_INFO, info_text);
    }
}

static void update_slider(void)
{
    short vslsize, vslpos;
    if (win_handle <= 0) return;

    if (visible_count <= 0 || visible_rows >= visible_count) {
        vslsize = 1000;
        vslpos = 1;
    } else {
        vslsize = (short)((long)visible_rows * 1000 / visible_count);
        if (vslsize < 50) vslsize = 50;
        if (vslsize > 1000) vslsize = 1000;

        int max_scroll = visible_count - visible_rows;
        vslpos = (short)(1 + ((long)scroll_top * 999 + max_scroll / 2) / max_scroll);
        if (vslpos < 1) vslpos = 1;
        if (vslpos > 1000) vslpos = 1000;
    }
    wind_set(win_handle, WF_VSLSIZE, vslsize, 0, 0, 0);
    wind_set(win_handle, WF_VSLPOSN, vslpos, 0, 0, 0);
}

static int match_state_filter(int state, int filter)
{
    switch (filter) {
    case FILTER_ALL:
        return 1;
    case FILTER_INSTALLED:
        return (state == PKG_INSTALLED || state == PKG_NEWER || state == PKG_LOCAL_ONLY);
    case FILTER_UPDATES:
        return (state == PKG_NEWER);
    case FILTER_NOT_INSTALLED:
        return (state == PKG_NOT_INSTALLED);
    default:
        return 1;
    }
}

static void rebuild_visible_list(void)
{
    int prev_selected_pkg = (selected_index >= 0 && selected_index < visible_count)
                            ? visible_indices[selected_index] : -1;

    visible_count = 0;
    for (int i = 0; i < total_pkgs; i++) {
        if (match_state_filter(pkgs[i].state, current_filter)) {
            if (pkg_match(&pkgs[i], find_text)) {
                if (visible_count < MAX_PKGS) {
                    visible_indices[visible_count++] = i;
                }
            }
        }
    }

    selected_index = -1;
    if (prev_selected_pkg >= 0) {
        for (int j = 0; j < visible_count; j++) {
            if (visible_indices[j] == prev_selected_pkg) {
                selected_index = j;
                break;
            }
        }
    }

    int max_scroll = (visible_count > visible_rows) ? visible_count - visible_rows : 0;
    if (scroll_top > max_scroll) scroll_top = max_scroll;
    if (scroll_top < 0) scroll_top = 0;

    update_slider();
    update_info_line();
}

static void recalc_layout(void)
{
    if (win_handle <= 0) return;
    short wx, wy, ww, wh;
    wind_get(win_handle, WF_WORKXYWH, &wx, &wy, &ww, &wh);
    short list_top = wy + row_height + 2;
    short avail_h = wy + wh - list_top;
    visible_rows = avail_h / row_height;
    if (visible_rows < 1) visible_rows = 1;

    int max_scroll = (visible_count > visible_rows) ? visible_count - visible_rows : 0;
    if (scroll_top > max_scroll) scroll_top = max_scroll;
    if (scroll_top < 0) scroll_top = 0;
}

static void update_toolbar_geometry(short wx, short wy)
{
    short cur_x = wx + 4;
    for (int i = 0; i < 4; i++) {
        short w = (short)(strlen(filter_labels[i]) * char_w);
        btn_rect[i].g_x = cur_x;
        btn_rect[i].g_y = wy;
        btn_rect[i].g_w = w;
        btn_rect[i].g_h = row_height;
        cur_x += w + 6;
    }
    find_prompt_x = cur_x + 4;
}

static const char *get_marker(int state)
{
    switch (state) {
    case PKG_INSTALLED:
        return "* ";
    case PKG_NEWER:
        return "+ ";
    case PKG_LOCAL_ONLY:
        return "L ";
    case PKG_NOT_INSTALLED:
    default:
        return "  ";
    }
}

static void get_version_release(const struct pkg *p, char *out, size_t out_size)
{
    if (p->state == PKG_LOCAL_ONLY && p->inst[0]) {
        snprintf(out, out_size, "%s", p->inst);
    } else if (p->rel[0]) {
        snprintf(out, out_size, "%s-%s", p->ver, p->rel);
    } else if (p->inst[0]) {
        snprintf(out, out_size, "%s", p->inst);
    } else {
        snprintf(out, out_size, "%s", p->ver);
    }
}

static void draw_window_contents(const GRECT *clip)
{
    GRECT work;
    wind_get_grect(win_handle, WF_WORKXYWH, &work);

    /* Clear clipped rectangle to white */
    short clr[4];
    clr[0] = clip->g_x;
    clr[1] = clip->g_y;
    clr[2] = clip->g_x + clip->g_w - 1;
    clr[3] = clip->g_y + clip->g_h - 1;
    vsf_interior(vdi_handle, FIS_SOLID);
    vsf_color(vdi_handle, 0);
    vswr_mode(vdi_handle, MD_REPLACE);
    v_bar(vdi_handle, clr);

    vst_color(vdi_handle, 1);
    vsl_color(vdi_handle, 1);

    /* Draw toolbar if visible within clip */
    if (clip->g_y < work.g_y + row_height + 2 && clip->g_y + clip->g_h > work.g_y) {
        update_toolbar_geometry(work.g_x, work.g_y);

        for (int i = 0; i < 4; i++) {
            short bpxy[10];
            bpxy[0] = btn_rect[i].g_x;                        bpxy[1] = btn_rect[i].g_y;
            bpxy[2] = btn_rect[i].g_x + btn_rect[i].g_w - 1; bpxy[3] = btn_rect[i].g_y;
            bpxy[4] = btn_rect[i].g_x + btn_rect[i].g_w - 1; bpxy[5] = btn_rect[i].g_y + btn_rect[i].g_h - 1;
            bpxy[6] = btn_rect[i].g_x;                        bpxy[7] = btn_rect[i].g_y + btn_rect[i].g_h - 1;
            bpxy[8] = btn_rect[i].g_x;                        bpxy[9] = btn_rect[i].g_y;
            v_pline(vdi_handle, 5, bpxy);

            v_gtext(vdi_handle, btn_rect[i].g_x, btn_rect[i].g_y, filter_labels[i]);

            if (current_filter == i) {
                short hpxy[4];
                hpxy[0] = btn_rect[i].g_x;
                hpxy[1] = btn_rect[i].g_y;
                hpxy[2] = btn_rect[i].g_x + btn_rect[i].g_w - 1;
                hpxy[3] = btn_rect[i].g_y + btn_rect[i].g_h - 1;
                vswr_mode(vdi_handle, MD_XOR);
                vsf_color(vdi_handle, 1);
                v_bar(vdi_handle, hpxy);
                vswr_mode(vdi_handle, MD_REPLACE);
            }
        }

        char find_buf[96];
        snprintf(find_buf, sizeof(find_buf), "Find: %s", find_text);
        v_gtext(vdi_handle, find_prompt_x, work.g_y, find_buf);

        short sep[4];
        sep[0] = work.g_x;
        sep[1] = work.g_y + row_height;
        sep[2] = work.g_x + work.g_w - 1;
        sep[3] = work.g_y + row_height;
        v_pline(vdi_handle, 2, sep);
    }

    /* Draw visible package rows */
    short list_top = work.g_y + row_height + 2;
    short avail_h = work.g_y + work.g_h - list_top;
    short vrows = avail_h / row_height;
    if (vrows < 1) vrows = 1;

    int max_chars = (work.g_w - 8) / char_w;
    int name_w = 22;
    int ver_w = 16;
    if (max_chars > 70) {
        name_w = 30;
        ver_w = 22;
    } else if (max_chars > 60) {
        name_w = 26;
        ver_w = 18;
    }

    for (int r = 0; r < vrows; r++) {
        int idx = scroll_top + r;
        if (idx >= visible_count) break;

        short ry = list_top + r * row_height;
        if (ry + row_height <= clip->g_y || ry >= clip->g_y + clip->g_h) {
            continue;
        }

        int pkg_idx = visible_indices[idx];
        const struct pkg *p = &pkgs[pkg_idx];
        const char *marker = get_marker(p->state);
        char vr[64];
        get_version_release(p, vr, sizeof(vr));

        long kb = (p->size > 0) ? ((p->size + 1023) / 1024) : 0;

        char line_buf[256];
        snprintf(line_buf, sizeof(line_buf), "%-2s %-*.*s %-*.*s %6ld KB",
                 marker, name_w, name_w, p->name, ver_w, ver_w, vr, kb);

        v_gtext(vdi_handle, work.g_x + 4, ry, line_buf);

        if (idx == selected_index) {
            short rpxy[4];
            rpxy[0] = work.g_x;
            rpxy[1] = ry;
            rpxy[2] = work.g_x + work.g_w - 1;
            rpxy[3] = ry + row_height - 1;
            vswr_mode(vdi_handle, MD_XOR);
            vsf_color(vdi_handle, 1);
            v_bar(vdi_handle, rpxy);
            vswr_mode(vdi_handle, MD_REPLACE);
        }
    }
}

static void do_redraw(short win, const GRECT *dirty)
{
    GRECT rect, work;
    wind_update(BEG_UPDATE);
    graf_mouse(M_OFF, NULL);

    wind_get_grect(win, WF_WORKXYWH, &work);
    wind_get_grect(win, WF_FIRSTXYWH, &rect);
    while (rect.g_w > 0 && rect.g_h > 0) {
        if (rc_intersect(dirty, &rect)) {
            if (rc_intersect(&work, &rect)) {
                short pxy[4];
                pxy[0] = rect.g_x;
                pxy[1] = rect.g_y;
                pxy[2] = rect.g_x + rect.g_w - 1;
                pxy[3] = rect.g_y + rect.g_h - 1;
                vs_clip(vdi_handle, 1, pxy);

                draw_window_contents(&rect);
            }
        }
        wind_get_grect(win, WF_NEXTXYWH, &rect);
    }

    short no_clip[4] = {0, 0, 0, 0};
    vs_clip(vdi_handle, 0, no_clip);
    graf_mouse(M_ON, NULL);
    wind_update(END_UPDATE);
}

static void redraw_window(void)
{
    if (win_handle <= 0) return;
    GRECT dirty;
    wind_get_grect(win_handle, WF_WORKXYWH, &dirty);
    do_redraw(win_handle, &dirty);
}

int main(void)
{
    short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
    short work_out[57];
    short dummy_h, dummy_v;

    if (appl_init() < 0) {
        return 1;
    }

    vdi_handle = graf_handle(&char_w, &char_h, &box_w, &box_h);
    v_opnvwk(work_in, &vdi_handle, work_out);

    vst_alignment(vdi_handle, TA_LEFT, TA_TOP, &dummy_h, &dummy_v);

    short cw, ch, cellw, cellh;
    vst_height(vdi_handle, char_h, &cw, &ch, &cellw, &cellh);
    row_height = (cellh > 0) ? cellh : (box_h > 0 ? box_h : 16);
    char_w = (cellw > 0) ? cellw : (box_w > 0 ? box_w : 8);
    char_h = (ch > 0) ? ch : (box_h > 0 ? box_h : 16);

    total_pkgs = pkg_load_index("/var/cache/yum.tt/index.tsv", pkgs, MAX_PKGS);
    if (total_pkgs < 0) {
        total_pkgs = 0;
    }

    int inst_ret = pkg_apply_installed("/var/cache/yum.tt/installed.tsv", pkgs, total_pkgs, MAX_PKGS);
    if (inst_ret < 0) {
        installed_unknown = 1;
    } else {
        installed_unknown = 0;
        total_pkgs = inst_ret;
    }

    n_installed = 0;
    n_newer = 0;
    if (!installed_unknown) {
        for (int i = 0; i < total_pkgs; i++) {
            if (pkgs[i].state != PKG_NOT_INSTALLED) {
                n_installed++;
            }
            if (pkgs[i].state == PKG_NEWER) {
                n_newer++;
            }
        }
    }

    find_text[0] = '\0';
    rebuild_visible_list();

    short desk_x, desk_y, desk_w, desk_h;
    wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);

    short win_w = (desk_w * 7) / 10;
    short win_h = (desk_h * 7) / 10;
    short win_x = desk_x + (desk_w - win_w) / 2;
    short win_y = desk_y + (desk_h - win_h) / 2;

    short parts = NAME | CLOSER | MOVER | SIZER | FULLER | INFO | UPARROW | DNARROW | VSLIDE;
    win_handle = wind_create(parts, desk_x, desk_y, desk_w, desk_h);
    if (win_handle < 0) {
        pkg_free(pkgs, total_pkgs);
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set_str(win_handle, WF_NAME, win_title);
    update_info_line();
    wind_open(win_handle, win_x, win_y, win_w, win_h);
    graf_mouse(ARROW, NULL);

    recalc_layout();
    update_slider();

    short running = 1;
    while (running) {
        short msg[8];
        short mx = 0, my = 0, mbutton = 0, kstate = 0, key = 0, mclicks = 0;
        short events = evnt_multi(
            MU_MESAG | MU_BUTTON | MU_KEYBD | MU_TIMER,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            100,
            &mx, &my,
            &mbutton,
            &kstate,
            &key,
            &mclicks
        );

        if (events & MU_MESAG) {
            switch (msg[0]) {
            case WM_REDRAW:
                if (msg[3] == win_handle) {
                    GRECT dirty;
                    dirty.g_x = msg[4];
                    dirty.g_y = msg[5];
                    dirty.g_w = msg[6];
                    dirty.g_h = msg[7];
                    do_redraw(win_handle, &dirty);
                }
                break;
            case WM_TOPPED:
                if (msg[3] == win_handle) {
                    wind_set(win_handle, WF_TOP, 0, 0, 0, 0);
                }
                break;
            case WM_CLOSED:
                if (msg[3] == win_handle) {
                    running = 0;
                }
                break;
            case WM_MOVED:
            case WM_SIZED:
                if (msg[3] == win_handle) {
                    wind_set(win_handle, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
                    recalc_layout();
                    update_slider();
                }
                break;
            case WM_FULLED:
                if (msg[3] == win_handle) {
                    short curr[4], full[4], prev[4];
                    wind_get(win_handle, WF_CURRXYWH, &curr[0], &curr[1], &curr[2], &curr[3]);
                    wind_get(win_handle, WF_FULLXYWH, &full[0], &full[1], &full[2], &full[3]);
                    wind_get(win_handle, WF_PREVXYWH, &prev[0], &prev[1], &prev[2], &prev[3]);
                    if (curr[0] == full[0] && curr[1] == full[1] && curr[2] == full[2] && curr[3] == full[3]) {
                        wind_set(win_handle, WF_CURRXYWH, prev[0], prev[1], prev[2], prev[3]);
                    } else {
                        wind_set(win_handle, WF_CURRXYWH, full[0], full[1], full[2], full[3]);
                    }
                    recalc_layout();
                    update_slider();
                }
                break;
            case WM_ARROWED:
                if (msg[3] == win_handle) {
                    int old_scroll = scroll_top;
                    int page = (visible_rows > 1) ? visible_rows - 1 : 1;
                    switch (msg[4]) {
                    case WA_UPPAGE:
                        scroll_top -= page;
                        break;
                    case WA_DNPAGE:
                        scroll_top += page;
                        break;
                    case WA_UPLINE:
                        scroll_top -= 1;
                        break;
                    case WA_DNLINE:
                        scroll_top += 1;
                        break;
                    }
                    int max_scroll = (visible_count > visible_rows) ? visible_count - visible_rows : 0;
                    if (scroll_top < 0) scroll_top = 0;
                    if (scroll_top > max_scroll) scroll_top = max_scroll;
                    if (scroll_top != old_scroll) {
                        update_slider();
                        redraw_window();
                    }
                }
                break;
            case WM_VSLID:
                if (msg[3] == win_handle && visible_count > visible_rows) {
                    int max_scroll = visible_count - visible_rows;
                    int new_scroll = (int)(((long)(msg[4] - 1) * max_scroll + 499) / 999);
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    if (new_scroll != scroll_top) {
                        scroll_top = new_scroll;
                        update_slider();
                        redraw_window();
                    }
                }
                break;
            }
        }

        if (events & MU_BUTTON) {
            if (mbutton & 1) {
                short wx, wy, ww, wh;
                wind_get(win_handle, WF_WORKXYWH, &wx, &wy, &ww, &wh);
                if (mx >= wx && mx < wx + ww && my >= wy && my < wy + wh) {
                    if (my >= wy && my < wy + row_height) {
                        update_toolbar_geometry(wx, wy);
                        for (int i = 0; i < 4; i++) {
                            if (mx >= btn_rect[i].g_x && mx < btn_rect[i].g_x + btn_rect[i].g_w) {
                                if (current_filter != i) {
                                    current_filter = i;
                                    scroll_top = 0;
                                    rebuild_visible_list();
                                    redraw_window();
                                }
                                break;
                            }
                        }
                    } else if (my >= wy + row_height + 2) {
                        int row = (my - (wy + row_height + 2)) / row_height;
                        int idx = scroll_top + row;
                        if (idx >= 0 && idx < visible_count) {
                            if (selected_index != idx) {
                                selected_index = idx;
                                redraw_window();
                            }
                        } else {
                            if (selected_index != -1) {
                                selected_index = -1;
                                redraw_window();
                            }
                        }
                    }
                }
            }
        }

        if (events & MU_KEYBD) {
            int ascii = key & 0xFF;
            int scan = (key >> 8) & 0xFF;

            if (ascii == 8 || scan == 0x0E) {
                size_t len = strlen(find_text);
                if (len > 0) {
                    find_text[len - 1] = '\0';
                    scroll_top = 0;
                    rebuild_visible_list();
                    redraw_window();
                }
            } else if (ascii == 27 || scan == 0x01) {
                if (find_text[0] != '\0') {
                    find_text[0] = '\0';
                    scroll_top = 0;
                    rebuild_visible_list();
                    redraw_window();
                }
            } else if (ascii >= 32 && ascii <= 126) {
                size_t len = strlen(find_text);
                if (len + 1 < sizeof(find_text)) {
                    find_text[len] = (char)ascii;
                    find_text[len + 1] = '\0';
                    scroll_top = 0;
                    rebuild_visible_list();
                    redraw_window();
                }
            }
        }
    }

    if (win_handle > 0) {
        wind_close(win_handle);
        wind_delete(win_handle);
    }
    pkg_free(pkgs, total_pkgs);
    v_clsvwk(vdi_handle);
    appl_exit();

    return 0;
}
