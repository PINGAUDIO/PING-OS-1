/* PING OS-1: the project in the flash (fm1-base's store, plat_store_*): a settings object per track, one for the
 * tempo, the swing, the key, the master chain and the song, and up to sixteen pattern objects (the engine's packed
 * form, ch_pattern_pack: the four tracks together). Each object is under PLAT_STORE_MAX (3840 B). SAVE writes only
 * the objects that changed since they were saved or loaded (a flash write stops the sound for a moment, so as few
 * as possible); the pattern playing is also written when the transport stops. A load replays the settings through
 * the engine's command API, which has a ring of 512: it runs in stages across frames (pj_load_pump), carrying on
 * where the ring was full. */
#pragma once
#include <stddef.h>

enum { PJ_MAGIC = 0x314A5250u, PJ_VERSION = 3 };          /* "PRJ1"; 3: patterns as objects, the song */
enum { PJ_PATTERNS = 16, PJ_SONG_MAX = 32 };
typedef struct { uint32_t magic; uint16_t version, size; } pj_hdr;
typedef struct {
    pj_hdr h;
    float tempo, swing;
    uint8_t key_root, key_scale, track, pad;
    uint8_t scale[CH_TRACKS], root[CH_TRACKS];            /* how each track's lanes were laid out */
    uint8_t mdev[CH_SLOTS], mbyp[CH_SLOTS], mpad[2];      /* the master chain: its effects, their settings */
    float mparam[CH_FX][CH_PARAMS];
    uint8_t mmode[CH_FX][CH_PARAMS];
    uint8_t cur_pat, song_on, song_len, spad;             /* the pattern playing; the song: its entries */
    struct { uint8_t pat, bars; } song[PJ_SONG_MAX];      /* pat 1..16 */
    uint8_t macro[CH_MACROS];                             /* the macros where they were left, 0..200 */
} pj_project;
enum { PJ_PROJECT_OLD = offsetof(pj_project, macro) };   /* a project saved before the macros */
enum { PJ_ROWS = 8 };                                      /* the settings rows kept: DRUM, SYNTH, SAMPLE, GRAIN, the four FX slots */
static const uint8_t PJ_ROW[PJ_ROWS] = {CH_DEV_DRUM, CH_DEV_SYNTH, CH_DEV_POLY, CH_DEV_GRAIN, CH_MROWS, CH_MROWS + 1, CH_MROWS + 2, CH_MROWS + 3};
typedef struct {
    pj_hdr h;
    uint8_t device[CH_SLOTS], bypass[CH_SLOTS], mute, pad;
    float level, pan, filter, send[CH_SENDS];
    float param[PJ_ROWS][CH_PARAMS];
    uint8_t mode[PJ_ROWS][CH_PARAMS];
    uint8_t mod_type[CH_MODS], mod_shape[CH_MODS];
    float mod_param[CH_MODS][CH_MOD_PARAMS];
    uint8_t mod_mode[CH_MODS][CH_MOD_PARAMS];
    struct { uint8_t mod, slot, param, bipolar; float depth; } map[CH_MAPS];
    float xparam[8];                                      /* the SYNTH's extras (CH_SYNTH_XROW): F DECAY, the oscillators' tuning */
    float kit[CH_PARAMS]; uint8_t kit_mode[CH_PARAMS];    /* KIT: its parts' PITCH and DECAY */
    float kitx[CH_PARAMS];                                /* and their MODEL and LEVEL (CH_KIT_XROW) */
    float xparam2[8];                                     /* the SYNTH's extras past the eighth: RIPPLE's */
} pj_track;
enum { PJ_TRACK_OLD = offsetof(pj_track, xparam) };     /* a track object saved before the SYNTH's extras */
enum { PJ_TRACK_OLD2 = offsetof(pj_track, xparam2) };   /* one saved before RIPPLE's */

static union { pj_project p; pj_track trk; uint8_t raw[PLAT_STORE_MAX]; } pj_buf CH_POOL;
static uint32_t pj_crc[OBJ_NOBJ];                          /* each object's hash as saved or loaded: unchanged = not written */
static struct { uint8_t stage, loaded, found; unsigned done; } pj_ld = {9, 0, 0, 0};   /* found: a saved project or track came in */
static int pj_loading(void) { return pj_ld.stage < 9; }
/* the song: the chain, and where it is */
static struct { uint8_t on, len; struct { uint8_t pat, bars; } e[PJ_SONG_MAX]; uint8_t pos, cued, cur; uint16_t start_bar; } song;

static uint32_t pj_hash(const void *d, unsigned n)
{
    const uint8_t *b = (const uint8_t *)d; uint32_t h = 2166136261u;
    for (unsigned i = 0; i < n; ++i) { h ^= b[i]; h *= 16777619u; }
    return h ? h : 1;
}
static void pj_head(pj_hdr *h, unsigned size) { h->magic = PJ_MAGIC; h->version = PJ_VERSION; h->size = (uint16_t)size; }
/* 0 = written or the same as saved, else the store's error */
static int pj_write(unsigned obj, unsigned n, int *wrote)
{
    uint32_t h = pj_hash(&pj_buf, n);
    if (pj_crc[obj] == h) return 0;
    int r = plat_store_save(obj, &pj_buf, n);
    if (r == 0) { pj_crc[obj] = h; ++*wrote; }
    return r;
}

