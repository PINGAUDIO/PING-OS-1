/* PING OS-1: the LIBRARY (SAVE + EDIT), one page to keep and bring back what is made. Three tabs: SOUNDS (the sixteen
 * presets: a track's engine, its effects, modulators and maps, a KIT among them), PATTERNS (the sixteen) and PROJECT.
 * The sixteen slots are drawn as the white keys are laid out, four by four. A white key or SELECT picks a slot,
 * ALGO or OCT- / OCT+ the tab, PRESET or black keys 1-4 the track. EDIT brings in, SAVE keeps; a slot already in use
 * asks for SAVE again, and so does going back to the last save. REC starts over, the track on SOUNDS, every track on
 * PROJECT: a box asks first, REC again is yes, any other button no. HOME (or SAVE + EDIT again) closes it. While it is
 * open the rest of the panel is quiet, but PLAY still starts and stops. */
enum { LIB_SOUNDS, LIB_PATTERNS, LIB_PROJECT, LIB_TABS };
static const char *const LIB_TAB[LIB_TABS] = {"SOUNDS", "PATTERNS", "PROJECT"};
enum { LIBC_NONE, LIBC_SOUND, LIBC_PATTERN, LIBC_REVERT, LIBC_INIT };
#define LIB_FILL RGB(40, 42, 54)                           /* a slot with something in it */
static struct {
    uint8_t open, tab, sel[2], scan_due, confirm, confirm_slot, reverting;
    int8_t dirty;                                          /* the project: 1 changes not saved, 0 all saved, -1 not known yet */
    uint32_t confirm_until, dirty_at;
    const char *name[PJ_PRESETS]; uint8_t eng[PJ_PRESETS];   /* the presets as stored: the model's name, the engine */
} lib;

