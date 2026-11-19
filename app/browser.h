/* PING OS-1: the BROWSER (a SEL tap), one page to hear and choose what a track or an FX slot holds. Opened on a
 * track: two tabs. ENGINES is two columns side by side, the six engines on the left and, on the right, the models
 * (or samples) of the engine the track has: ALGO goes up and down the engines, PRESET up and down the models, and
 * SELECT through all of it, on from an engine's last model into the next engine. PRESETS is the sixteen saved
 * sounds (SELECT). EDIT turns the tabs. Opened on an FX slot (FX on that slot's page): four tabs, the slots, each a list of
 * the effects; ALGO or SELECT goes through them, FX to the next slot. OCT- / OCT+ the track.
 * The line the bar is on is ON the track while it is there: it is heard, with the keys or the sequencer. SAVE keeps
 * it and closes; SEL or HOME closes and the track is as it was when the page opened (so is a track left by OCT).
 * While it is open the rest of the panel is quiet, but PLAY still starts and stops. */
enum { BRW_ROWS = 6, BRW_MAX = 40 };
typedef struct { uint8_t dev, byp, mode[CH_PARAMS]; float param[CH_PARAMS]; } brw_slot_t;
static struct {
    uint8_t open, fx, tab, n;                              /* fx: opened on an FX slot (the tabs are the slots) */
    uint8_t sel, top, changed;                             /* changed: something was tried, so closing has something to undo */
    uint8_t dev[BRW_MAX];                                  /* the FX list: each line's device */
    const char *was;                                       /* what was there when it opened */
    brw_slot_t fxwas[CH_FX];                               /* an FX slot's page: the four slots as they were */
} brw;
static pj_preset brw_was CH_POOL;                          /* a track's page: its sound as it was */