/* ---- patterns: an object each, the engine's packed form after the header ---- */
static uint16_t pat_exist;                                 /* a bit per pattern with an object (refreshed when HOME is held) */
static uint16_t pat_unsaved;                               /* a bit per pattern with edits not in the flash (in RAM or the stash) */
static void pat_scan(void) { pat_exist = 0; for (unsigned i = 0; i < PJ_PATTERNS; ++i) if (plat_store_size(OBJ_PT0 + i) > (int)sizeof(pj_hdr)) pat_exist |= (uint16_t)(1u << i); }
/* the pattern `no` written from a packer (the current or the stash): 0 ok, -2 too detailed to fit, else the store's */
static int pat_write(unsigned no, unsigned (*packer)(uint8_t *, unsigned), int *wrote)
{
    unsigned n = packer(pj_buf.raw + sizeof(pj_hdr), PLAT_STORE_MAX - sizeof(pj_hdr));
    if (!n) return -2;
    pj_head((pj_hdr *)pj_buf.raw, n + sizeof(pj_hdr));
    int r = pj_write(OBJ_PT0 + no, n + sizeof(pj_hdr), wrote);
    if (r == 0) { pat_exist |= (uint16_t)(1u << no); pat_unsaved &= (uint16_t)~(1u << no); }
    return r;
}
/* pattern `no` read into pj_buf: 1 = there (packed at pj_buf.raw + sizeof(pj_hdr)) */
static int pat_read(unsigned no)
{
    int n = plat_store_load(OBJ_PT0 + no, &pj_buf, sizeof pj_buf);
    if (n < (int)sizeof(pj_hdr) + 8) return 0;
    const pj_hdr *h = (const pj_hdr *)&pj_buf;
    if (h->magic != PJ_MAGIC || h->version != PJ_VERSION || h->size > (unsigned)n || h->size <= sizeof(pj_hdr)) return 0;
    pj_crc[OBJ_PT0 + no] = pj_hash(&pj_buf, h->size);
    return 1;
}
/* what is edited goes to the flash: the stash (another pattern's edits) and the one playing; 1 = something written */
static int pat_flush(int say)
{
    if (ch_snapshot_get()->pattern_pending) return 0;   /* (pj_buf holds the switch's pattern until the bar) */
    int wrote = 0, err = 0; char which[24] = "";
    if (ch_stash_valid() && ui.stash_no >= 0) {
        int r = pat_write((unsigned)ui.stash_no, ch_stash_pack, &wrote);
        if (r == -2) { app_say("PATTERN", "TOO DETAILED TO SAVE"); return 0; }
        err |= r; ch_stash_drop(); text_cat(which, "P"); { char n[4]; int_text(ui.stash_no + 1, n); text_cat(which, n); } ui.stash_no = -1;
    }
    if (ch_pattern_dirty()) pat_unsaved |= (uint16_t)(1u << ui.cur_pat);
    if (pat_unsaved >> ui.cur_pat & 1u) {
        int r = pat_write(ui.cur_pat, ch_pattern_pack, &wrote);
        if (r == -2) { app_say("PATTERN", "TOO DETAILED TO SAVE"); return 0; }
        err |= r; ch_pattern_mark_saved();
        if (which[0]) text_cat(which, " "); text_cat(which, "P"); { char n[4]; int_text(ui.cur_pat + 1, n); text_cat(which, n); }
    }
    if (err) app_say("SAVE", "FAILED");
    else if (say && wrote) app_say("SAVED", which);
    return wrote;
}
/* to pattern `n` (0..15): at the bar while playing, at once otherwise. The one playing, if edited, goes to the
 * stash; a stash holding another pattern's edits is written to the flash first (a moment's silence) */