static unsigned bits16(uint32_t v) { unsigned n = 0; while (v) { n += v & 1u; v >>= 1; } return n; }
/* the presets' names and the patterns there (pj_buf holds a cued pattern until the bar: then later) */
static void lib_scan(void)
{
    if (ch_snapshot_get()->pattern_pending || pj_loading()) { lib.scan_due = 1; return; }
    pre_exist = 0;
    for (unsigned b = 0; b < PJ_PRESETS / PJ_PER_BANK; ++b) {
        int have = pre_bank_read(b);
        const pj_bank *bk = (const pj_bank *)pj_buf.raw;
        for (unsigned k = 0; k < PJ_PER_BANK; ++k) {
            unsigned n = b * PJ_PER_BANK + k;
            if (have && bk->p[k].used) { pre_exist |= (uint16_t)(1u << n); lib.name[n] = preset_name(&bk->p[k]); lib.eng[n] = bk->p[k].engine; }
            else lib.name[n] = 0;
        }
    }
    pat_scan(); lib.scan_due = 0; ui.redraw = 1;
}
/* anything not in the flash: a pattern's edits, or the settings unlike those saved */
static void lib_dirty_check(void)
{
    if (pj_loading() || ch_snapshot_get()->pattern_pending) return;
    int d = ch_pattern_dirty() || pat_unsaved || ch_stash_valid();
    if (!d) { pj_fill_project(); d = pj_hash(&pj_buf, sizeof(pj_project)) != pj_crc[OBJ_PROJECT]; }
    for (unsigned t = 0; t < CH_TRACKS && !d; ++t) { pj_fill_track(t); d = pj_hash(&pj_buf, sizeof(pj_track)) != pj_crc[OBJ_TRK0 + t]; }
    if (lib.dirty != d) ui.redraw = 1;
    lib.dirty = (int8_t)d;
}
static void lib_open(void)
{
    for (unsigned k = 0; k < NKEYS; ++k) if (ui.last_note[k]) { ch_note(ui.track, (unsigned)ui.last_note[k], 0); ui.last_note[k] = 0; }
    for (unsigned t = 0; t < CH_TRACKS; ++t) if (ui.pad_note[t]) { ch_note(t, ui.pad_note[t], 0); ui.pad_note[t] = 0; }
    lib.open = 1; lib.confirm = 0; lib.sel[1] = ui.cur_pat; lib.dirty = -1; lib.dirty_at = 0;
    ui.save_used = 1; ui.held_long[B_EDIT] = 1; ui.held_long[B_SAVE] = 1;   /* (their releases: nothing more) */
    lib_scan(); ui.msg_until = 0; ui.redraw = 1;
}
static void lib_close(void)
{
    lib.open = 0; lib.confirm = 0; ui.msg_until = 0; ui.redraw = 1;
    lcd_fill(0, 0, 240, 240, C_BLACK);
}
/* SAVE (keep) or EDIT (bring in) on the slot or the project */
static void lib_act(int save, uint32_t now)
{
    int again = lib.confirm && (int32_t)(lib.confirm_until - now) > 0;
    if (pj_loading()) { app_say("PROJECT", "LOADING, WAIT"); return; }
    if (lib.tab == LIB_SOUNDS) {
        unsigned n = lib.sel[0];
        if (!save) { if (pre_exist >> n & 1u) preset_load(n); else { char d[4]; int_text((int32_t)n + 1, d); app_say(d, "EMPTY"); } }
        else if ((pre_exist >> n & 1u) && !(again && lib.confirm == LIBC_SOUND && lib.confirm_slot == n)) {
            lib.confirm = LIBC_SOUND; lib.confirm_slot = (uint8_t)n; lib.confirm_until = now + 3000u; ui.redraw = 1; return;
        } else { preset_save(n); lib_scan(); }
    } else if (lib.tab == LIB_PATTERNS) {
        unsigned n = lib.sel[1];
        char nm[8] = "P"; { char d[4]; int_text((int32_t)n + 1, d); text_cat(nm, d); }
        if (!save) pattern_select(n, 1);
        else if ((int)n == ui.cur_pat) { if (!pat_flush(1)) app_say(nm, "ALREADY SAVED"); }
        else if ((pat_exist >> n & 1u) && !(again && lib.confirm == LIBC_PATTERN && lib.confirm_slot == n)) {
            lib.confirm = LIBC_PATTERN; lib.confirm_slot = (uint8_t)n; lib.confirm_until = now + 3000u; ui.redraw = 1; return;
        } else { pattern_copy(n); pat_scan(); }
    } else {
        if (save) pj_save();
        else if (plat_store_size(OBJ_PROJECT) <= (int)sizeof(pj_hdr)) app_say("PROJECT", "NOTHING SAVED YET");
        else if (ch_snapshot_get()->pattern_pending) app_say("PROJECT", "WAIT FOR THE BAR");
        else if (!(again && lib.confirm == LIBC_REVERT)) { lib.confirm = LIBC_REVERT; lib.confirm_until = now + 3000u; ui.redraw = 1; return; }
        else {                                             /* back to the last save: the edits dropped, the project read again */
            if (ch_stash_valid()) ch_stash_drop();
            ui.stash_no = -1; pat_unsaved = 0;
            pj_load_begin(); lib.reverting = 1; app_say("PROJECT", "LOADING");
        }
        lib.dirty_at = 0;
    }
    lib.confirm = 0; ui.redraw = 1;
}
static void lib_lights(uint32_t btn, uint32_t keys)
{
    const ch_snapshot *sn = ch_snapshot_get();
    uint32_t lit = (1u << B_SAVE) | (1u << B_EDIT), klit = 0, kdim = 0;
    if (lib.tab != LIB_PATTERNS) lit |= 1u << B_REC;
    if (sn->playing) lit |= LED_PLAY_GREEN;
    if (lib.tab != LIB_PROJECT) {
        uint32_t have = lib.tab == LIB_SOUNDS ? pre_exist : (uint32_t)pat_exist | 1u << ui.cur_pat;
        for (unsigned w = 0; w < 16; ++w) { if (w == lib.sel[lib.tab]) klit |= 1u << WK[w]; else if (have >> w & 1u) kdim |= 1u << WK[w]; }
    }
    if (lib.tab == LIB_SOUNDS) for (unsigned t = 0; t < CH_TRACKS; ++t) { if (t == ui.track) klit |= 1u << BK[t]; else kdim |= 1u << BK[t]; }
    plat_lights(lit | btn, klit | keys, 0, kdim, 1);
    plat_lights_mid((1u << NB) - 1u, 0);
}
static void lib_tab(int d)
{
    lib.tab = (uint8_t)((lib.tab + LIB_TABS + (d > 0 ? 1 : -1)) % LIB_TABS);
    lib.confirm = 0; lib.dirty_at = 0; ui.msg_until = 0; ui.redraw = 1;
}
/* a frame's input while the page is open: all of it here */
static void lib_input(uint32_t pressed, uint32_t btn, uint32_t keys, uint32_t now)
{
    if (lib.scan_due) lib_scan();
    if (lib.reverting && !pj_loading()) { lib.reverting = 0; lib.dirty_at = 0; lib_scan(); app_say("PROJECT", "BACK TO THE LAST SAVE"); }
    if (lib.confirm && (int32_t)(now - lib.confirm_until) > 0) { lib.confirm = 0; ui.redraw = 1; }
    if (lib.confirm == LIBC_INIT && (pressed & ~(1u << B_REC))) {   /* the INIT box: any other button is no, and nothing more */
        if (pressed >> B_HOME & 1u) { ui.home_used = 1; ui.held_long[B_HOME] = 1; }
        ui.save_used = 1; lib.confirm = 0; pressed = 0; ui.redraw = 1;
    }
    int both = ((pressed >> B_SAVE & 1u) && (btn >> B_EDIT & 1u)) || ((pressed >> B_EDIT & 1u) && (btn >> B_SAVE & 1u));
    if ((pressed >> B_HOME & 1u) || both) {
        if (pressed >> B_HOME & 1u) { ui.home_used = 1; ui.held_long[B_HOME] = 1; }
        ui.save_used = 1; ui.held_long[B_EDIT] = 1; ui.held_long[B_SAVE] = 1;
        lib_close();
    } else {
        if (pressed >> B_SAVE & 1u) lib_act(1, now);
        if (pressed >> B_EDIT & 1u) lib_act(0, now);
        if ((pressed >> B_REC & 1u) && lib.tab != LIB_PATTERNS) {   /* INIT: the track (SOUNDS) or all of them (PROJECT); a box asks first */
            ui.rec_used = 1; ui.redraw = 1;
            if (lib.confirm != LIBC_INIT) { lib.confirm = LIBC_INIT; lib.confirm_until = now + 0x3fffffffu; }
            else {
                lib.confirm = 0; lib.dirty_at = 0;
                if (lib.tab == LIB_SOUNDS) { init_track(ui.track); char n[4] = {'T', (char)('1' + ui.track), 0}; app_say("INIT", n); }
                else { init_all(); app_say("INIT", "ALL TRACKS"); }
            }
        }
        if (pressed >> B_OCTDN & 1u) lib_tab(-1);
        if (pressed >> B_OCTUP & 1u) lib_tab(1);
    }
    if (pressed >> B_PLAY & 1u) { unsigned p = !ch_snapshot_get()->playing; ch_transport(p); }
    for (unsigned k = 0; k < NKEYS; ++k) {
        uint32_t bit = 1u << k;
        if (!(keys & bit) || (ui.keys & bit) || !lib.open) continue;
        for (int w = 0; w < 16; ++w) if (WK[w] == k && lib.tab != LIB_PROJECT) { lib.sel[lib.tab] = (uint8_t)w; lib.confirm = 0; ui.msg_until = 0; ui.redraw = 1; }
        for (int b = 0; b < CH_TRACKS; ++b) if (BK[b] == k && lib.tab == LIB_SOUNDS) { ui.track = (uint8_t)b; lib.confirm = 0; ui.redraw = 1; }
    }
    ui.keys = keys;
    int32_t s = plat_enc(EN_SELECT);
    if (s && lib.open && lib.tab != LIB_PROJECT) {
        lib.sel[lib.tab] = (uint8_t)(((int)lib.sel[lib.tab] + (int)s % 16 + 16) % 16);
        lib.confirm = 0; ui.msg_until = 0; ui.redraw = 1;
    }
    for (unsigned i = 0; i < 6; ++i) {
        int32_t e = plat_enc(ENC_OF[i]);
        if (!e || !lib.open) continue;
        if (i == 4) lib_tab(e);
        if (i == 5) { int t = ((int)ui.track + (e > 0 ? 1 : -1) + CH_TRACKS) % CH_TRACKS; ui.track = (uint8_t)t; lib.confirm = 0; ui.redraw = 1; }
    }
    if (lib.open) lib_lights(btn, keys);
}