static const uint8_t BRW_ENG[6] = {CH_DEV_DRUM, CH_DEV_KIT, CH_DEV_SYNTH, CH_DEV_POLY, CH_DEV_GRAIN, CH_DEV_BYPASS};
static unsigned brw_tabs(void) { return brw.fx ? CH_FX : 2u; }
static unsigned brw_slot(void) { return CH_FX1 + brw.tab; }
/* the engine's first parameter when it is a list of names (a model, a sample), or 0 */
static const ch_param_desc *brw_model(void)
{
    const ch_track_state *ts = ch_track_get(ui.track);
    if (ts->device[0] == CH_DEV_BYPASS || ts->device[0] == CH_DEV_KIT) return 0;
    const ch_device_desc *d = ch_device_info(ts->device[0]);
    return d && d->params && d->param[0].kind == CH_SELECT && d->param[0].choices ? &d->param[0] : 0;
}
static const char *brw_tab_name(unsigned i)
{
    if (brw.fx) return SLOT_NAME[CH_FX1 + i];
    return i == 0 ? "ENGINES" : "PRESETS";
}
/* the line on the track now, or -1 */
static int brw_current(void)
{
    if (brw.fx) {
        unsigned cur = ch_track_get(ET)->device[brw_slot()];
        for (unsigned i = 0; i < brw.n; ++i) if (brw.dev[i] == cur) return (int)i;
        return -1;
    }
    const ch_track_state *ts = ch_track_get(ui.track);
    if (brw.tab == 0) return brw_model() ? (int)(ts->param[ch_row(CH_MATERIAL, ts->device[0])][0] + 0.5f) : -1;
    return -1;
}
static void brw_show(void)
{
    if (brw.sel < brw.top) brw.top = brw.sel;
    if (brw.sel >= brw.top + BRW_ROWS) brw.top = (uint8_t)(brw.sel - BRW_ROWS + 1);
    if (brw.n > BRW_ROWS && brw.top > brw.n - BRW_ROWS) brw.top = (uint8_t)(brw.n - BRW_ROWS);
    if (brw.n <= BRW_ROWS) brw.top = 0;
}
/* the list for the tab; the bar on the line the track has (keep: the bar stays where it was, the PRESET tab's) */
static void brw_list(void)
{
    brw.n = 0;
    if (brw.fx) {
        brw.dev[brw.n++] = CH_DEV_BYPASS;
        for (unsigned k = 0; k < CH_FX_TYPES && brw.n < BRW_MAX; ++k) brw.dev[brw.n++] = FX_ORDER[k];   /* (by kind) */
    } else if (brw.tab == 0) { const ch_param_desc *m = brw_model(); brw.n = m ? m->steps : 0; }
    else brw.n = PJ_PRESETS;
    int c = brw_current();
    brw.sel = (uint8_t)(c >= 0 && c < (int)brw.n ? c : 0);
    brw.top = (uint8_t)(brw.sel >= BRW_ROWS ? brw.sel - BRW_ROWS / 2 : 0);
    brw_show();
    ui.redraw = 1;
}
static const char *brw_name(unsigned i, int *have)
{
    *have = 1;
    if (brw.fx) return brw.dev[i] == CH_DEV_BYPASS ? "EMPTY" : ch_device_info(brw.dev[i])->name;
    if (brw.tab == 0) { const ch_param_desc *m = brw_model(); const char *s = m && m->choices[i] ? m->choices[i] : "-"; *have = !(s[0] == '-' && !s[1]); return s; }
    *have = pre_exist >> i & 1u;
    return lib.name[i] ? lib.name[i] : "EMPTY";
}
static void brw_notes_off(void)
{
    for (unsigned k = 0; k < NKEYS; ++k) if (ui.last_note[k]) { ch_note(ui.track, (unsigned)ui.last_note[k], 0); ui.last_note[k] = 0; }
    for (unsigned t = 0; t < CH_TRACKS; ++t) if (ui.pad_note[t]) { ch_note(t, ui.pad_note[t], 0); ui.pad_note[t] = 0; }
}
/* what is there now, kept to go back to */
static void brw_keep(void)
{
    if (brw.fx) {
        const ch_track_state *ts = ch_track_get(ET);
        for (unsigned f = 0; f < CH_FX; ++f) {
            brw_slot_t *w = &brw.fxwas[f];
            w->dev = ts->device[CH_FX1 + f]; w->byp = ts->bypass[CH_FX1 + f];
            for (unsigned k = 0; k < CH_PARAMS; ++k) { w->param[k] = ts->param[CH_MROWS + f][k]; w->mode[k] = ts->mode[CH_MROWS + f][k]; }
        }
        brw.was = 0;
    } else { preset_capture(&brw_was, ui.track); brw.was = material_name(ui.track); }
    brw.changed = 0;
}
/* and back to it */
static void brw_undo(void)
{
    if (!brw.changed) return;
    brw_notes_off();
    if (brw.fx) {
        const ch_track_state *ts = ch_track_get(ET);
        for (unsigned f = 0; f < CH_FX; ++f) {
            const brw_slot_t *w = &brw.fxwas[f];
            if (ts->device[CH_FX1 + f] == w->dev) continue;        /* (only a slot's effect is ever tried here) */
            ch_device_set(ET, CH_FX1 + f, w->dev); ch_bypass_set(ET, CH_FX1 + f, w->byp);
            if (w->dev == CH_DEV_BYPASS) continue;
            const ch_device_desc *d = ch_device_info(w->dev);
            for (unsigned k = 0; k < d->params && k < CH_PARAMS; ++k) { ch_param_set(ET, CH_FX1 + f, k, w->param[k]); if (d->param[k].modes) ch_mode_set(ET, CH_FX1 + f, k, w->mode[k]); }
        }
    } else preset_apply(&brw_was, ui.track, 1);
    brw.changed = 0;
}
static void brw_open(void)
{
    brw_notes_off();
    brw.open = 1;
    brw.fx = ui.view == V_DEVICE && ui.slot >= CH_FX1 && ui.slot < CH_FX1 + CH_FX;
    if (!brw.fx) { ui.mfx = 0; lib_scan(); }
    brw.tab = brw.fx ? (uint8_t)(ui.slot - CH_FX1) : 0;
    brw_keep(); brw_list(); ui.msg_until = 0; ui.redraw = 1;
}
static void brw_close(int keep)
{
    if (keep) app_say("BROWSER", brw.changed ? "KEPT" : "NOTHING CHANGED");
    else { int ch = brw.changed; brw_undo(); if (ch) app_say("BROWSER", "AS IT WAS"); else ui.msg_until = 0; }
    brw_notes_off();
    ui.page[brw.fx ? brw_slot() : 0] = 0;
    brw.open = 0; ui.redraw = 1;
    lcd_fill(0, 0, 240, 240, C_BLACK);
}
static void brw_set_tab(unsigned tab)
{
    if (tab == brw.tab) return;
    brw.tab = (uint8_t)(tab % brw_tabs());
    if (brw.fx) { ui.slot = (uint8_t)brw_slot(); ui.fxpos = brw.tab; }
    ui.msg_until = 0; brw_list();
}
/* the bar's line onto the track, or into the slot: to be heard */
static void brw_try(void)
{
    if (!brw.n) return;
    unsigned i = brw.sel;
    brw_notes_off();
    if (brw.fx) {
        unsigned slot = brw_slot();
        if (ch_track_get(ET)->device[slot] == brw.dev[i]) return;
        ch_device_set(ET, slot, brw.dev[i]); ch_bypass_set(ET, slot, 0);
    } else if (brw.tab == 0) {
        if (!brw_model()) return;
        unsigned dev = ch_track_get(ui.track)->device[0];
        ch_param_set(ui.track, CH_MATERIAL, 0, (float)i);
        if (dev == CH_DEV_SYNTH) synth_presets(ui.track, i);
    } else {
        if (!(pre_exist >> i & 1u)) return;
        preset_load(i); ui.msg_until = 0;
    }
    brw.changed = 1; ui.redraw = 1;
}
/* a track's engine, d up or down the six (the ENGINES tab opened first): on the track, heard; its models listed.
 * Returns 0 at the list's end */