static void pattern_select(unsigned n, int at_bar)
{
    const ch_snapshot *sn = ch_snapshot_get();
    if (n >= PJ_PATTERNS) return;
    if ((int)n != ui.cur_pat && !sn->pattern_pending) ap_leave_pattern();   /* (AUTOPILOT's REVERT: this pattern as it was, before it is left) */
    char nm[8] = "P"; { char d[4]; int_text((int32_t)n + 1, d); text_cat(nm, d); }
    if (sn->pattern_pending) { app_say(nm, "WAIT FOR THE BAR"); return; }
    if ((int)n == ui.cur_pat) { app_say(nm, "PLAYING"); return; }
    if (ch_pattern_dirty()) pat_unsaved |= (uint16_t)(1u << ui.cur_pat);
    int keep = (pat_unsaved >> ui.cur_pat & 1u) != 0;              /* edits not yet in the flash: kept in the stash */
    if (keep && ch_stash_valid() && ui.stash_no != (int)n) {       /* a third pattern edited: the stash to the flash now */
        int wrote = 0;
        int r = pat_write((unsigned)ui.stash_no, ch_stash_pack, &wrote);
        if (r == -2) { app_say("PATTERN", "STASH TOO DETAILED: SAVE"); return; }
        ch_stash_drop(); ui.stash_no = -1;
    }
    at_bar = at_bar && sn->playing;
    int back = ch_stash_valid() && ui.stash_no == (int)n;
    if (back) {
        ch_pattern_restore(at_bar, keep);
        ui.stash_no = keep ? ui.cur_pat : -1;
    } else {
        int have = pat_read(n);
        ch_pattern_switch(have ? pj_buf.raw + sizeof(pj_hdr) : 0, at_bar, keep);
        if (keep) ui.stash_no = ui.cur_pat;
    }
    ui.cur_pat = (uint8_t)n;
    app_say(nm, at_bar ? "AT THE BAR" : back ? "BACK" : (pat_exist >> n & 1u) ? "" : "NEW");
    ui.redraw = 1;
}
/* the pattern playing copied into `n` (the flash) */
static void pattern_copy(unsigned n)
{
    if (n >= PJ_PATTERNS || (int)n == ui.cur_pat) return;
    ap_leave_pattern();
    if (ch_snapshot_get()->pattern_pending) { app_say("COPY", "WAIT FOR THE BAR"); return; }
    if (ch_stash_valid() && ui.stash_no == (int)n) { ch_stash_drop(); ui.stash_no = -1; }
    int wrote = 0;
    pj_crc[OBJ_PT0 + n] = 0;                                /* (a copy always writes) */
    int r = pat_write(n, ch_pattern_pack, &wrote);
    char nm[16] = "TO P"; { char d[4]; int_text((int32_t)n + 1, d); text_cat(nm, d); }
    app_say(r == -2 ? "TOO DETAILED" : r ? "COPY FAILED" : "COPIED", nm);
}

/* ---- presets: a track's sound (its engine and settings, the four effects, modulators, mappings); sixteen in four
 * bank objects of four. SAVE held + a white key stores one, EDIT held + a white key brings it in. ---- */