/* ---- the page ---- */
static void lib_fit(char *out, const char *s, const felucca_font_t *f, int32_t w)
{
    unsigned n = 0; while (s[n] && n < 23) { out[n] = s[n]; ++n; } out[n] = 0;
    while (n && text_w(f, out) > w) out[--n] = 0;
}
/* a button's name in a light box, then what it does */
static int32_t lib_hint(int32_t x, int32_t y, const char *key, const char *what, int on)
{
    int32_t w = text_w(&FONT_T, key) + 6;
    cv_rect(x, y, w, 14, on ? C_GRAY : C_LINE);
    cv_text(x + 3, y + 1, &FONT_T, key, C_BLACK);
    return cv_text(x + w + 4, y + 1, &FONT_T, what, on ? C_WHITE : C_DIM) + 8;
}
static void lib_cell(int32_t x, int32_t y, unsigned n, int have, int sel, const char *name, uint16_t ncol)
{
    const int32_t w = 58, h = 37;
    cv_rect(x, y, w, h, sel ? C_WHITE : have ? LIB_FILL : C_LINE);
    int32_t b = sel ? 2 : 1;
    cv_rect(x + b, y + b, w - 2 * b, h - 2 * b, have ? LIB_FILL : C_BLACK);
    char d[4]; int_text((int32_t)n + 1, d);
    cv_text(x + 5, y + 3, &FONT_T, d, sel ? C_WHITE : have ? C_GRAY : C_DIM);
    if (name) {
        char nm[24];
        const felucca_font_t *f = text_w(&FONT_XS, name) <= w - 9 ? &FONT_XS : &FONT_T;
        lib_fit(nm, name, f, w - 9);
        cv_text(x + 5, y + 18, f, nm, ncol);
    }
}
/* a pattern slot's state, for its cell and the line below */
static const char *lib_pat_state(unsigned n, uint16_t *col)
{
    *col = C_HI;
    if ((int)n == ui.cur_pat) { *col = C_ACC; return ch_snapshot_get()->pattern_pending ? "NEXT" : "PLAYING"; }
    if (ui.stash_no == (int)n) { *col = C_ACC_HI; return "EDITED"; }
    return (pat_exist >> n & 1u) ? "SAVED" : 0;
}
static void lib_draw(uint32_t now)
{
    if (lib.tab == LIB_PROJECT && (lib.dirty < 0 || now - lib.dirty_at > 500u)) { lib.dirty_at = now; lib_dirty_check(); }
    /* the tabs */
    cv_begin(240, 40, C_BLACK);
    for (unsigned i = 0; i < LIB_TABS; ++i) {
        int32_t w = text_w(&FONT_B, LIB_TAB[i]), x = 40 + 80 * (int32_t)i - w / 2;
        cv_text(x, 9, &FONT_B, LIB_TAB[i], i == lib.tab ? C_WHITE : C_DIM);
        if (i == lib.tab) cv_rect(x, 29, w, 2, C_ACC);
    }
    cv_rect(0, 38, 240, 1, C_LINE);
    cv_blit(0, 0);
    /* the slots, or the project */
    for (unsigned band = 0; band < 2; ++band) {
        cv_begin(240, 78, C_BLACK);
        if (lib.confirm == LIBC_INIT) {                    /* the INIT box, in place of the slots until it is answered */
            if (!band) {
                char q[10] = "INIT T1?"; q[6] = (char)('1' + ui.track);
                const char *qs = lib.tab == LIB_SOUNDS ? q : "INIT ALL TRACKS?";
                cv_rect(20, 26, 200, 52, C_WHITE); cv_rect(21, 27, 198, 51, C_BLACK);
                cv_text(120 - text_w(&FONT_B, qs) / 2, 50, &FONT_B, qs, C_WHITE);
            } else {
                cv_rect(20, 0, 200, 46, C_WHITE); cv_rect(21, 0, 198, 45, C_BLACK);
                int32_t x = lib_hint(54, 14, "REC", "YES", 1);
                lib_hint(x + 6, 14, "OTHER", "CANCEL", 1);
            }
        } else if (lib.tab == LIB_PROJECT) {
            char t[40], d[8];
            if (!band) {
                const char *st = lib.dirty < 0 ? "CHECKING" : lib.dirty ? "UNSAVED CHANGES" : "ALL SAVED";
                cv_text(120 - text_w(&FONT_B, "PROJECT") / 2, 22, &FONT_B, "PROJECT", C_WHITE);
                cv_text(120 - text_w(&FONT_B, st) / 2, 48, &FONT_B, st, lib.dirty > 0 ? C_ACC : C_GRAY);
            } else {
                t[0] = 0; text_cat(t, "P"); int_text(ui.cur_pat + 1, d); text_cat(t, d); text_cat(t, " PLAYING   ");
                int_text((int32_t)(ch_tempo() + 0.5f), d); text_cat(t, d); text_cat(t, " BPM");
                cv_text(120 - text_w(&FONT_XS, t) / 2, 10, &FONT_XS, t, C_GRAY);
                t[0] = 0; int_text((int32_t)bits16(pat_exist), d); text_cat(t, d); text_cat(t, bits16(pat_exist) == 1 ? " PATTERN   " : " PATTERNS   ");
                int_text((int32_t)bits16(pre_exist), d); text_cat(t, d); text_cat(t, bits16(pre_exist) == 1 ? " SOUND SAVED" : " SOUNDS SAVED");
                cv_text(120 - text_w(&FONT_XS, t) / 2, 30, &FONT_XS, t, C_GRAY);
            }
        } else {
            for (unsigned r = 0; r < 2; ++r)
                for (unsigned c = 0; c < 4; ++c) {
                    unsigned n = (band * 2 + r) * 4 + c;
                    int32_t x = 1 + (int32_t)c * 60, y = (int32_t)r * 39;
                    int sel = n == lib.sel[lib.tab];
                    if (lib.tab == LIB_SOUNDS) lib_cell(x, y, n, pre_exist >> n & 1u, sel, lib.name[n], C_HI);
                    else { uint16_t col; const char *st = lib_pat_state(n, &col); lib_cell(x, y, n, st != 0, sel, st && st[0] != 'S' ? st : 0, col); }
                }
        }
        cv_blit(0, band ? 120 : 42);
    }
    /* what the slot holds, or a message; then what the buttons do */
    cv_begin(240, 42, C_BLACK);
    char t[40], d[8];
    int conf = lib.confirm && (int32_t)(lib.confirm_until - now) > 0;
    if (conf) {
        t[0] = 0;
        if (lib.confirm == LIBC_REVERT) text_cat(t, "EDIT AGAIN: LOSE CHANGES");
        else if (lib.confirm == LIBC_INIT) { }             /* (its box is over the slots) */
        else { text_cat(t, "SAVE AGAIN: REPLACE "); if (lib.confirm == LIBC_PATTERN) text_cat(t, "P"); int_text(lib.confirm_slot + 1, d); text_cat(t, d); }
        cv_text(6, 3, &FONT_B, t, C_ACC);
    } else if (now < ui.msg_until) {
        int32_t x = cv_text(6, 3, &FONT_B, ui.msg[0], C_WHITE);
        char m[24]; lib_fit(m, ui.msg[1], &FONT_B, 234 - x - 8); cv_text(x + 8, 3, &FONT_B, m, C_HI);
    } else if (lib.tab == LIB_SOUNDS) {
        unsigned n = lib.sel[0];
        int_text((int32_t)n + 1, d);
        int32_t x = cv_text(6, 3, &FONT_B, d, C_WHITE);
        if (lib.name[n]) {
            x = cv_text(x + 10, 3, &FONT_B, lib.name[n], C_WHITE);
            const ch_device_desc *dd = lib.eng[n] < CH_DEVICES ? ch_device_info(lib.eng[n]) : 0;
            if (dd && lib.eng[n] != CH_DEV_KIT && lib.eng[n] != CH_DEV_BYPASS && x + 8 + text_w(&FONT_XS, dd->name) < 200) cv_text(x + 8, 4, &FONT_XS, dd->name, C_DIM);
        } else cv_text(x + 10, 3, &FONT_B, "EMPTY", C_DIM);
        char tn[4] = {'T', (char)('1' + ui.track), 0};
        cv_text(234 - text_w(&FONT_B, tn), 3, &FONT_B, tn, TRACK_COL[ui.track]);
    } else if (lib.tab == LIB_PATTERNS) {
        unsigned n = lib.sel[1]; uint16_t col; const char *st = lib_pat_state(n, &col);
        t[0] = 0; text_cat(t, "P"); int_text((int32_t)n + 1, d); text_cat(t, d);
        int32_t x = cv_text(6, 3, &FONT_B, t, C_WHITE);
        cv_text(x + 10, 3, &FONT_B, st ? st : "EMPTY", st ? col : C_DIM);
    } else cv_text(6, 3, &FONT_B, lib.dirty > 0 ? "SAVE TO KEEP YOUR CHANGES" : "", C_GRAY);
    {
        int32_t x = 3, y = 25;
        if (lib.tab == LIB_SOUNDS) {
            int have = pre_exist >> lib.sel[0] & 1u;
            char into[12] = "LOAD T"; into[6] = (char)('1' + ui.track); into[7] = 0;
            x = lib_hint(x, y, "EDIT", into, have);
            x = lib_hint(x, y, "SAVE", have ? "REPLACE" : "STORE", 1);
            x = lib_hint(x, y, "REC", "INIT", 1);
        } else if (lib.tab == LIB_PATTERNS) {
            unsigned n = lib.sel[1];
            x = lib_hint(x, y, "EDIT", (int)n == ui.cur_pat ? "PLAYING" : "PLAY", (int)n != ui.cur_pat);
            x = lib_hint(x, y, "SAVE", (int)n == ui.cur_pat ? "SAVE" : "COPY HERE", 1);
        } else {
            x = lib_hint(x, y, "EDIT", "LAST SAVE", 1);
            x = lib_hint(x, y, "SAVE", "SAVE", 1);
            x = lib_hint(x, y, "REC", "INIT ALL", 1);
        }
        lib_hint(x, y, "HOME", "", 1);                    /* (closes: the row has no room left to say so) */
    }
    cv_blit(0, 198);
}