static int brw_engine(int d)
{
    brw_set_tab(0);
    int e = 0;
    for (int i = 0; i < 6; ++i) if (BRW_ENG[i] == ch_track_get(ui.track)->device[0]) e = i;
    int to = e + d; to = to < 0 ? 0 : to > 5 ? 5 : to;
    if (to == e) return 0;
    brw_notes_off();
    ch_device_set(ui.track, 0, BRW_ENG[to]); ch_bypass_set(ui.track, 0, 0);
    if (BRW_ENG[to] == CH_DEV_SYNTH) synth_presets(ui.track, model_of(ui.track, CH_DEV_SYNTH));
    brw.changed = 1; ui.msg_until = 0;
    brw_list();
    return 1;
}
/* the bar d lines along the tab's list (the tab opened first if it is another), and that line tried. cross (the
 * ENGINES tab): past the list's end it goes on into the next engine, at its first model (back: the last of the one before) */
static void brw_move(unsigned tab, int d, int cross)
{
    brw_set_tab(tab);
    cross = cross && !brw.fx && brw.tab == 0;
    int s = (int)brw.sel + d;
    if (cross && (!brw.n || s < 0 || s >= (int)brw.n)) {
        if (!brw_engine(d > 0 ? 1 : -1) || !brw.n) return;
        s = d > 0 ? 0 : (int)brw.n - 1;
        brw.sel = (uint8_t)s; brw_show(); ui.redraw = 1;
        brw_try();
        return;
    }
    if (!brw.n) { ui.redraw = 1; return; }
    s = s < 0 ? 0 : s >= (int)brw.n ? (int)brw.n - 1 : s;
    if (s == (int)brw.sel) return;
    brw.sel = (uint8_t)s; brw_show();
    ui.msg_until = 0; ui.redraw = 1;
    brw_try();
}
static void brw_track(int d)
{
    if (ui.mfx) return;
    brw_undo();                                            /* what was only tried on the track left: gone */
    brw_notes_off();
    ui.track = (uint8_t)(((int)ui.track + (d > 0 ? 1 : -1) + CH_TRACKS) % CH_TRACKS);
    brw_keep(); ui.msg_until = 0; brw_list();
}
static void brw_lights(uint32_t btn, uint32_t keys)
{
    uint32_t lit = (1u << B_SEL) | (brw.changed ? 1u << B_SAVE : 0);
    if (ch_snapshot_get()->playing) lit |= LED_PLAY_GREEN;
    plat_lights(lit | btn, keys, 0, 0, 1);
    plat_lights_mid((1u << NB) - 1u, 0);
}
/* a frame's input while the page is open: all of it here */
static void brw_input(uint32_t pressed, uint32_t btn, uint32_t keys, uint32_t now)
{
    (void)now;
    if (lib.scan_due) lib_scan();
    if ((pressed >> B_HOME & 1u) || (pressed >> B_SEL & 1u) || (pressed >> B_SAVE & 1u)) {
        if (pressed >> B_HOME & 1u) { ui.home_used = 1; ui.held_long[B_HOME] = 1; }
        ui.sel_used = 1; ui.save_used = 1; ui.held_long[B_SAVE] = 1;
        brw_close(pressed >> B_SAVE & 1u);
    } else {
        if (pressed >> B_EDIT & 1u) { ui.held_long[B_EDIT] = 1; if (!brw.fx) brw_set_tab((brw.tab + 1u) % 2u); }
        if (pressed >> B_FX & 1u) { ui.held_long[B_FX] = 1; if (brw.fx) brw_set_tab((brw.tab + 1u) % CH_FX); }
        if (pressed >> B_OCTDN & 1u) brw_track(-1);
        if (pressed >> B_OCTUP & 1u) brw_track(1);
    }
    if (pressed >> B_PLAY & 1u) { unsigned p = !ch_snapshot_get()->playing; ch_transport(p); }
    for (unsigned k = 0; k < NKEYS; ++k) {                 /* the keys play the track: what is on it, heard */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit) && brw.open) {
            int32_t n = 53 + (int32_t)k + 12 * ui.oct; n = n < 1 ? 1 : n > 127 ? 127 : n;
            ui.last_note[k] = n; ch_note(ui.track, (unsigned)n, 100);
        } else if (!(keys & bit) && (ui.keys & bit) && ui.last_note[k]) { ch_note(ui.track, (unsigned)ui.last_note[k], 0); ui.last_note[k] = 0; }
    }
    ui.keys = keys;
    int32_t s = plat_enc(EN_SELECT);
    if (s && brw.open) brw_move(brw.tab, (int)s, 1);
    for (unsigned i = 0; i < 6; ++i) {
        int32_t e = plat_enc(ENC_OF[i]);
        if (!e || !brw.open) continue;
        if (i == 4) { if (brw.fx) brw_move(brw.tab, (int)e, 0); else brw_engine((int)e); }   /* ALGO: the engines (an FX slot: its effects) */
        if (i == 5 && !brw.fx) {                                       /* PRESET: the models of the engine showing */
            brw_set_tab(0);
            if (brw_model()) brw_move(0, (int)e, 0);
            else { app_say("MODEL", "NONE FOR THIS ENGINE"); ui.redraw = 1; }
        }
    }
    if (brw.open) brw_lights(btn, keys);
}