enum { PJ_PRESETS = 16, PJ_PER_BANK = 4 };
typedef struct {
    uint8_t used, engine, pad[2];
    float eparam[CH_PARAMS]; uint8_t emode[CH_PARAMS];
    float xparam[CH_PARAMS];                              /* the engine's extras (SYNTH, KIT) */
    uint8_t fxdev[CH_FX], fxbyp[CH_FX];
    float fxparam[CH_FX][CH_PARAMS]; uint8_t fxmode[CH_FX][CH_PARAMS];
    uint8_t mod_type[CH_MODS], mod_shape[CH_MODS];
    float mod_param[CH_MODS][CH_MOD_PARAMS]; uint8_t mod_mode[CH_MODS][CH_MOD_PARAMS];
    struct { uint8_t mod, slot, param, bipolar; float depth; } map[CH_MAPS];
} pj_preset;
typedef struct { pj_hdr h; pj_preset p[PJ_PER_BANK]; } pj_bank;
static uint16_t pre_exist;                                 /* a bit per preset stored */
static int pre_bank_read(unsigned bank)                    /* into pj_buf; 0 = none there (pj_buf then a blank bank) */
{
    int n = plat_store_load(OBJ_PRE0 + bank, &pj_buf, sizeof pj_buf);
    const pj_hdr *h = (const pj_hdr *)&pj_buf;
    if (n < (int)sizeof(pj_bank) || h->magic != PJ_MAGIC || h->version != PJ_VERSION || h->size != sizeof(pj_bank)) {
        memset(&pj_buf, 0, sizeof(pj_bank)); pj_head((pj_hdr *)pj_buf.raw, sizeof(pj_bank)); return 0;
    }
    return 1;
}
static void pre_scan(void)
{
    if (ch_snapshot_get()->pattern_pending) return;
    pre_exist = 0;
    for (unsigned b = 0; b < PJ_PRESETS / PJ_PER_BANK; ++b) {
        if (!pre_bank_read(b)) continue;
        const pj_bank *bk = (const pj_bank *)pj_buf.raw;
        for (unsigned k = 0; k < PJ_PER_BANK; ++k) if (bk->p[k].used) pre_exist |= (uint16_t)(1u << (b * PJ_PER_BANK + k));
    }
}
static const char *preset_name(const pj_preset *q)
{
    unsigned model = (unsigned)(q->eparam[0] + 0.5f);
    if (q->engine == CH_DEV_DRUM) return ch_drum_names[model < CH_DRUM_MODELS ? model : 0];
    if (q->engine == CH_DEV_SYNTH) return ch_synth_names[model < CH_SYNTH_MODELS ? model : 0];
    if (q->engine == CH_DEV_KIT) return "KIT";
    if (q->engine == CH_DEV_POLY) return "SAMPLE";
    if (q->engine == CH_DEV_GRAIN) return "GRAIN";
    return "OFF";
}
/* a track's sound into q, and q onto a track (exact: the track's own mappings go first, so it is q and nothing more) */
static void preset_capture(pj_preset *q, unsigned t)
{
    const ch_track_state *ts = ch_track_get(t);
    memset(q, 0, sizeof *q);
    q->used = 1; q->engine = ts->device[CH_MATERIAL];
    unsigned er = ch_row(CH_MATERIAL, q->engine);
    for (unsigned k = 0; k < CH_PARAMS; ++k) { q->eparam[k] = ts->param[er][k]; q->emode[k] = ts->mode[er][k]; }
    if (ch_xrow(q->engine)) for (unsigned k = 0; k < CH_PARAMS; ++k) q->xparam[k] = ts->param[ch_xrow(q->engine)][k];
    for (unsigned f = 0; f < CH_FX; ++f) {
        q->fxdev[f] = ts->device[CH_FX1 + f]; q->fxbyp[f] = ts->bypass[CH_FX1 + f];
        for (unsigned k = 0; k < CH_PARAMS; ++k) { q->fxparam[f][k] = ts->param[CH_MROWS + f][k]; q->fxmode[f][k] = ts->mode[CH_MROWS + f][k]; }
    }
    for (unsigned m = 0; m < CH_MODS; ++m) {
        q->mod_type[m] = ts->mod_type[m]; q->mod_shape[m] = ts->mod_shape[m];
        for (unsigned k = 0; k < CH_MOD_PARAMS; ++k) { q->mod_param[m][k] = ts->mod_param[m][k]; q->mod_mode[m][k] = ts->mod_mode[m][k]; }
    }
    for (unsigned i = 0; i < CH_MAPS; ++i) { q->map[i].mod = ts->map[i].mod; q->map[i].slot = ts->map[i].slot; q->map[i].param = ts->map[i].param; q->map[i].bipolar = ts->map[i].bipolar; q->map[i].depth = ts->map[i].depth; }
}
static void preset_apply(const pj_preset *q, unsigned t, int exact)
{
    unsigned eng = q->engine == CH_DEV_KIT ? CH_DEV_KIT : q->engine == CH_DEV_SYNTH ? CH_DEV_SYNTH : q->engine == CH_DEV_POLY ? CH_DEV_POLY : q->engine == CH_DEV_GRAIN ? CH_DEV_GRAIN : q->engine == CH_DEV_BYPASS ? CH_DEV_BYPASS : CH_DEV_DRUM;
    if (eng != CH_DEV_BYPASS) {
        ch_device_set(t, CH_MATERIAL, eng);
        const ch_device_desc *d = ch_device_info(eng);
        for (unsigned k = 0; k < d->params; ++k) {        /* (past CH_PARAMS: the extras, kept apart; they once read on into emode) */
            if (k >= CH_PARAMS) { ch_param_set(t, CH_MATERIAL, k, q->xparam[k - CH_PARAMS]); continue; }
            ch_param_set(t, CH_MATERIAL, k, q->eparam[k]); if (d->param[k].modes) ch_mode_set(t, CH_MATERIAL, k, q->emode[k]);
        }
    }
    ch_device_set(t, CH_MATERIAL, eng); ch_bypass_set(t, CH_MATERIAL, 0);
    for (unsigned f = 0; f < CH_FX; ++f) {
        unsigned dev = q->fxdev[f] >= CH_DEV_FX_FIRST && q->fxdev[f] < CH_DEVICES ? q->fxdev[f] : CH_DEV_BYPASS;
        ch_device_set(t, CH_FX1 + f, dev); ch_bypass_set(t, CH_FX1 + f, q->fxbyp[f]);
        if (dev == CH_DEV_BYPASS) continue;
        const ch_device_desc *d = ch_device_info(dev);
        for (unsigned k = 0; k < d->params; ++k) { ch_param_set(t, CH_FX1 + f, k, q->fxparam[f][k]); if (d->param[k].modes) ch_mode_set(t, CH_FX1 + f, k, q->fxmode[f][k]); }
    }
    for (unsigned m = 0; m < CH_MODS; ++m) {
        ch_mod_type_set(t, m, q->mod_type[m] < CH_MOD_TYPES ? q->mod_type[m] : 0); ch_mod_shape_set(t, m, q->mod_shape[m]);
        for (unsigned k = 0; k < CH_MOD_PARAMS; ++k) { ch_mod_param_set(t, m, k, q->mod_param[m][k]); ch_mod_mode_set(t, m, k, q->mod_mode[m][k]); }
    }
    if (exact) {
        const ch_track_state *ts = ch_track_get(t);
        for (unsigned i = 0; i < CH_MAPS; ++i) if (ts->map[i].depth != 0) ch_map_set(t, ts->map[i].mod, ts->map[i].slot, ts->map[i].param, 0, 0);
    }
    for (unsigned i = 0; i < CH_MAPS; ++i)
        if (q->map[i].depth != 0 && q->map[i].mod < CH_MODS + CH_MACROS && q->map[i].slot <= CH_MIXSLOT && q->map[i].param < CH_PARAMS)
            ch_map_set(t, q->map[i].mod, q->map[i].slot, q->map[i].param, q->map[i].depth, q->map[i].bipolar);
}
static void preset_save(unsigned n)
{
    if (n >= PJ_PRESETS || ch_snapshot_get()->pattern_pending) { app_say("PRESET", "WAIT FOR THE BAR"); return; }
    unsigned bank = n / PJ_PER_BANK, slot = n % PJ_PER_BANK;
    pre_bank_read(bank);
    pj_bank *bk = (pj_bank *)pj_buf.raw;
    preset_capture(&bk->p[slot], ui.track);
    int wrote = 0; pj_crc[OBJ_PRE0 + bank] = 0;
    int r = pj_write(OBJ_PRE0 + bank, sizeof(pj_bank), &wrote);
    char nm[20] = "PRESET "; { char d[4]; int_text((int32_t)n + 1, d); text_cat(nm, d); }
    if (r) app_say(nm, "SAVE FAILED"); else { pre_exist |= (uint16_t)(1u << n); app_say(nm, "SAVED"); }
}
static void preset_load(unsigned n)
{
    if (n >= PJ_PRESETS || ch_snapshot_get()->pattern_pending) { app_say("PRESET", "WAIT FOR THE BAR"); return; }
    unsigned bank = n / PJ_PER_BANK, slot = n % PJ_PER_BANK;
    char nm[20] = "PRESET "; { char d[4]; int_text((int32_t)n + 1, d); text_cat(nm, d); }
    if (!pre_bank_read(bank)) { app_say(nm, "EMPTY"); return; }
    const pj_bank *bk = (const pj_bank *)pj_buf.raw; const pj_preset *q = &bk->p[slot];
    if (!q->used) { app_say(nm, "EMPTY"); return; }
    preset_apply(q, ui.track, 0);
    ui.page[0] = 0;
    app_say(nm, preset_name(q));
    ui.redraw = 1;
}

/* ---- save: every object, the unchanged ones skipped ---- */
static void pj_fill_project(void)
{
    pj_project *p = &pj_buf.p; memset(p, 0, sizeof *p); pj_head(&p->h, sizeof *p);
    p->spad = (uint8_t)(ch_return() * 100 + 1.5f);          /* the RETURN's level, in hundredths + 1 (0: never set) */
    p->tempo = ch_tempo(); p->swing = ch_swing(); p->key_root = ui.key_root; p->key_scale = ui.key_scale; p->track = ui.track;
    for (unsigned t = 0; t < CH_TRACKS; ++t) { p->scale[t] = ui.scale[t]; p->root[t] = ui.root[t]; }
    const ch_track_state *m = ch_track_get(CH_MASTER);
    for (unsigned k = 0; k < CH_SLOTS; ++k) { p->mdev[k] = m->device[k]; p->mbyp[k] = m->bypass[k]; }
    for (unsigned k = 0; k < CH_FX; ++k) for (unsigned q = 0; q < CH_PARAMS; ++q) { p->mparam[k][q] = m->param[CH_MROWS + k][q]; p->mmode[k][q] = m->mode[CH_MROWS + k][q]; }
    p->cur_pat = ui.cur_pat; p->song_on = song.on; p->song_len = song.len;
    p->mpad[0] = (uint8_t)(ui.mono | ui.panel << 1 | ui.mixfoot << 2 | (ui.spec_tilt & 3u) << 3); p->mpad[1] = (uint8_t)(ui.spec_tone + 1);   /* the display: MONO, and its tone + 1 (0: never set) */
    for (unsigned i = 0; i < PJ_SONG_MAX; ++i) { p->song[i].pat = song.e[i].pat; p->song[i].bars = song.e[i].bars; }
    for (unsigned m = 0; m < CH_MACROS; ++m) p->macro[m] = (uint8_t)(ch_macro(m) * 200 + 0.5f);
}
static void pj_fill_track(unsigned t)
{
    pj_track *p = &pj_buf.trk; const ch_track_state *s = ch_track_get(t);
    memset(p, 0, sizeof *p); pj_head(&p->h, sizeof *p);
    for (unsigned k = 0; k < CH_SLOTS; ++k) { p->device[k] = s->device[k]; p->bypass[k] = s->bypass[k]; }
    p->mute = s->mute; p->level = s->level; p->pan = s->pan; p->filter = s->filter;
    p->pad = s->ss_dest;                                 /* (the pad byte: the steps' SEND's send; 0, as an older project has it: all) */
    for (unsigned k = 0; k < CH_SENDS; ++k) p->send[k] = s->send[k];
    p->send[t] = s->tofx;                            /* (a track has no send to itself: its place holds TO FX, as what goes round the effects, so an older project's 0 is all of it through them) */
    for (unsigned r = 0; r < PJ_ROWS; ++r) for (unsigned k = 0; k < CH_PARAMS; ++k) { p->param[r][k] = s->param[PJ_ROW[r]][k]; p->mode[r][k] = s->mode[PJ_ROW[r]][k]; }
    for (unsigned k = 0; k < 8; ++k) { p->xparam[k] = s->param[CH_SYNTH_XROW][k]; p->xparam2[k] = s->param[CH_SYNTH_XROW][8 + k]; }
    for (unsigned k = 0; k < CH_PARAMS; ++k) { p->kit[k] = s->param[CH_DEV_KIT][k]; p->kit_mode[k] = s->mode[CH_DEV_KIT][k]; p->kitx[k] = s->param[CH_KIT_XROW][k]; }
    for (unsigned m = 0; m < CH_MODS; ++m) {
        p->mod_type[m] = s->mod_type[m]; p->mod_shape[m] = s->mod_shape[m];
        for (unsigned k = 0; k < CH_MOD_PARAMS; ++k) { p->mod_param[m][k] = s->mod_param[m][k]; p->mod_mode[m][k] = s->mod_mode[m][k]; }
    }
    for (unsigned i = 0; i < CH_MAPS; ++i) { p->map[i].mod = s->map[i].mod; p->map[i].slot = s->map[i].slot; p->map[i].param = s->map[i].param; p->map[i].bipolar = s->map[i].bipolar; p->map[i].depth = s->map[i].depth; }
}
static void pj_save(void)
{
    ap_before_save();
    if (pj_loading()) { app_say("SAVE", "LOADING, WAIT"); return; }
    if (ch_snapshot_get()->pattern_pending) { app_say("SAVE", "WAIT FOR THE BAR"); return; }
    int wrote = 0, err = 0;
    pj_fill_project(); err |= pj_write(OBJ_PROJECT, sizeof(pj_project), &wrote);
    for (unsigned t = 0; t < CH_TRACKS; ++t) { pj_fill_track(t); err |= pj_write(OBJ_TRK0 + t, sizeof(pj_track), &wrote); }
    wrote += pat_flush(0);
    if (err) app_say("SAVE", "FAILED");
    else if (!wrote) app_say("SAVED", "NO CHANGES");
    else app_say("SAVED", "PROJECT");
}