/* ---- the page ---- */
static uint16_t dev_col(unsigned dev);
/* Its colours are the EDIT page's: an engine's name in that engine's colour (the tracks' four and teal, OFF white), its
 * models in the same; an effect's place in that place's colour (the four pills') */
static void brw_draw(uint32_t now)
{
    const uint16_t acc = brw.fx ? TRACK_COL[brw.tab & 3u] : dev_col(ch_track_get(ui.track)->device[0]);
    /* the tabs, and whose they are */
    unsigned nt = brw_tabs();
    cv_begin(240, 40, C_BLACK);
    {
        char tn[4] = {'T', (char)('1' + ui.track), 0};
        const char *who = ui.mfx ? "M" : tn;
        cv_text(6, 9, &FONT_B, who, ui.mfx ? C_WHITE : TRACK_COL[ui.track]);
        int32_t x0 = 34, span = (240 - x0) / (int32_t)nt;
        for (unsigned i = 0; i < nt; ++i) {
            const char *nm = brw_tab_name(i);
            const felucca_font_t *f = text_w(&FONT_B, nm) <= span - 6 ? &FONT_B : &FONT_XS;
            int32_t w = text_w(f, nm), x = x0 + span * (int32_t)i + (span - w) / 2;
            cv_text(x, f == &FONT_B ? 9 : 12, f, nm, i == brw.tab ? C_WHITE : C_DIM);
            if (i == brw.tab) cv_rect(x, 29, w, 2, acc);
        }
    }
    cv_rect(0, 38, 240, 1, C_LINE);
    cv_blit(0, 0);
    /* the list: six lines, the bar on the one that sounds */
    int cur = brw_current();
    for (unsigned band = 0; band < 2; ++band) {
        cv_begin(240, 78, C_BLACK);
        if (!brw.fx && brw.tab == 0) {                     /* ENGINES: two columns, the engines and the models of the one the track has */
            int32_t oy = 14 - (int32_t)band * 78;
            const ch_param_desc *m = brw_model();
            unsigned dev0 = ch_track_get(ui.track)->device[0];
            if (!band) {
                cv_text(10, -2, &FONT_XS, "ALGO", C_DIM);
                int32_t x = cv_text(90, -2, &FONT_XS, m ? m->name : "MODEL", C_GRAY);
                cv_text(x + 6, -2, &FONT_XS, "SELECT", C_DIM);
            }
            cv_rect(78, 0, 1, 78, C_LINE);
            for (unsigned i = 0; i < 6; ++i) {
                int32_t y = oy + (int32_t)i * 24;
                if (y >= 78 || y + 24 <= 0) continue;
                int on = BRW_ENG[i] == dev0;
                if (on) { cv_rect(2, y, 74, 22, LIB_FILL); cv_rect(2, y, 3, 22, dev_col(BRW_ENG[i])); }
                cv_text(10, y + 3, &FONT_B, BRW_ENG[i] == CH_DEV_BYPASS ? "OFF" : ch_device_info(BRW_ENG[i])->name, dev_col(BRW_ENG[i]));
                unsigned j = brw.top + i;
                if (j < brw.n) {
                    int sel = j == brw.sel, have; const char *nm = brw_name(j, &have);
                    if (sel) { cv_rect(82, y, 150, 22, LIB_FILL); cv_rect(82, y, 3, 22, acc); }
                    char fit[24]; lib_fit(fit, nm, &FONT_B, 138);
                    cv_text(90, y + 3, &FONT_B, fit, !have ? C_DIM : sel ? C_WHITE : acc);   /* (the one the bar is on white, as the model is on the EDIT page) */
                }
            }
            if (!brw.n && !band) {
                cv_text(90, 20, &FONT_XS, dev0 == CH_DEV_KIT ? "EIGHT DRUMS" : dev0 == CH_DEV_BYPASS ? "THE TRACK IS OFF" : "NO MODELS", C_DIM);
                if (dev0 == CH_DEV_KIT) cv_text(90, 34, &FONT_XS, "CHOSEN ON THE EDIT PAGES", C_DIM);
            }
            if (brw.n > BRW_ROWS) {
                int32_t h = 144 * BRW_ROWS / (int32_t)brw.n, at = oy + 144 * (int32_t)brw.top / (int32_t)brw.n;
                int32_t a0 = oy < 0 ? 0 : oy, b0 = oy + 144 > 78 ? 78 : oy + 144;
                cv_rect(236, a0, 2, b0 - a0, C_LINE);
                int32_t a = at < 0 ? 0 : at, b = at + h > 78 ? 78 : at + h;
                if (b > a) cv_rect(236, a, 2, b - a, C_GRAY);
            }
            cv_blit(0, band ? 120 : 42);
            continue;
        }
        if (!brw.n && !band) {
            const char *m = "NOTHING TO CHOOSE HERE";
            cv_text(120 - text_w(&FONT_XS, m) / 2, 40, &FONT_XS, m, C_DIM);
        }
        for (unsigned r = 0; r < 3; ++r) {
            unsigned i = brw.top + band * 3 + r;
            if (i >= brw.n) break;
            int32_t y = (int32_t)r * 26;
            int sel = i == brw.sel, have; const char *nm = brw_name(i, &have);
            if (sel) { cv_rect(2, y, 228, 24, LIB_FILL); cv_rect(2, y, 3, 24, acc); }
            char d[4]; int_text((int32_t)i + 1, d);
            cv_text(26 - text_w(&FONT_T, d), y + 7, &FONT_T, d, sel ? C_GRAY : C_DIM);
            char fit[24]; lib_fit(fit, nm, &FONT_B, 170);
            cv_text(34, y + 4, &FONT_B, fit, !have ? C_DIM : sel ? C_WHITE : brw.fx ? C_GRAY : acc);
            if ((int)i == cur) cv_rect(214, y + 8, 8, 8, acc);
        }
        if (brw.n > BRW_ROWS) {                            /* where the six are in the list */
            int32_t h = 156 * BRW_ROWS / (int32_t)brw.n, at = 156 * (int32_t)brw.top / (int32_t)brw.n - (int32_t)band * 78;
            cv_rect(236, 0, 2, 78, C_LINE);
            int32_t a = at < 0 ? 0 : at, b = at + h > 78 ? 78 : at + h;
            if (b > a) cv_rect(236, a, 2, b - a, C_GRAY);
        }
        cv_blit(0, band ? 120 : 42);
    }
    /* a message, or what the track holds now (and held before); then what the buttons do */
    cv_begin(240, 42, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(6, 3, &FONT_B, ui.msg[0], C_WHITE);
        char m[24]; lib_fit(m, ui.msg[1], &FONT_B, 234 - x - 8); cv_text(x + 8, 3, &FONT_B, m, C_HI);
    } else {
        const char *now_on = brw.fx ? fx_name(ch_track_get(ET), brw_slot()) : material_name(ui.track);
        const char *was = brw.fx ? (brw.fxwas[brw.tab].dev == CH_DEV_BYPASS ? "EMPTY" : ch_device_info(brw.fxwas[brw.tab].dev)->name) : brw.was;
        char m[24]; lib_fit(m, now_on, &FONT_B, 120);
        int32_t x = cv_text(6, 3, &FONT_B, m, acc);
        if (brw.changed && was) {
            x = cv_text(x + 10, 6, &FONT_XS, "WAS", C_DIM);
            lib_fit(m, was, &FONT_XS, 234 - x - 6); cv_text(x + 5, 6, &FONT_XS, m, C_GRAY);
        }
    }
    {
        int32_t x = 6, y = 25;
        x = lib_hint(x, y, "SAVE", "KEEP", brw.changed);
        x = lib_hint(x, y, "SEL", brw.changed ? "UNDO" : "CLOSE", 1);
        lib_hint(x, y, brw.fx ? "FX" : "EDIT", brw.fx ? "SLOT" : brw.tab ? "ENGINES" : "PRESETS", 1);
    }
    cv_blit(0, 198);
}