/* ---- load: an object a stage, its settings pushed in order; on a full ring, the rest next frame ---- */
static int pj_obj(unsigned obj, unsigned size)
{
    int n = plat_store_load(obj, &pj_buf, sizeof pj_buf);
    if (n < (int)sizeof(pj_hdr)) return 0;
    const pj_hdr *h = (const pj_hdr *)&pj_buf;
    if (h->magic != PJ_MAGIC || h->version != PJ_VERSION || n < (int)h->size) return 0;
    if (h->size != size && size == sizeof(pj_project) && h->size == PJ_PROJECT_OLD) {   /* from before the macros: they at 0 */
        memset(pj_buf.raw + PJ_PROJECT_OLD, 0, sizeof(pj_project) - PJ_PROJECT_OLD);
        pj_crc[obj] = 0; return 1;
    }
    if (h->size != size && size == sizeof(pj_track) && h->size == PJ_TRACK_OLD2) {   /* from before RIPPLE: its extras as it starts */
        for (unsigned k = 0; k < 8; ++k) { float v = 0; pj_buf.trk.xparam2[k] = ch_synth_preset(13u, CH_PARAMS + 8 + k, &v) ? v : 0; }
        pj_crc[obj] = 0; return 1;
    }
    if (h->size != size) {
        if (size != sizeof(pj_track) || h->size != PJ_TRACK_OLD) return 0;   /* a track from before the SYNTH's extras: those from the model's presets */
        pj_track *tp = &pj_buf.trk; unsigned model = (unsigned)(tp->param[1][0] + 0.5f);
        for (unsigned k = 0; k < 8; ++k) { float v = 0; ch_synth_preset(model, CH_PARAMS + k, &v); tp->xparam[k] = v; }
        pj_crc[obj] = 0;                                  /* (not as saved: written again on the next SAVE) */
        return 1;
    }
    pj_crc[obj] = pj_hash(&pj_buf, size);
    return 1;
}
#define DO(call) do { if (k++ == pj_ld.done) { if ((call) < 0) return -1; ++pj_ld.done; } } while (0)
static int pj_row(unsigned t, unsigned slot, unsigned dev, const float *param, const uint8_t *mode, unsigned *kp)
{
    unsigned k = *kp; const ch_device_desc *d = ch_device_info(dev);
    for (unsigned p = 0; d && p < d->params; ++p) {
        if (p >= CH_PARAMS) { DO(ch_param_set(t, slot, p, dev == CH_DEV_KIT ? pj_buf.trk.kitx[p - CH_PARAMS] : p - CH_PARAMS < 8 ? pj_buf.trk.xparam[p - CH_PARAMS] : pj_buf.trk.xparam2[p - CH_PARAMS - 8])); continue; }   /* an engine's extras */
        DO(ch_param_set(t, slot, p, param[p]));
        if (d->param[p].modes) DO(ch_mode_set(t, slot, p, mode[p]));
    }
    *kp = k; return 0;
}
static int pj_stage_project(void)
{
    const pj_project *p = &pj_buf.p; unsigned k = 0;
    ch_return_set(p->spad && p->spad <= 101 ? (float)(p->spad - 1) * 0.01f : 0.8f);
    DO(ch_tempo_set(p->tempo)); DO(ch_swing_set(p->swing));
    for (unsigned m = 0; m < CH_MACROS; ++m) DO(ch_macro_set(m, (float)(p->macro[m] <= 200 ? p->macro[m] : 0) / 200.0f));
    for (unsigned t = 0; t < CH_TRACKS; ++t) { ui.scale[t] = (uint8_t)(p->scale[t] < SCALES ? p->scale[t] : 0); ui.root[t] = p->root[t] >= 24 && p->root[t] <= 96 ? p->root[t] : 60; }
    ui.mono = p->mpad[0] & 1u; ui.panel = p->mpad[0] >> 1 & 1u; ui.mixfoot = p->mpad[0] >> 2 & 1u; ui.spec_tilt = p->mpad[0] >> 3 & 3u; if (p->mpad[1] && p->mpad[1] <= 25) ui.spec_tone = (uint8_t)(p->mpad[1] - 1);
    ui.key_root = p->key_root <= 12 ? p->key_root : 0; ui.key_scale = p->key_scale < SCALES ? p->key_scale : 0; key_apply();
    if (p->track < CH_TRACKS) ui.track = p->track;
    ui.cur_pat = (uint8_t)(p->cur_pat < PJ_PATTERNS ? p->cur_pat : 0);
    song.on = p->song_on != 0; song.len = (uint8_t)(p->song_len <= PJ_SONG_MAX ? p->song_len : 0);
    for (unsigned i = 0; i < PJ_SONG_MAX; ++i) { song.e[i].pat = (uint8_t)(p->song[i].pat <= PJ_PATTERNS ? p->song[i].pat : 0); song.e[i].bars = (uint8_t)(p->song[i].bars >= 1 && p->song[i].bars <= 64 ? p->song[i].bars : 4); }
    for (unsigned sl = CH_FX1; sl <= CH_FX4; ++sl) {       /* the master chain, through the ring like a track's */
        unsigned dev = p->mdev[sl] >= CH_DEV_FX_FIRST && p->mdev[sl] < CH_DEVICES ? p->mdev[sl] : CH_DEV_BYPASS;
        DO(ch_device_set(CH_MASTER, sl, dev)); DO(ch_bypass_set(CH_MASTER, sl, p->mbyp[sl]));
        if (dev != CH_DEV_BYPASS && pj_row(CH_MASTER, sl, dev, p->mparam[sl - CH_FX1], p->mmode[sl - CH_FX1], &k) < 0) return -1;
    }
    return 0;
}
static int pj_stage_track(unsigned t)
{
    const pj_track *p = &pj_buf.trk; unsigned k = 0;
    /* the engines' settings: each engine in turn (its row is set while it is the engine), then the one in use */
    DO(ch_device_set(t, CH_MATERIAL, CH_DEV_DRUM)); if (pj_row(t, CH_MATERIAL, CH_DEV_DRUM, p->param[0], p->mode[0], &k) < 0) return -1;
    DO(ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH)); if (pj_row(t, CH_MATERIAL, CH_DEV_SYNTH, p->param[1], p->mode[1], &k) < 0) return -1;
    DO(ch_device_set(t, CH_MATERIAL, CH_DEV_POLY)); if (pj_row(t, CH_MATERIAL, CH_DEV_POLY, p->param[2], p->mode[2], &k) < 0) return -1;
    DO(ch_device_set(t, CH_MATERIAL, CH_DEV_GRAIN)); if (pj_row(t, CH_MATERIAL, CH_DEV_GRAIN, p->param[3], p->mode[3], &k) < 0) return -1;
    DO(ch_device_set(t, CH_MATERIAL, CH_DEV_KIT)); if (pj_row(t, CH_MATERIAL, CH_DEV_KIT, p->kit, p->kit_mode, &k) < 0) return -1;
    DO(ch_device_set(t, CH_MATERIAL, p->device[0] == CH_DEV_KIT ? CH_DEV_KIT : p->device[0] == CH_DEV_SYNTH ? CH_DEV_SYNTH : p->device[0] == CH_DEV_POLY ? CH_DEV_POLY : p->device[0] == CH_DEV_GRAIN ? CH_DEV_GRAIN : p->device[0] == CH_DEV_BYPASS ? CH_DEV_BYPASS : CH_DEV_DRUM));
    DO(ch_bypass_set(t, CH_MATERIAL, p->bypass[0]));
    for (unsigned s = CH_FX1; s <= CH_FX4; ++s) {
        unsigned dev = p->device[s] >= CH_DEV_FX_FIRST && p->device[s] < CH_DEVICES ? p->device[s] : CH_DEV_BYPASS;
        DO(ch_device_set(t, s, dev)); DO(ch_bypass_set(t, s, p->bypass[s]));
        if (dev != CH_DEV_BYPASS && pj_row(t, s, dev, p->param[4 + s - CH_FX1], p->mode[4 + s - CH_FX1], &k) < 0) return -1;
    }
    for (unsigned m = 0; m < CH_MODS; ++m) {
        DO(ch_mod_type_set(t, m, p->mod_type[m] < CH_MOD_TYPES ? p->mod_type[m] : 0)); DO(ch_mod_shape_set(t, m, p->mod_shape[m]));
        for (unsigned q = 0; q < CH_MOD_PARAMS; ++q) { DO(ch_mod_param_set(t, m, q, p->mod_param[m][q])); DO(ch_mod_mode_set(t, m, q, p->mod_mode[m][q])); }
    }
    for (unsigned i = 0; i < CH_MAPS; ++i)
        if (p->map[i].depth != 0 && p->map[i].mod < CH_MODS + CH_MACROS && p->map[i].slot <= CH_MIXSLOT && p->map[i].param < CH_PARAMS)
            DO(ch_map_set(t, p->map[i].mod, p->map[i].slot, p->map[i].param, p->map[i].depth, p->map[i].bipolar));
    DO(ch_mix_set(t, CH_MIX_LEVEL, p->level)); DO(ch_mix_set(t, CH_MIX_PAN, p->pan)); DO(ch_mix_set(t, CH_MIX_FILTER, p->filter));
    DO(ch_mix_set(t, CH_MIX_MUTE, p->mute ? 1.0f : 0.0f));
    for (unsigned d = 0; d < CH_SENDS; ++d) DO(ch_send_set(t, d, d == t ? 0 : p->send[d]));
    DO(ch_mix_set(t, CH_MIX_TOFX, p->send[t] >= 0 && p->send[t] <= 1 ? p->send[t] : 0));
    DO(ch_mix_set(t, CH_MIX_SSDEST, (float)(p->pad >= CH_SS_FX && p->pad <= CH_SS_LAST ? p->pad : CH_SS_FX)));
    return 0;
}
#undef DO
static void pj_load_begin(void) { pj_ld.stage = 0; pj_ld.loaded = 0; pj_ld.found = 0; pj_ld.done = 0; pat_scan(); }
/* a frame's share of the load; 1 while there is more. The last stage brings the pattern playing in */
static int pj_load_pump(void)
{
    while (pj_ld.stage < 9) {
        unsigned st = pj_ld.stage;
        if (st == 5) {                                     /* the pattern: straight to the engine (no ring) */
            int have = pat_read(ui.cur_pat);
            if (have || pj_ld.found) ch_pattern_switch(have ? pj_buf.raw + sizeof(pj_hdr) : 0, 0, 0);   /* (nothing saved at all: the factory beat stays) */
            pj_ld.stage = 9; break;
        }
        if (!pj_ld.loaded) {
            unsigned obj = st == 0 ? OBJ_PROJECT : OBJ_TRK0 + st - 1;
            unsigned size = st == 0 ? sizeof(pj_project) : sizeof(pj_track);
            if (!pj_obj(obj, size)) { ++pj_ld.stage; pj_ld.done = 0; continue; }   /* none saved: as it starts */
            pj_ld.loaded = 1; pj_ld.found = 1; pj_ld.done = 0;
        }
        int r = st == 0 ? pj_stage_project() : pj_stage_track(st - 1);
        if (r < 0) return 1;                               /* the ring is full: the rest next frame */
        ++pj_ld.stage; pj_ld.loaded = 0; pj_ld.done = 0;
    }
    return 0;
}

/* ---- the song: each frame while playing, the next entry's pattern cued in its last bar ---- */
static void song_frame(void)
{
    const ch_snapshot *sn = ch_snapshot_get();
    if (!song.on || !song.len) { song.cued = 0; return; }
    if (!sn->playing) { song.pos = 0; song.cued = 0; song.start_bar = 0; return; }
    unsigned cur = song.e[song.pos].pat ? song.e[song.pos].pat - 1u : 0;
    if (!song.cued && sn->bar == 0 && (int)cur != ui.cur_pat && !sn->pattern_pending) pattern_select(cur, 0);   /* the start: its first */
    if (song.cued && !sn->pattern_pending) {              /* the switch went through: the next entry begins */
        song.pos = song.cur; song.start_bar = sn->bar; song.cued = 0; ui.redraw = 1;
    }
    unsigned bars = song.e[song.pos].bars ? song.e[song.pos].bars : 4;
    if (!song.cued && sn->bar >= song.start_bar + bars - 1) {
        unsigned nx = (song.pos + 1u) % song.len;
        unsigned pat = song.e[nx].pat ? song.e[nx].pat - 1u : 0;
        song.cur = (uint8_t)nx;
        if ((int)pat != ui.cur_pat) { pattern_select(pat, 1); song.cued = 1; }
        else { song.pos = (uint8_t)nx; song.start_bar = (uint16_t)(song.start_bar + bars); }   /* the same pattern again */
    }
}
