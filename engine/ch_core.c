/* Engine core: commands, transport, parameter/modulation evaluation, the
 * per-track chain, sends and the mixer. */
#include "ch_internal.h"

ch_time ch_t;
ch_trk ch_tr[CH_TRACKS] CH_POOL;
ch_trk ch_mst CH_POOL;
const ch_sample *ch_smp[CH_SAMPLES];
float ch_mix_level;
#define CH_FX_SLEEP ((uint16_t)(2 * CH_RATE / ch_sub))   /* 2 s of sub-blocks */
#define CH_PWAKE 400                                    /* sub-blocks (0.29 s) the values stay awake after a change: the 9 ms glide is at its rest long before */
/* On the FM-1 it starts at 64: tried there 2026-11-18, the same project, RENDER 3908 of 5804 where 32 gave 5164, and
 * the load governor at all eight voices where it had held three. A host (the plugin, the bench) stays at 32, as it was */
#if defined(CHISEL_FM1)
#define CH_SUB_START 64
#else
#define CH_SUB_START 32
#endif
unsigned ch_sub = CH_SUB_START;                    /* a sub-block's frames now */
static uint8_t sub_want = CH_SUB_START;            /* ch_sub_set: what it is to be, taken at a render call's start */
void ch_sub_set(unsigned frames) { sub_want = frames >= 64 ? 64 : 32; }
unsigned ch_sub_get(void) { return sub_want; }
unsigned ch_sub_off, ch_frames;
#ifdef CHISEL_HOST
const int32_t *ch_host_in;       /* the FX plugin: sound in for the next render */
int ch_host_fx = -1;             /* ... the one track it goes into (its engine is left out, no track starts a voice); -1: not the FX plugin */
#define HOST_IN(ti, p) (ch_host_fx < 0 || (int)(ti) == ch_host_fx ? (p) : 0)
#define HOST_FX (ch_host_fx >= 0)
void (*ch_note_tap)(unsigned track, unsigned note, unsigned vel, unsigned frame);   /* the plugin's MIDI out: each note as it is given to a track */
#else
#define HOST_IN(ti, p) (p)
#endif
static ch_snapshot snap;
static float send_bus[2][CH_TRACKS][2][CH_SUB];   /* [phase][dest][ch][frame] */
static unsigned send_phase;
static uint8_t bus_live[2];                         /* per phase: the destinations a send wrote into (a bit each); the others are zeros and are not read */
static float master_comp, master_main = 0.8f, master_env, master_g = 1;
/* the limiter: the last thing before the ceiling's knee (ch_knee: nothing is bent under 0.85). It looks LIM_D samples
 * (~0.7 ms) ahead: the gain a sample needs (the ceiling over it) is held for LIM_D samples and averaged over LIM_D, and
 * the sound is delayed to meet it, so the gain is already down, on a smooth ramp, when the peak arrives (no overshoot)
 * and comes back over ~80 ms. Pulling the gain down at the sample itself, as it did, cut a dense loud mix (DRIVE on
 * several tracks) into a buzz; this way even 10 dB of limiting is a level, not a distortion */
#define LIM_D 32
static float lim_g = 1, lim_rel = 1, lim_sum = LIM_D;
static float lim_dl[LIM_D], lim_dr[LIM_D], lim_held[LIM_D];
static unsigned lim_w, lim_hold;
static uint8_t pm_reset = 1;                          /* the mix kept for the master is to be emptied (ch_init) */
static uint8_t lim_on = 1, lim_was = 1;              /* ch_limiter_set: 0, the mix goes straight to the knee (the LOAD page's switch) */
void ch_limiter_set(int on) { lim_on = on != 0; }
int ch_limiter(void) { return lim_on; }
#define LIM_CEIL 0.94f
#define LIM_REL (1.0f / (0.08f * (float)CH_RATE))
static void lim_reset(void)
{
    lim_g = lim_rel = 1; lim_sum = LIM_D; lim_w = 0; lim_hold = 0;
    for (unsigned i = 0; i < LIM_D; ++i) { lim_dl[i] = lim_dr[i] = 0; lim_held[i] = 1; }
}
/* before the limiter, a 10 Hz high-pass on the master: DC and rumble below hearing off, nothing a kick or an 808 is made
 * of (an E1 loses a quarter of a dB) */
static float sub_l, sub_r;
#define SUB_K (6.2831853f * 10.0f / (float)CH_RATE)
/* the metronome: a short sine blip on each beat (1.3 kHz; the bar's first 1.8 kHz), gentle, into the master */
static uint8_t metro_on;
static uint8_t count_on, count_left, count_n;   /* the count-in: running, clicks still to come, clicks made */
static float count_wait;                        /* frames to its next click (or, none left, to the start) */
uint16_t ch_hits[CH_TRACKS];
uint16_t ch_lane_hits[CH_TRACKS][CH_KIT_PARTS];   /* a KIT's hits, each part's counted (SIDECHAIN on one drum) */
uint8_t ch_last_note[CH_TRACKS];
float ch_tmix[CH_TRACKS][2][CH_SUB];
static float metro_env, metro_ph, metro_inc;

/* ---------------- command ring (control -> audio) ---------------- */
enum { OP_SAMPLE, OP_DEVICE, OP_BYPASS, OP_PARAM, OP_MODE, OP_ACTION, OP_MODTYPE, OP_MODPARAM,
       OP_MODMODE, OP_MODSHAPE, OP_MAP, OP_NOTE, OP_SEND, OP_MIX, OP_MASTER, OP_TRANSPORT, OP_TEMPO,
       OP_SEQ, OP_SWING, OP_INIT, OP_METRO, OP_STUTTER, OP_MACRO, OP_COUNT };
/* the count-in, a sub-block of it: a click a beat (the first higher), then the transport starts */
static CH_NOINLINE void count_step(unsigned n);
typedef struct { uint8_t op, t, a, b, c, d; float f, g; const void *p; } ch_cmd;
enum { RING = 384 };                             /* (512 left the pool under its 8 KB headroom once the spectrum came: its ring and its 30 x 96 history; a full ring is retried next frame) */
static ch_cmd ring[RING] CH_POOL;
static volatile unsigned ring_w, ring_r;
static ch_track_state ctl[CH_TRACKS + 1];        /* [CH_MASTER]: the master chain */
static float ctl_tempo = 120;
static ch_seq_state ctl_seq[CH_TRACKS];
static void track_ctl(unsigned ti);
static float ctl_swing, swing;

static int push(ch_cmd c)
{
    unsigned w = ring_w, next = (w + 1) & (RING - 1);
    if (next == ring_r) return -1;              /* full: the UI retries on its next frame */
    ring[w] = c;
#if defined(CHISEL_FM1) && defined(__GNUC__)
    __asm__ volatile("" ::: "memory");          /* one core: the UI and the audio interrupt; order the stores for the compiler */
#elif defined(__GNUC__)
    __sync_synchronize();
#endif
    ring_w = next;
    return 0;
}

/* ---------------- control side API ---------------- */
const ch_track_state *ch_track_get(unsigned t) { return t <= CH_MASTER ? &ctl[t] : 0; }
float ch_tempo(void) { return ctl_tempo; }
const ch_snapshot *ch_snapshot_get(void) { return &snap; }

int ch_sample_bind(unsigned slot, const ch_sample *s)
{
    if (slot >= CH_SAMPLES) return -1;
    ch_cmd c = {OP_SAMPLE, 0, (uint8_t)slot}; c.p = s; return push(c);
}
int ch_device_set(unsigned t, unsigned slot, unsigned dev)
{
    const ch_device_desc *d = ch_device_info(dev);
    if (t > CH_MASTER || slot >= CH_SLOTS || !d) return -1;
    if (dev != CH_DEV_BYPASS && (slot == CH_MATERIAL ? d->slot != CH_MATERIAL || t == CH_MASTER : dev < CH_DEV_FX_FIRST)) return -1;
    ch_track_state *s = &ctl[t];
    s->device[slot] = (uint8_t)dev;
    if (slot != CH_MATERIAL) {                  /* this slot alone: the effect's own settings, its mappings gone */
        unsigned r = ch_row(slot, dev);
        for (unsigned p = 0; p < CH_PARAMS; ++p) { s->param[r][p] = p < d->params ? d->param[p].def : 0; s->mode[r][p] = 0; }
        for (unsigned i = 0; i < CH_MAPS; ++i) if (s->map[i].slot == slot) s->map[i].depth = 0;
    }
    ch_cmd c = {OP_DEVICE, (uint8_t)t, (uint8_t)slot, (uint8_t)dev}; return push(c);
}
int ch_bypass_set(unsigned t, unsigned slot, unsigned on)
{
    if (t > CH_MASTER || slot >= CH_SLOTS) return -1;
    ctl[t].bypass[slot] = on != 0;
    ch_cmd c = {OP_BYPASS, (uint8_t)t, (uint8_t)slot, (uint8_t)(on != 0)}; return push(c);
}
int ch_param_set(unsigned t, unsigned slot, unsigned p, float v)
{
    if (t > CH_MASTER || slot >= CH_SLOTS) return -1;
    unsigned dev = ctl[t].device[slot];
    const ch_device_desc *d = ch_device_info(dev);
    if (!d || p >= d->params) return -1;
    const ch_param_desc *pd = &d->param[p];
    v = pd->kind == CH_BI ? ch_clamp(v, -1, 1) : pd->kind == CH_SELECT ? ch_clamp((float)(int)(v + 0.5f), 0, (float)(pd->steps - 1)) : ch_clamp(v, 0, 1);
    if (p >= CH_PARAMS) {                                 /* an engine's extra parameters (SYNTH, KIT): its spare row, read raw by the engine */
        unsigned xr = ch_xrow(dev);
        if (slot != CH_MATERIAL || !xr) return -1;
        ctl[t].param[xr][p - CH_PARAMS] = v;
        ch_cmd c = {OP_PARAM, (uint8_t)t, (uint8_t)xr, (uint8_t)(p - CH_PARAMS), (uint8_t)dev}; c.d = 1; c.f = v; return push(c);
    }
    unsigned r = ch_row(slot, dev);
    ctl[t].param[r][p] = v;
    ch_cmd c = {OP_PARAM, (uint8_t)t, (uint8_t)r, (uint8_t)p, (uint8_t)dev}; c.f = v; return push(c);
}
int ch_mode_set(unsigned t, unsigned slot, unsigned p, unsigned m)
{
    if (t > CH_MASTER || slot >= CH_SLOTS) return -1;
    if (p >= CH_PARAMS) return 0;                          /* (the SYNTH's extras: no modes) */
    unsigned r = ch_row(slot, ctl[t].device[slot]);
    ctl[t].mode[r][p] = (uint8_t)m;
    ch_cmd c = {OP_MODE, (uint8_t)t, (uint8_t)r, (uint8_t)p, (uint8_t)m}; return push(c);
}
int ch_action(unsigned t, unsigned slot, unsigned a, unsigned v)
{
    if (t >= CH_TRACKS || slot >= CH_SLOTS) return -1;
    if (slot == CH_MATERIAL && ctl[t].device[slot] == CH_DEV_POLY && a == CH_POLY_LOAD) ctl[t].sample[1] = (uint8_t)v;
    ch_cmd c = {OP_ACTION, (uint8_t)t, (uint8_t)slot, (uint8_t)a, (uint8_t)v}; return push(c);
}
int ch_mod_type_set(unsigned t, unsigned m, unsigned type)
{
    if (t >= CH_TRACKS || m >= CH_MODS || type >= CH_MOD_TYPES) return -1;
    ctl[t].mod_type[m] = (uint8_t)type;
    { ch_mod tmp; ch_mods_init(&tmp, type, 1);          /* the defaults, modes included (the LFO's RATE starts synced) */
      for (unsigned i = 0; i < CH_MOD_PARAMS; ++i) { ctl[t].mod_param[m][i] = tmp.p[i]; ctl[t].mod_mode[m][i] = tmp.m[i]; } }
    ch_cmd c = {OP_MODTYPE, (uint8_t)t, (uint8_t)m, (uint8_t)type}; return push(c);
}
int ch_mod_param_set(unsigned t, unsigned m, unsigned p, float v)
{
    if (t >= CH_TRACKS || m >= CH_MODS || p >= CH_MOD_PARAMS) return -1;
    const ch_param_desc *pd = ch_mod_param_info(ctl[t].mod_type[m], p);
    v = pd->kind == CH_BI ? ch_clamp(v, -1, 1) : pd->kind == CH_SELECT ? ch_clamp(v, 0, (float)(pd->steps - 1)) : ch_clamp(v, 0, 1);
    ctl[t].mod_param[m][p] = v;
    ch_cmd c = {OP_MODPARAM, (uint8_t)t, (uint8_t)m, (uint8_t)p}; c.f = v; return push(c);
}
int ch_mod_mode_set(unsigned t, unsigned m, unsigned p, unsigned mode)
{
    if (t >= CH_TRACKS || m >= CH_MODS || p >= CH_MOD_PARAMS) return -1;
    ctl[t].mod_mode[m][p] = (uint8_t)mode;
    ch_cmd c = {OP_MODMODE, (uint8_t)t, (uint8_t)m, (uint8_t)p, (uint8_t)mode}; return push(c);
}
int ch_mod_shape_set(unsigned t, unsigned m, unsigned s)
{
    if (t >= CH_TRACKS || m >= CH_MODS) return -1;
    ctl[t].mod_shape[m] = (uint8_t)s;
    ch_cmd c = {OP_MODSHAPE, (uint8_t)t, (uint8_t)m, (uint8_t)s}; return push(c);
}
int ch_map_set(unsigned t, unsigned m, unsigned slot, unsigned p, float depth, unsigned bip)
{
    if (t >= CH_TRACKS || m >= CH_MODS + CH_MACROS || p >= CH_PARAMS) return -1;
    if (slot >= CH_SLOTS && !(slot == CH_MIXSLOT && p < 3)) return -1;
    ch_track_state *s = &ctl[t];
    int free_i = -1, at = -1;
    for (int i = 0; i < CH_MAPS; ++i) {
        if (s->map[i].depth != 0 && s->map[i].mod == m && s->map[i].slot == slot && s->map[i].param == p) at = i;
        if (s->map[i].depth == 0 && free_i < 0) free_i = i;
    }
    if (at < 0) at = free_i;
    if (at < 0) return -1;
    s->map[at].mod = (uint8_t)m; s->map[at].slot = (uint8_t)slot; s->map[at].param = (uint8_t)p;
    s->map[at].bipolar = bip != 0; s->map[at].depth = ch_clamp(depth, -1, 1);
    ch_cmd c = {OP_MAP, (uint8_t)t, (uint8_t)at, (uint8_t)m, (uint8_t)slot, (uint8_t)p}; c.f = s->map[at].depth; c.g = (float)(bip != 0);
    return push(c);
}
int ch_note(unsigned t, unsigned note, unsigned vel)
{
    if (t >= CH_TRACKS) return -1;
    ch_cmd c = {OP_NOTE, (uint8_t)t, (uint8_t)note, (uint8_t)vel}; return push(c);
}
int ch_send_set(unsigned t, unsigned target, float a)
{
    if (t >= CH_TRACKS || target >= CH_SENDS) return -1;
    ctl[t].send[target] = ch_clamp(a, 0, 1);
    ch_cmd c = {OP_SEND, (uint8_t)t, (uint8_t)target}; c.f = ctl[t].send[target]; return push(c);
}
int ch_mix_set(unsigned t, unsigned what, float v)
{
    if (t >= CH_TRACKS) return -1;
    if (what == CH_MIX_LEVEL) ctl[t].level = v; else if (what == CH_MIX_FILTER) ctl[t].filter = v;
    else if (what == CH_MIX_PAN) ctl[t].pan = v; else if (what == CH_MIX_MUTE) ctl[t].mute = v != 0;
    else if (what == CH_MIX_SSDEST) ctl[t].ss_dest = (uint8_t)(v < 0.5f ? 0 : v > (float)CH_SS_LAST ? CH_SS_LAST : (int)(v + 0.5f));
    else if (what == CH_MIX_TOFX) { v = ch_clamp(v, 0, 1); ctl[t].tofx = v; if (v > 0) ch_fx_sent = 1; }
    ch_cmd c = {OP_MIX, (uint8_t)t, (uint8_t)what}; c.f = v; return push(c);
}
int ch_master_set(float comp, float main) { ch_cmd c = {OP_MASTER}; c.f = comp; c.g = main; return push(c); }
int ch_transport(unsigned playing) { ch_cmd c = {OP_TRANSPORT, 0, (uint8_t)(playing != 0)}; return push(c); }
const ch_seq_state *ch_seq_get(unsigned t) { return t < CH_TRACKS ? &ctl_seq[t] : 0; }
float ch_swing(void) { return ctl_swing; }
static float ctl_macro[CH_MACROS], macro_v[CH_MACROS];   /* the macros: as set (control), as heard (audio) */
int ch_macro_set(unsigned m, float v)
{
    if (m >= CH_MACROS) return -1;
    ctl_macro[m] = ch_clamp(v, 0, 1);
    ch_cmd c = {OP_MACRO, 0, (uint8_t)m}; c.f = ctl_macro[m]; return push(c);
}
float ch_macro(unsigned m) { return m < CH_MACROS ? ctl_macro[m] : 0; }
static volatile uint8_t pat_dirty;              /* the pattern edited since it was loaded or saved */
int ch_pattern_dirty(void) { return pat_dirty; }
void ch_pattern_mark_saved(void) { pat_dirty = 0; }
int ch_step_set(unsigned t, unsigned l, unsigned i, const ch_step *st)
{
    if (t >= CH_TRACKS || l >= CH_LANES || i >= CH_STEPS || !st) return -1;
    ch_step s = *st;
    if (s.vel > 127) s.vel = 127;
    if (s.ratchet < 1) s.ratchet = 1;
    if (s.ratchet > CH_RATCHET_LAST) s.ratchet = CH_RATCHET_LAST;
    if (s.chance > 100) s.chance = 100;
    if (s.cyc_b > 8) s.cyc_b = 8;
    if (s.cyc_a > s.cyc_b) s.cyc_a = s.cyc_b;
    if (s.cyc_b && !s.cyc_a) s.cyc_a = 1;
    s.tie = s.tie != 0;
    if (s.oct == 4u) s.oct = 5u;                          /* (-4: no further than three down) */
    if (s.micro > 50) s.micro = 50;
    if (s.micro < -50) s.micro = -50;
    if (memcmp(&ctl_seq[t].step[l][i], &s, sizeof s) != 0) pat_dirty = 1;
    if (s.send) ch_fx_sent = 1;
    ctl_seq[t].step[l][i] = s;         /* the audio side reads the pattern where it is */
    return 0;
}
int ch_track_init(unsigned t)
{
    if (t >= CH_TRACKS) return -1;
    track_ctl(t);                      /* what the UI reads, now; the sound on the audio side, in order */
    ch_cmd c = {OP_INIT, (uint8_t)t}; return push(c);
}
int ch_lane_note_set(unsigned t, unsigned l, unsigned note)
{
    if (t >= CH_TRACKS || l >= CH_LANES || note > 127) return -1;
    if (ctl_seq[t].note[l] != note) pat_dirty = 1;
    ctl_seq[t].note[l] = (uint8_t)note;
    return 0;
}
int ch_seq_len_set(unsigned t, unsigned len)
{
    if (t >= CH_TRACKS || len < 1 || len > CH_STEPS) return -1;
    if (ctl_seq[t].len != len) pat_dirty = 1;
    ctl_seq[t].len = (uint8_t)len;
    ch_cmd c = {OP_SEQ, (uint8_t)t, 0, (uint8_t)len}; return push(c);
}
int ch_seq_dir_set(unsigned t, unsigned d)
{
    if (t >= CH_TRACKS || d >= CH_DIRS) return -1;
    if (ctl_seq[t].dir != d) pat_dirty = 1;
    ctl_seq[t].dir = (uint8_t)d;
    ch_cmd c = {OP_SEQ, (uint8_t)t, 1, (uint8_t)d}; return push(c);
}
int ch_seq_rate_set(unsigned t, unsigned r)
{
    if (t >= CH_TRACKS || r > 4) return -1;
    if (ctl_seq[t].rate != r) pat_dirty = 1;
    ctl_seq[t].rate = (uint8_t)r;
    ch_cmd c = {OP_SEQ, (uint8_t)t, 2, (uint8_t)r}; return push(c);
}
int ch_swing_set(float a) { ctl_swing = ch_clamp(a, 0, 1); ch_cmd c = {OP_SWING}; c.f = ctl_swing; return push(c); }
static CH_NOINLINE void count_step(unsigned n)
{
    if (count_wait <= 0) {
        if (!count_left) { count_on = 0; ch_t.beat = 0; ch_t.beat_c = 0; ch_t.started = 1; ch_t.playing = 1; }
        else {
            metro_env = 1; metro_ph = 0; metro_inc = (count_n ? 1300.0f : 1800.0f) / (float)CH_RATE;
            --count_left; ++count_n; count_wait += 1.0f / ch_t.beats_per_frame;
        }
    }
    count_wait -= (float)n;
}
int ch_count_in(unsigned beats) { ch_cmd c = {OP_COUNT, 0, (uint8_t)beats}; return push(c); }
int ch_metronome_set(unsigned on) { ch_cmd c = {OP_METRO, 0, (uint8_t)(on != 0)}; return push(c); }
int ch_stutter(unsigned on, unsigned div, unsigned tracks) { ch_cmd c = {OP_STUTTER, 0, (uint8_t)(on != 0), (uint8_t)(div < CH_STUTTER_DIVS ? div : CH_STUTTER_DIVS - 1), (uint8_t)(tracks & 15u)}; return push(c); }
int ch_tempo_set(float bpm) { ctl_tempo = ch_clamp(bpm, 24, 284); ch_cmd c = {OP_TEMPO}; c.f = ctl_tempo; return push(c); }

/* ---------------- defaults ---------------- */
/* A track as it starts: the audio side's state (track_runtime) and what the UI reads (track_ctl), from the
 * same tables. ch_init runs both; ch_track_init runs track_ctl at once and track_runtime on the audio side. */
static const uint8_t DEF_DEV[CH_SLOTS] = {CH_DEV_DRUM, CH_DEV_BYPASS, CH_DEV_BYPASS, CH_DEV_BYPASS, CH_DEV_BYPASS};
static const uint8_t DEF_MOD[CH_MODS] = {CH_MOD_WAVE, CH_MOD_WAVE, CH_MOD_TRIG, CH_MOD_TRIG};   /* two LFOs, two random values a note */
static void def_modes(uint8_t mode[CH_ROWS][CH_PARAMS])
{
}
static const uint8_t DEF_MST[CH_SLOTS] = {CH_DEV_BYPASS, CH_DEV_DJ, CH_DEV_REPEAT, CH_DEV_TRANCE, CH_DEV_REVERB};
static void track_runtime(unsigned ti)
{
    ch_trk *t = ti == CH_MASTER ? &ch_mst : &ch_tr[ti];
    const uint8_t *dd0 = ti == CH_MASTER ? DEF_MST : DEF_DEV;
    memset(t, 0, sizeof *t);
    t->pwake = CH_PWAKE;
    t->ssend = t->ssg = t->ssg1 = 0;
    t->ss_dest = CH_SS_FX;                               /* the steps' SEND: into the master effects, until it is set */
    t->tofx = t->fx_s = t->fx_s1 = 0;                    /* nothing sent to the master's returns */
    for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) { t->device[s2] = dd0[s2]; t->bypass[s2] = 0; }
    for (unsigned d = 0; d < CH_DEV_FX_FIRST; ++d) {
        const ch_device_desc *dd = ch_device_info(d);
        for (unsigned p = 0; p < dd->params; ++p) {        /* (past CH_PARAMS: the engine's spare row) */
            if (p < CH_PARAMS) t->base[d][p] = t->smooth[d][p] = dd->param[p].def;
            else if (ch_xrow(d)) t->base[ch_xrow(d)][p - CH_PARAMS] = t->smooth[ch_xrow(d)][p - CH_PARAMS] = dd->param[p].def;
        }
    }
    def_modes(t->mode);
    for (unsigned m = 0; m < CH_MODS; ++m) ch_mods_init(&t->mod[m], DEF_MOD[m], 0x9e3779b9u * (ti * 4 + m + 1));
    t->level = 0.8f; t->pan = 0; t->filter = 0; t->run = 1;
    t->sample[0] = t->sample[1] = (uint8_t)ti;
    t->g_l = t->g_r = 0;
    if (ti == CH_MASTER) return;                         /* (the master: its FX slots and settings only) */
    t->grain.rng.s = 0x7A3C9E1Du + ti * 977u;
    ch_drum_init(t, ti);
    ch_synth_init(t, ti);
    t->seq.len = 16; t->seq.rate = 1; t->seq.pos = t->seq.shown = t->seq.ahead = -1; t->seq.rng.s = 0x8badf00du + ti * 131u;
    for (unsigned l = 0; l < CH_LANES; ++l) t->seq.ln[l].due = -1;
}
static void track_ctl(unsigned ti)
{
    ch_track_state *s = &ctl[ti];
    const uint8_t *dd0 = ti == CH_MASTER ? DEF_MST : DEF_DEV;
    memset(s, 0, sizeof *s);
    for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) { s->device[s2] = dd0[s2]; s->bypass[s2] = 0; }
    for (unsigned d = 0; d < CH_DEV_FX_FIRST; ++d) {
        const ch_device_desc *dd = ch_device_info(d);
        for (unsigned p = 0; p < dd->params; ++p) {
            if (p < CH_PARAMS) s->param[d][p] = dd->param[p].def;
            else if (ch_xrow(d)) s->param[ch_xrow(d)][p - CH_PARAMS] = dd->param[p].def;
        }
    }
    def_modes(s->mode);
    for (unsigned m = 0; m < CH_MODS; ++m) {
        ch_mod tmp;
        ch_mods_init(&tmp, DEF_MOD[m], 1);
        s->mod_type[m] = DEF_MOD[m];
        for (unsigned p = 0; p < CH_MOD_PARAMS; ++p) { s->mod_param[m][p] = tmp.p[p]; s->mod_mode[m][p] = tmp.m[p]; }
    }
    s->level = 0.8f; s->pan = 0; s->filter = 0;
    s->sample[0] = s->sample[1] = (uint8_t)ti;
    if (ti == CH_MASTER) return;
    {   /* an empty pattern; the lanes a C major scale up from C4 (lane 1 = the sound as tuned) */
        static const uint8_t MAJ[CH_LANES] = {60, 62, 64, 65, 67, 69, 71, 72};
        ch_seq_state *q = &ctl_seq[ti];
        memset(q->step, 0, sizeof q->step);
        q->nlock = 0;                              /* (its locks too: INIT left their dots behind) */
        for (unsigned l = 0; l < CH_LANES; ++l) {
            q->note[l] = MAJ[l];
            for (unsigned i = 0; i < CH_STEPS; ++i) { q->step[l][i].ratchet = 1; q->step[l][i].chance = 100; }
        }
        q->len = 16; q->rate = 1; q->dir = 0;
    }
    if (ti < CH_TRACKS) { ctl[ti].ss_dest = CH_SS_FX; ctl[ti].tofx = 0; }
}
static void track_defaults(unsigned ti) { track_runtime(ti); track_ctl(ti); }

static float ret_level = 0.8f, ret_g = 1, ret_pk[2];   /* the RETURN: its level knob (written by the control side, a float whole), its gain gliding, its meter */
void ch_init(void)
{
    pm_reset = 1;
    memset(&ch_t, 0, sizeof ch_t);
    memset(send_bus, 0, sizeof send_bus); bus_live[0] = bus_live[1] = 0;
    lim_reset(); sub_l = sub_r = 0;                      /* the limiter and the subsonic filter as new */
    ch_fx_sent = 0; ch_fx_retmask = 0; ret_level = 0.8f; ret_g = 1; ret_pk[0] = ret_pk[1] = 0;
    ch_t.bpm = ch_t.bpm_target = 120;
    for (unsigned i = 0; i <= CH_MASTER; ++i) track_defaults(i);
    memset(ch_voices, 0, sizeof ch_voices);
    memset(send_bus, 0, sizeof send_bus);
    ring_w = ring_r = 0;
    ch_mem_init();
}

/* ---------------- patterns: the packed form, the stash, the switch at the bar ---------------- */
static uint8_t pat_stash[CH_PAT_MAX] CH_POOL;         /* the stash, packed (a raw one is 12 KB) */
static uint8_t pat_tmp[CH_PAT_MAX] CH_POOL;           /* a swap's other half */
static unsigned stash_len;
static uint8_t stash_valid;
static const uint8_t *pat_next;                 /* a switch waiting: its packed pattern */
static volatile uint8_t pat_kind;               /* 0 none, 1 packed, 2 empty, 3 the stash */
static uint8_t pat_keep, pat_now;
enum { PAT_MAGIC0 = 'P', PAT_MAGIC1 = 'A', PAT_MAGIC2 = 'T', PAT_MAGIC3 = '7', PAT_MAGIC3_V6 = '6', PAT_MAGIC3_V5 = '5', PAT_MAGIC3_V4 = '4', PAT_MAGIC3_V3 = '3', PAT_MAGIC3_OLD = '2' };   /* 3: with the locks; 4: the rolls; 5: the gate */
/* a step in 32 bits. Format 4: velocity 7, ratchet 3 (bits 7-8 and 22), chance 7, the cycle pair as one index (6 bits: (a, b)
 * with a <= b <= 8), micro 7, tie 1. Format 3 held the cycle as two nibbles and the ratchet in two bits. */
static unsigned cyc_index(unsigned a, unsigned b) { return b ? b * (b - 1) / 2 + a : 0; }
static uint32_t step_pack(const ch_step *st)
{
    unsigned r = st->ratchet ? st->ratchet - 1u : 0, m = (unsigned)(st->micro + 50);
    return (uint32_t)(st->vel & 127u) | (r & 3u) << 7 | (st->chance & 127u) << 9 | (cyc_index(st->cyc_a, st->cyc_b) & 63u) << 16 | (r >> 2 & 1u) << 22
        | (m & 127u) << 24 | (st->tie ? 1u << 31 : 0u);
}
static ch_step step_unpack(uint32_t v, int v3)
{
    ch_step st = {0};                                   /* (ungate 0: the full step, unless the record says) */
    st.vel = (uint8_t)(v & 127u); st.chance = (uint8_t)(v >> 9 & 127u); st.micro = (int8_t)((int)(v >> 24 & 127u) - 50);
    st.tie = (uint8_t)(v >> 31);
    if (v3) { st.ratchet = (uint8_t)((v >> 7 & 3u) + 1u); st.cyc_a = (uint8_t)(v >> 16 & 15u); st.cyc_b = (uint8_t)(v >> 20 & 15u); }
    else {
        st.ratchet = (uint8_t)(((v >> 7 & 3u) | (v >> 22 & 1u) << 2) + 1u);
        unsigned ci = v >> 16 & 63u, b = 0; st.cyc_a = st.cyc_b = 0;
        if (ci) { while (b < 8 && cyc_index(b, b) < ci) ++b; st.cyc_b = (uint8_t)b; st.cyc_a = (uint8_t)(ci - cyc_index(0, b)); }
    }
    if (st.chance > 100) st.chance = 100;
    if (st.ratchet > CH_RATCHET_LAST) st.ratchet = 1;
    return st;
}
#define PAT_PLAIN (100u | 100u << 9 | 50u << 24)      /* a plain hit: velocity 100, no ratchet, every time, on the grid */
static unsigned pack_from(const ch_seq_state *q4, uint8_t *out, unsigned cap)
{
    unsigned n = 0;
    if (cap < 4) return 0;
    out[n++] = PAT_MAGIC0; out[n++] = PAT_MAGIC1; out[n++] = PAT_MAGIC2; out[n++] = PAT_MAGIC3;
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        const ch_seq_state *q = &q4[t];
        if (n + 3 + CH_LANES + CH_LANES * 8 + 2 > cap) return 0;
        out[n++] = q->len; out[n++] = q->dir; out[n++] = q->rate;
        for (unsigned l = 0; l < CH_LANES; ++l) out[n++] = q->note[l];
        for (unsigned l = 0; l < CH_LANES; ++l)
            for (unsigned b = 0; b < 8; ++b) { uint8_t m = 0; for (unsigned k = 0; k < 8; ++k) if (q->step[l][b * 8 + k].vel) m |= (uint8_t)(1u << k); out[n++] = m; }
        unsigned at = n; n += 2; unsigned recs = 0;
        for (unsigned l = 0; l < CH_LANES; ++l)
            for (unsigned i = 0; i < CH_STEPS; ++i) {
                const ch_step *st = &q->step[l][i];
                if (!st->vel) continue;
                uint32_t v = step_pack(st);
                if (v == PAT_PLAIN && !st->ungate && !st->send && !st->oct) continue;
                if (n + 8 > cap) return 0;
                out[n++] = (uint8_t)l; out[n++] = (uint8_t)i;
                out[n++] = (uint8_t)v; out[n++] = (uint8_t)(v >> 8); out[n++] = (uint8_t)(v >> 16); out[n++] = (uint8_t)(v >> 24);
                out[n++] = (uint8_t)(st->ungate | st->oct << 4);   /* (format 7: the gate in fifteenths, the octave over it) */
                out[n++] = st->send;                          /* (format 6) */
                ++recs;
            }
        out[at] = (uint8_t)recs; out[at + 1] = (uint8_t)(recs >> 8);
    }
    for (unsigned t = 0; t < CH_TRACKS; ++t) {           /* the parameter locks */
        const ch_seq_state *q = &q4[t];
        if (n + 1 + 5u * q->nlock > cap) return 0;
        out[n++] = q->nlock;
        for (unsigned i = 0; i < q->nlock; ++i) {
            out[n++] = q->lock[i].step; out[n++] = q->lock[i].slot; out[n++] = q->lock[i].param;
            out[n++] = (uint8_t)q->lock[i].v; out[n++] = (uint8_t)((uint16_t)q->lock[i].v >> 8);
        }
    }
    return n;
}
/* 0 = unpacked; else the data was not a pattern (nothing changed) */
static int unpack_to(const uint8_t *in, ch_seq_state *q4)
{
    unsigned n = 4;
    if (in[0] != PAT_MAGIC0 || in[1] != PAT_MAGIC1 || in[2] != PAT_MAGIC2 || (in[3] != PAT_MAGIC3 && in[3] != PAT_MAGIC3_V6 && in[3] != PAT_MAGIC3_V5 && in[3] != PAT_MAGIC3_V4 && in[3] != PAT_MAGIC3_V3 && in[3] != PAT_MAGIC3_OLD)) return -1;
    int locks = in[3] != PAT_MAGIC3_OLD, v3 = in[3] == PAT_MAGIC3_V3 || in[3] == PAT_MAGIC3_OLD, v7 = in[3] == PAT_MAGIC3, gated = v7 || in[3] == PAT_MAGIC3_V6 || in[3] == PAT_MAGIC3_V5, sends = v7 || in[3] == PAT_MAGIC3_V6;
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        ch_seq_state *q = &q4[t];
        unsigned len = in[n++], dir = in[n++], rate = in[n++];
        q->len = (uint8_t)(len >= 1 && len <= CH_STEPS ? len : 16); q->dir = (uint8_t)(dir < CH_DIRS ? dir : 0); q->rate = (uint8_t)(rate < 5 ? rate : 1);
        for (unsigned l = 0; l < CH_LANES; ++l) { unsigned note = in[n++]; q->note[l] = (uint8_t)(note <= 127 ? note : 60); }
        ch_step plain = step_unpack(PAT_PLAIN, 0), off = {0, 1, 100, 0, 0, 0};
        for (unsigned l = 0; l < CH_LANES; ++l)
            for (unsigned b = 0; b < 8; ++b) { uint8_t m = in[n++]; for (unsigned k = 0; k < 8; ++k) q->step[l][b * 8 + k] = (m >> k & 1u) ? plain : off; }
        unsigned recs = in[n] | (unsigned)in[n + 1] << 8; n += 2;
        for (unsigned r = 0; r < recs; ++r) {
            unsigned l = in[n], i = in[n + 1];
            uint32_t v = (uint32_t)in[n + 2] | (uint32_t)in[n + 3] << 8 | (uint32_t)in[n + 4] << 16 | (uint32_t)in[n + 5] << 24;
            n += 6;
            unsigned ug = gated ? in[n++] : 0, sd = sends ? in[n++] : 0;
            if (l < CH_LANES && i < CH_STEPS) { q->step[l][i] = step_unpack(v, v3); if (v7) { q->step[l][i].ungate = (uint8_t)(ug & 15u); q->step[l][i].oct = (uint8_t)((ug >> 4 & 7u) == 4u ? 0u : ug >> 4 & 7u); } else ch_step_gate_set(&q->step[l][i], ug <= 100 ? 100u - ug : 100u);   /* (to format 6: the gate in hundredths) */ q->step[l][i].send = (uint8_t)(sd & 15u); if (sd & 15u) ch_fx_sent = 1; }
        }
        q->nlock = 0;
    }
    if (locks)
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            ch_seq_state *q = &q4[t];
            unsigned cnt = in[n++], k = 0;
            for (unsigned i = 0; i < cnt; ++i, n += 5) {
                if (k >= CH_LOCKS || in[n] >= CH_STEPS || in[n + 1] >= CH_SLOTS || in[n + 2] >= CH_PARAMS) continue;
                q->lock[k].step = in[n]; q->lock[k].slot = in[n + 1]; q->lock[k].param = in[n + 2];
                q->lock[k].v = (int16_t)((uint16_t)in[n + 3] | (uint16_t)in[n + 4] << 8); ++k;
            }
            q->nlock = (uint8_t)k;
        }
    return 0;
}
unsigned ch_pattern_pack(uint8_t *out, unsigned cap) { return pack_from(ctl_seq, out, cap); }
/* the app's one-step undo: ch_undo_mark packs the pattern before a sweeping change; ch_undo_swap puts it back and keeps
 * what it replaced, so a second call redoes. At once, on the control side, as step edits are */
static int8_t scope_buf[CH_SCOPE_N] CH_POOL;        /* the master or one track, for the spectrum */
static volatile uint16_t scope_w;
static uint8_t scope_tick;
static volatile uint8_t scope_src;
static float scope_prev;
const int8_t *ch_scope_data(unsigned *wpos) { *wpos = scope_w; return scope_buf; }
void ch_scope_source(unsigned src) { scope_src = (uint8_t)(src <= CH_TRACKS ? src : 0); }
static uint8_t pat_undo[CH_PAT_MAX] CH_POOL;
static unsigned undo_len;
static uint8_t pat_kept[CH_PAT_MAX] CH_POOL; static unsigned kept_len;
int ch_keep_mark(void) { kept_len = pack_from(ctl_seq, pat_kept, CH_PAT_MAX); return kept_len ? 0 : -1; }
int ch_keep_back(void)
{
    if (!kept_len) return -1;
    if (unpack_to(pat_kept, ctl_seq) != 0) { kept_len = 0; return -1; }
    for (unsigned t = 0; t < CH_TRACKS; ++t) {           /* the running sequencers' length, direction and rate follow */
        ch_cmd c = {OP_SEQ, (uint8_t)t, 0, ctl_seq[t].len}; push(c);
        ch_cmd d = {OP_SEQ, (uint8_t)t, 1, ctl_seq[t].dir}; push(d);
        ch_cmd r = {OP_SEQ, (uint8_t)t, 2, ctl_seq[t].rate}; push(r);
    }
    pat_dirty = 1;
    return 0;
}
int ch_undo_mark(void) { undo_len = pack_from(ctl_seq, pat_undo, CH_PAT_MAX); return undo_len ? 0 : -1; }
int ch_undo_swap(void)
{
    if (!undo_len) return -1;
    unsigned n = pack_from(ctl_seq, pat_tmp, CH_PAT_MAX);
    if (unpack_to(pat_undo, ctl_seq) != 0) { undo_len = 0; return -1; }
    if (n) { memcpy(pat_undo, pat_tmp, n); undo_len = n; } else undo_len = 0;
    for (unsigned t = 0; t < CH_TRACKS; ++t) {           /* the running sequencers' length, direction and rate follow */
        ch_cmd c = {OP_SEQ, (uint8_t)t, 0, ctl_seq[t].len}; push(c);
        ch_cmd d = {OP_SEQ, (uint8_t)t, 1, ctl_seq[t].dir}; push(d);
        ch_cmd r = {OP_SEQ, (uint8_t)t, 2, ctl_seq[t].rate}; push(r);
    }
    pat_dirty = 1;
    return 0;
}
unsigned ch_stash_pack(uint8_t *out, unsigned cap) { if (!stash_valid || stash_len > cap) return 0; memcpy(out, pat_stash, stash_len); return stash_len; }
int ch_stash_valid(void) { return stash_valid; }
void ch_stash_drop(void) { stash_valid = 0; }
int ch_pattern_switch(const uint8_t *packed, int at_bar, int keep)
{
    if (pat_kind) return -1;                    /* one at a time: the last waits for its bar */
    pat_next = packed; pat_keep = (uint8_t)(keep != 0); pat_now = (uint8_t)!at_bar;
    pat_kind = packed ? 1 : 2;
    return 0;
}
int ch_pattern_restore(int at_bar, int keep)
{
    if (pat_kind || !stash_valid) return -1;
    pat_keep = (uint8_t)(keep != 0); pat_now = (uint8_t)!at_bar;
    pat_kind = 3;
    return 0;
}
/* the switch itself (audio side, at the bar or at once): the stash, the new pattern, the tracks' settings */
static void pattern_apply(void)
{
    unsigned kind = pat_kind;
    if (kind == 3) {
        if (pat_keep) {                                  /* a swap: the current packed aside, the stash in, then into the stash */
            unsigned n2 = pack_from(ctl_seq, pat_tmp, CH_PAT_MAX);
            unpack_to(pat_stash, ctl_seq);
            if (n2) { memcpy(pat_stash, pat_tmp, n2); stash_len = n2; } else stash_valid = 0;   /* (too detailed: lost) */
        } else { unpack_to(pat_stash, ctl_seq); stash_valid = 0; }
    } else {
        if (pat_keep) { stash_len = pack_from(ctl_seq, pat_stash, CH_PAT_MAX); stash_valid = stash_len != 0; }
        if (kind == 2 || !pat_next || unpack_to(pat_next, ctl_seq) != 0) {
            for (unsigned t = 0; t < CH_TRACKS; ++t) {       /* empty: the steps off, the lanes and settings kept */
                ch_step off = {0, 1, 100, 0, 0, 0};
                for (unsigned l = 0; l < CH_LANES; ++l) for (unsigned i = 0; i < CH_STEPS; ++i) ctl_seq[t].step[l][i] = off;
            }
        }
    }
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        ch_seq *q = &ch_tr[t].seq;
        q->len = ctl_seq[t].len; q->dir = ctl_seq[t].dir; q->rate = ctl_seq[t].rate;
        if (q->pos >= q->len) q->pos = -1;
    }
    pat_dirty = 0;
    pat_kind = 0;
}

/* ---------------- audio side: notes and the sequencer ---------------- */
/* the voice budget: with as many drum and synth voices sounding as the FM-1 can render, a new note first fades
 * out the quietest of them (one not already going; a hit under 50 ms old is spared). The limit is the load governor's (ch_render): CH_VOICE_BUDGET
 * while the render has time to spare, fewer while it is near its deadline */
static unsigned vb_cap = CH_VOICE_BUDGET;
uint32_t ch_cut_kit[8], ch_cut_budget[4], ch_cut_bpart[8], ch_guard_trips[CH_SLOTS];
ch_dbg_t ch_dbg;
/* how much a voice still has to say: a drum's level, a synth's envelope (a released one counts for less) */
CH_SI float dvoice_weight(const ch_dvoice *v) { return ch_dvoice_worth(v); }   /* (a fresh hit is kept; a low drum counts double) */
CH_SI float svoice_weight(const ch_svoice *v) { return v->env * v->fade * (v->gate ? 1.0f : 0.5f) + (v->n < 2205 ? 1.0f : 0); }   /* (a released note half) */
/* the voice that matters least faded out until no more than keep sound. own: the track whose new note asks for room, -1 none:
 * a SYNTH's own older notes count half, so a new chord gives up its last chord before another track's kick */
static unsigned voice_shed_for(unsigned keep, int own)   /* (in the flash: it runs at a note, not a frame; its place in the RAM is ANALOG's voice's) */
{
    /* Which voice goes when there are too many: a muted track's first (no one hears it), then the one that matters least.
     * A note under 100 ms old is never taken: a hit or a note just begun is what the listener follows, and cutting it is
     * a step gone silent. When only young notes are left the count may stand over the limit for a moment instead; the
     * governor answers sustained load, not one note */
    unsigned shed = 0;
    unsigned live = 0;
    const uint32_t young = (uint32_t)(CH_RATE / 10);
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        const ch_trk *k = &ch_tr[t];
        for (unsigned i = 0; i < CH_DVOICES; ++i) live += k->drum.v[i].on && !k->drum.v[i].kill;
        for (unsigned i = 0; i < CH_SVOICES; ++i) live += k->synth.v[i].on && !k->synth.v[i].kill;
    }
    for (unsigned i = 0; i < CH_POLY_VOICES; ++i) live += ch_voices[i].active && !ch_voices[i].kill;   /* the SAMPLE engine's notes too */
    while (live > keep) {
        float least = 1e30f; uint8_t *kill = 0; const ch_dvoice *kd = 0; const ch_svoice *ks = 0;
        int spare = live <= keep + 4;                    /* young notes kept up to 4 over the limit; past that (a pile-up of chords at once) even they go */
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            ch_trk *k = &ch_tr[t];
            int muted = k->mute;
            for (unsigned i = 0; i < CH_DVOICES; ++i) {
                ch_dvoice *v = &k->drum.v[i];
                if (!v->on || v->kill || (spare && !muted && v->n < young)) continue;
                float w = muted ? -1.0f : dvoice_weight(v);
                if (w < least) { least = w; kill = &v->kill; kd = v; ks = 0; }
            }
            for (unsigned i = 0; i < CH_SVOICES; ++i) {
                ch_svoice *v = &k->synth.v[i];
                if (!v->on || v->kill || (spare && !muted && v->n < young)) continue;
                float w = muted ? -1.0f : svoice_weight(v) * ((int)t == own ? 0.5f : 1.0f);
                if (w < least) { least = w; kill = &v->kill; kd = 0; ks = v; }
            }
        }
        for (unsigned i = 0; i < CH_POLY_VOICES; ++i) {
            ch_voice *v = &ch_voices[i];
            int muted = ch_tr[v->track & 3u].mute;
            if (!v->active || v->kill || (spare && !muted && v->n < young)) continue;
            float w = muted ? -1.0f : v->env * (v->stage < 4 ? 1.0f : 0.5f);
            if (w < least) { least = w; kill = &v->kill; kd = 0; ks = 0; }
        }
        if (!kill) break;                                    /* only young notes left (and within 4 of the limit): none of them is cut */
        if (kd ? kd->n < young : ks ? ks->n < young : 0) ++ch_dbg.cut_budget;
        if (least > 0.1f) for (unsigned t = 0; t < CH_TRACKS; ++t)
            if ((uint8_t *)kill >= (uint8_t *)&ch_tr[t] && (uint8_t *)kill < (uint8_t *)(&ch_tr[t] + 1)) { ++ch_cut_budget[t]; if (kd && kd->part < 8) ++ch_cut_bpart[kd->part]; }
        *kill = 1; --live; ++shed;
    }
    return shed;
}
static unsigned voice_shed(unsigned keep) { return voice_shed_for(keep, -1); }
static void voice_budget(unsigned ti) { voice_shed_for(vb_cap - 1, ch_tr[ti].device[CH_MATERIAL] == CH_DEV_SYNTH ? (int)ti : -1); }   /* room for one more */
static CH_AUDIO_CODE void track_note(unsigned ti, unsigned note, unsigned vel)
{
    ch_trk *t = &ch_tr[ti];
    int silent = vel && t->mute;                         /* muted: no voice is started (nothing heard, nothing taken from the others) */
#ifdef CHISEL_HOST
    if (ch_note_tap && !silent) ch_note_tap(ti, note, vel, ch_sub_off);
#endif
    if (vel && !silent && !t->bypass[CH_MATERIAL] && (t->device[CH_MATERIAL] == CH_DEV_DRUM || t->device[CH_MATERIAL] == CH_DEV_KIT || t->device[CH_MATERIAL] == CH_DEV_SYNTH || t->device[CH_MATERIAL] == CH_DEV_POLY)) voice_budget(ti);
#ifdef CHISEL_HOST
    if (HOST_FX) silent = 1;
#endif
    if (!t->bypass[CH_MATERIAL] && !silent) {
        if (t->device[CH_MATERIAL] == CH_DEV_POLY) ch_poly_note(t, ti, note, vel);
        else if (t->device[CH_MATERIAL] == CH_DEV_GRAIN) ch_grain_note(t, ti, note, vel);
        else if (t->device[CH_MATERIAL] == CH_DEV_DRUM) ch_drum_note(t, ti, note, vel);
        else if (t->device[CH_MATERIAL] == CH_DEV_KIT && vel) {   /* KIT: the lane whose note it is plays its part (else the nearest) */
            const ch_seq_state *q = &ctl_seq[ti]; unsigned part = 0; int best = 1000;
            for (unsigned l = 0; l < CH_KIT_PARTS && l < CH_LANES; ++l) { int d = (int)q->note[l] - (int)note; if (d < 0) d = -d; if (d < best) { best = d; part = l; } }
            ch_kit_note(t, ti, part, vel);
        }
        else if (t->device[CH_MATERIAL] == CH_DEV_SYNTH) ch_synth_note(t, ti, note, vel);
    }
    if (vel && t->device[CH_MATERIAL] == CH_DEV_KIT) {   /* a KIT's hit, counted on its part (muted or not, as the track's are) */
        const ch_seq_state *q = &ctl_seq[ti]; unsigned part = 0; int best = 1000;
        for (unsigned l = 0; l < CH_KIT_PARTS && l < CH_LANES; ++l) { int d = (int)q->note[l] - (int)note; if (d < 0) d = -d; if (d < best) { best = d; part = l; } }
        ++ch_lane_hits[ti][part];
    }
    if (vel) { ++t->notes_held; t->drum.flash = 1; ++ch_hits[ti]; ch_last_note[ti] = (uint8_t)(note ? note : 1); } else if (t->notes_held) --t->notes_held;
    ch_mod_note(t, vel != 0);
}


/* the sequencer's counts for the bench (hits sent, steps skipped by CHANCE and CYCLE, ties held): not kept on the FM-1,
 * where this code runs from the little RAM there is for it and nothing reads them */
#ifdef CHISEL_FM1
#define DBG(x) ((void)0)
#else
#define DBG(x) (++ch_dbg.x)
#endif
static const float RATE_BEATS[5] = {0.125f, 0.25f, 1.0f / 6, 0.5f, 1.0f};
static void lock_apply(unsigned ti, int step);
/* a roll's velocity for the hit about to sound: XF IN fades in from a quarter along a curve, XF OUT fades out to one;
 * the X pair keep the step's velocity */
static unsigned roll_vel(const ch_lane_run *L)
{
    float k = (float)L->rat_left / 3.0f;                 /* 1 at the first hit, 0 at the last */
    float f = L->rat_kind == 3 ? 0.25f + 0.75f * (1 - k) * (1 - k) : L->rat_kind == 4 ? 0.25f + 0.75f * k * k : 1.0f;
    unsigned v = (unsigned)(L->rat_vel * f + 0.5f); return v < 1 ? 1 : v > 127 ? 127 : v;
}
/* how much of a step (or of a ratchet's slot) a note is held at a GATE of g %: the whole of it at 100 but a hair, so the
 * next note starts clean; a blip, never nothing, at 0 */
static float gate_share(unsigned g) { float f = (float)g * 0.0097f; return f < 0.03f ? 0.03f : f; }
static CH_AUDIO_CODE void lane_hit(unsigned ti, ch_seq *s, unsigned l, unsigned vel, ch_tf at, unsigned ratchet, float sb, unsigned longer)
{
    ch_lane_run *L = &s->ln[l];
    float gs = gate_share(L->due_gate);
    L->gate = L->due_gate;
    {   /* the step's SEND: what this hit adds to the track's send, from here on (two hits at once: the larger) */
        ch_trk *tk = &ch_tr[ti]; float sc = (float)L->due_snd * (1.0f / 15);
        if (!tk->ss_set || sc > tk->ss_new) tk->ss_new = sc;
        tk->ss_set = 1;
    }
    unsigned note = ctl_seq[ti].note[l];
    {   /* the step's octave. A KIT's lane is a drum: its part struck that much higher or lower (the note stays the lane's: it
         * picks the part). Any other: the note itself, unless that is past the keyboard's end */
        int o = 12 * ((int)(L->due_oct ^ 4u) - 4); unsigned n2 = note + (unsigned)o;   /* (in semitones) */
        if (ch_tr[ti].device[CH_MATERIAL] == CH_DEV_KIT) ch_tr[ti].drum.kit_oct[l] = (int8_t)o;
        else if (n2 - 1u < 127u) note = n2;
    }
    if (L->gate_note) { track_note(ti, L->gate_note, 0); L->gate_note = 0; }
    L->gate_note = (uint8_t)(note ? note : 1);
    L->rat_kind = 0; L->rat_mul = 1;
    if (ratchet >= CH_RATCHET_X_IN) {                      /* a roll: 4 hits, the gaps a geometric series over 92 % of the step */
        int in = ratchet == CH_RATCHET_X_IN || ratchet == CH_RATCHET_XF_IN;
        float r = in ? 0.6f : 1 / 0.6f, g0 = sb * 0.92f * (1 - r) / (1 - r * r * r);   /* the first gap: the three sum to the span */
        L->rat_kind = (uint8_t)(ratchet - CH_RATCHET_X_IN + 1); L->rat_mul = r; L->rat_left = 3; L->rat_gap = g0; L->rat_next = at + g0;
        L->rat_vel = (uint8_t)vel;
        L->gate_off = at + g0 * gs;
        track_note(ti, note, roll_vel(L));                /* the first hit at the roll's starting level */
        return;
    }
    track_note(ti, note, vel); DBG(hits);
    L->gate_off = longer && ratchet <= 1 ? at + sb * 1.08f : at + sb * gs / (float)ratchet;   /* a tie follows: into the next step */
    if (ratchet > 1) {
        L->rat_left = (uint8_t)(ratchet - 1); L->rat_gap = sb / (float)ratchet; L->rat_next = at + L->rat_gap;
        L->rat_vel = (uint8_t)(vel * 0.8f + 1);
    } else L->rat_left = 0;
}
/* the stutter (a performance hold): the step sounding when it began repeats at a division on every track while the
 * sequencers count on in silence underneath, so letting go lands where the pattern would have been */
static const ch_tf STUT_BEATS[CH_STUTTER_DIVS] = {0.5, 0.25, 0.125, 0.0625};   /* 1/8 1/16 1/32 1/64 */
static uint8_t stut_on, stut_mask;   /* the tracks held (bits) */
static ch_tf stut_stop = -1;         /* let go: the 1/8 the hold ends on (the pattern's own step there sounds), -1 still held */
static int stut_holds(unsigned ti, ch_tf at) { return stut_on && (stut_mask >> ti & 1u) && (stut_stop < 0 || at < stut_stop - (ch_tf)1e-4f); }
static ch_tf stut_div, stut_next[CH_TRACKS];
static int16_t stut_step[CH_TRACKS];
static void stutter_set(unsigned on, unsigned div, unsigned mask)
{
    stut_div = STUT_BEATS[div < CH_STUTTER_DIVS ? div : 1];
    if (!on) {                                           /* let go: on to the end of the 1/8 it is in, so the pattern comes back on the beat */
        if (stut_on && stut_stop < 0) { int32_t k = (int32_t)(ch_t.beat * 2); stut_stop = (ch_tf)k * (ch_tf)0.5f; if (stut_stop < ch_t.beat - (ch_tf)1e-4f) stut_stop += (ch_tf)0.5f; }
        return;
    }
    if (stut_on) { stut_stop = -1; return; }             /* (a turn while it is ending: held again, at the new rate) */
    if (!ch_t.playing) return;
    stut_stop = -1;
    stut_mask = (uint8_t)(mask ? mask : 15u);
    int32_t n = (int32_t)(ch_t.beat / stut_div); if ((ch_tf)n * stut_div < ch_t.beat) ++n;   /* the first hit on the grid */
    for (unsigned ti = 0; ti < CH_TRACKS; ++ti) {
        if (!(stut_mask >> ti & 1u)) continue;
        ch_seq *s = &ch_tr[ti].seq;
        int st = s->shown >= 0 ? s->shown : s->pos >= 0 ? s->pos : 0;
        stut_step[ti] = (int16_t)st; stut_next[ti] = (ch_tf)n * stut_div;
        s->shown = (int16_t)st; s->ahead = -1;
        for (unsigned l = 0; l < CH_LANES; ++l) { s->ln[l].due = -1; s->ln[l].rat_left = 0; }
    }
    stut_on = 1;
}
static void stutter_run(unsigned ti, ch_tf b1)
{
    ch_seq *s = &ch_tr[ti].seq;
    const ch_seq_state *q = &ctl_seq[ti];
    while (stut_next[ti] < b1 && (stut_stop < 0 || stut_next[ti] < stut_stop - (ch_tf)1e-4f)) {
        for (unsigned l = 0; l < CH_LANES; ++l) {
            const ch_step *st = &q->step[l][stut_step[ti]];
            if (!st->vel) continue;
            ch_lane_run *L = &s->ln[l];
            unsigned note = q->note[l];
            if (L->gate_note) track_note(ti, L->gate_note, 0);
            track_note(ti, note, st->vel);
            L->gate_note = (uint8_t)(note ? note : 1); L->gate_off = stut_next[ti] + stut_div * 0.5f; L->rat_left = 0;
        }
        stut_next[ti] += stut_div;
    }
}
static void seq_advance(ch_seq *s)
{
    int len = s->len ? s->len : 16, p = s->pos;
    switch (s->dir) {
    case CH_DIR_REV: if (p < 0) p = len - 1; else if (--p < 0) { p = len - 1; ++s->pass; } break;
    case CH_DIR_PEND:
        if (p < 0) { p = 0; s->pend_dir = 1; break; }
        if (len == 1) { ++s->pass; break; }
        if (!s->pend_dir) s->pend_dir = 1;             /* switched to PEND mid-play: the direction was never set */
        p += s->pend_dir;
        if (p >= len) { p = len - 2; s->pend_dir = -1; }
        if (p < 0) { p = 1; s->pend_dir = 1; ++s->pass; }
        break;
    case CH_DIR_RND: p = (int)(ch_rand(&s->rng) % (uint32_t)len); s->pass = s->count / (uint32_t)len; break;
    default: if (p < 0) p = 0; else if (++p >= len) { p = 0; ++s->pass; } break;
    }
    s->pos = (int16_t)p;
}
static CH_AUDIO_CODE void seq_run(unsigned ti, ch_tf b0, ch_tf b1)
{
    ch_seq *s = &ch_tr[ti].seq;
    const ch_seq_state *q = &ctl_seq[ti];
    float sb = RATE_BEATS[s->rate < 5 ? s->rate : 1];
    if (ch_t.started) {
        s->pos = -1; s->shown = -1; s->ahead = -1; s->next = 0; s->count = 0; s->pass = 0;
        for (unsigned l = 0; l < CH_LANES; ++l) { s->ln[l].due = -1; s->ln[l].rat_left = 0; }
    }
    /* look half a step ahead so a cell pushed early still lands on time */
    while (s->next - sb * 0.5f < b1) {
        seq_advance(s);
        if (stut_holds(ti, s->next)) { s->ahead = -1; ++s->count; s->next += sb; continue; }   /* held: counting on in silence (up to where it ends) */
        if (s->ahead >= 0) { s->shown = s->ahead; s->shown_at = s->ahead_at; lock_apply(ti, s->shown); }   /* (a step can only be one ahead) */
        s->ahead = s->pos; s->ahead_at = s->next;
        ch_tf base = s->next;
        if ((s->count & 1u) && s->rate <= 1) base += swing * 0.5f * sb;
        for (unsigned l = 0; l < CH_LANES; ++l) {
            const ch_step *st = &q->step[l][s->pos];
            if (!st->vel) continue;
            if (st->chance < 100 && ch_rand(&s->rng) % 100u >= st->chance) { DBG(chance); continue; }
            if (st->cyc_b && (s->pass % st->cyc_b) != (uint32_t)(st->cyc_a - 1)) { DBG(cycle); continue; }
            ch_lane_run *L = &s->ln[l];
            ch_tf at = base + (ch_tf)st->micro * 0.01f * sb;
            if (at < b0) at = b0;
            const ch_step *nx = &q->step[l][(unsigned)(s->pos + 1) % (unsigned)(s->len ? s->len : 16)];
            unsigned longer = nx->vel && nx->tie;
            unsigned mdev = ch_tr[ti].device[CH_MATERIAL];
            int held = mdev != CH_DEV_DRUM && mdev != CH_DEV_KIT;   /* a drum has nothing to hold: a tied step on one plays its hit (never a silent step) */
            if (held && st->tie && L->gate_note && L->due < 0) {   /* tied: the note before holds on through this step */
                L->gate_off = longer && st->ratchet <= 1 ? at + sb * 1.08f : at + sb * gate_share(ch_step_gate(st));   /* (the tied step's own GATE ends it) */
                DBG(tie);
                continue;
            }
            if (L->due >= 0) lane_hit(ti, s, l, L->due_vel, L->due, L->due_rat, sb, L->due_long);
            L->due = at; L->due_vel = st->vel; L->due_rat = st->ratchet; L->due_long = (uint8_t)longer; L->due_gate = (uint8_t)ch_step_gate(st); L->due_snd = st->send; L->due_oct = st->oct;
        }
        ++s->count;
        s->next += sb;
    }
    if (s->ahead >= 0 && s->ahead_at < b1) { s->shown = s->ahead; s->shown_at = s->ahead_at; s->ahead = -1; lock_apply(ti, s->shown); }
    if (stut_on && (stut_mask >> ti & 1u)) stutter_run(ti, b1);
    for (unsigned l = 0; l < CH_LANES; ++l) {
        ch_lane_run *L = &s->ln[l];
        if (L->due >= 0 && L->due < b1) { lane_hit(ti, s, l, L->due_vel, L->due, L->due_rat, sb, L->due_long); L->due = -1; }
        if (L->rat_left && L->rat_next < b1) {         /* the ratchet's next hit (the last one's gate may have closed) */
            unsigned note = ctl_seq[ti].note[l];
            if (L->gate_note) track_note(ti, L->gate_note, 0);
            --L->rat_left;
            track_note(ti, note, L->rat_kind ? roll_vel(L) : L->rat_vel);
            L->gate_note = (uint8_t)(note ? note : 1);
            if (L->rat_kind) L->rat_gap *= L->rat_mul;
            L->gate_off = L->rat_next + L->rat_gap * gate_share(L->gate);
            L->rat_next += L->rat_gap;
        }
        if (L->gate_note && L->gate_off < b1) { track_note(ti, L->gate_note, 0); L->gate_note = 0; }
    }
}
static void seq_stop(unsigned ti)
{
    ch_seq *s = &ch_tr[ti].seq;
    for (unsigned l = 0; l < CH_LANES; ++l) {
        ch_lane_run *L = &s->ln[l];
        if (L->gate_note) { track_note(ti, L->gate_note, 0); L->gate_note = 0; }
        L->due = -1; L->rat_left = 0;
    }
    s->pos = s->shown = s->ahead = -1;
    lock_apply(ti, -1);                           /* stopped: the knobs' own values again */
}

/* ---- parameter locks (the pattern's; applied on the audio side when the step comes) ---- */
int ch_lock_set(unsigned t, unsigned step, unsigned slot, unsigned param, float value)
{
    if (t >= CH_TRACKS || step >= CH_STEPS || slot >= CH_SLOTS || param >= CH_PARAMS) return -1;
    ch_seq_state *q = &ctl_seq[t];
    int16_t v = (int16_t)(ch_clamp(value, -127, 127) * 256.0f + (value < 0 ? -0.5f : 0.5f));
    for (unsigned i = 0; i < q->nlock; ++i)
        if (q->lock[i].step == step && q->lock[i].slot == slot && q->lock[i].param == param) { if (q->lock[i].v != v) pat_dirty = 1; q->lock[i].v = v; return 0; }
    if (q->nlock >= CH_LOCKS) return -1;
    q->lock[q->nlock].step = (uint8_t)step; q->lock[q->nlock].slot = (uint8_t)slot; q->lock[q->nlock].param = (uint8_t)param; q->lock[q->nlock].v = v;
    ++q->nlock; pat_dirty = 1;
    return 0;
}
int ch_lock_get(unsigned t, unsigned step, unsigned slot, unsigned param, float *value)
{
    if (t >= CH_TRACKS) return 0;
    const ch_seq_state *q = &ctl_seq[t];
    for (unsigned i = 0; i < q->nlock; ++i)
        if (q->lock[i].step == step && q->lock[i].slot == slot && q->lock[i].param == param) { if (value) *value = (float)q->lock[i].v * (1.0f / 256); return 1; }
    return 0;
}
void ch_lock_clear(unsigned t, unsigned step)
{
    if (t >= CH_TRACKS) return;
    ch_seq_state *q = &ctl_seq[t];
    unsigned k = 0;
    for (unsigned i = 0; i < q->nlock; ++i) if (step < CH_STEPS && q->lock[i].step != step) q->lock[k++] = q->lock[i];
    if (k != q->nlock) pat_dirty = 1;
    q->nlock = (uint8_t)k;
}
int ch_lock_any(unsigned t, unsigned step)
{
    if (t >= CH_TRACKS) return 0;
    const ch_seq_state *q = &ctl_seq[t];
    for (unsigned i = 0; i < q->nlock; ++i) if (q->lock[i].step == step) return 1;
    return 0;
}
/* audio side: the step's locks into force (a locked parameter snaps to its value; one let go snaps back to the knob) */
static void lock_apply(unsigned ti, int step)
{
    ch_trk *t = &ch_tr[ti];
    const ch_seq_state *q = &ctl_seq[ti];
    uint16_t was[CH_SLOTS];
    for (unsigned s = 0; s < CH_SLOTS; ++s) { was[s] = t->lk_mask[s]; t->lk_mask[s] = 0; if (was[s]) t->pwake = CH_PWAKE; }
    if (q->nlock) t->pwake = CH_PWAKE;
    if (step >= 0)
        for (unsigned i = 0; i < q->nlock; ++i) {
            const ch_lock *L = &q->lock[i];
            if (L->step != step || L->slot >= CH_SLOTS || L->param >= CH_PARAMS) continue;
            if (L->param >= ch_device_info(t->device[L->slot])->params) continue;
            t->lk_mask[L->slot] |= (uint16_t)(1u << L->param); t->lk_v[L->slot][L->param] = (float)L->v * (1.0f / 256);
        }
    for (unsigned s = 0; s < CH_SLOTS; ++s) {
        uint16_t changed = (uint16_t)(was[s] ^ t->lk_mask[s]) | t->lk_mask[s];
        if (!changed) continue;
        unsigned row = ch_row(s, t->device[s]);
        const ch_device_desc *dd = ch_device_info(t->device[s]);
        for (unsigned p = 0; p < CH_PARAMS; ++p)
            if (changed >> p & 1u) {
                /* an effect's continuous setting (a MIX, a DRIVE, a level) glides to the lock with the knobs' smoothing (~9 ms), no
                 * step in the sound; the engine's settings, and stepped ones, land at once: the step's note reads them as it starts */
                if (s != CH_MATERIAL && p < dd->params && dd->param[p].kind != CH_SELECT) continue;
                t->smooth[row][p] = (t->lk_mask[s] >> p & 1u) ? t->lk_v[s][p] : t->base[row][p];
                t->v[s][p].l = t->v[s][p].r = t->smooth[row][p];   /* at once: the step's note reads it before the next evaluation */
            }
    }
}

/* ---------------- audio side: commands ---------------- */
static void apply(const ch_cmd *c)
{
    ch_trk *t = c->t == CH_MASTER ? &ch_mst : &ch_tr[c->t];
    t->pwake = CH_PWAKE;                                  /* (any command: its values glide again) */
    for (unsigned q = 0; q < CH_SLOTS; ++q) t->fx_quiet[q] = 0;   /* (and its effects wake: a new one may make sound of its own) */
    switch (c->op) {
    case OP_SAMPLE: ch_smp[c->a] = (const ch_sample *)c->p; break;
    case OP_DEVICE:
        t->device[c->a] = c->b;
        if (c->a != CH_MATERIAL) {             /* a new effect in an FX slot: its own state, settings, no mappings */
            unsigned k = c->a - CH_FX1, r = ch_row(c->a, c->b);
            const ch_device_desc *d = ch_device_info(c->b);
            ch_fx_release(t, c->t, k);
            memset(&t->fx[k], 0, sizeof t->fx[k]);
            for (unsigned p = 0; p < CH_PARAMS; ++p) {
                t->base[r][p] = t->smooth[r][p] = p < d->params ? d->param[p].def : 0;
                t->mode[r][p] = 0;
            }
            for (unsigned i = 0; i < CH_MAPS; ++i) if (t->map[i].slot == c->a) t->map[i].used = 0, t->map[i].depth = 0;
        }
        break;
    case OP_BYPASS: t->bypass[c->a] = c->b; break;
    case OP_PARAM: {                   /* a: the row, b: the parameter, c: the device; d: one of the SYNTH's extras (raw) */
        if (c->d) { t->base[c->a][c->b] = c->f; break; }
        const ch_param_desc *pd = &ch_device_info(c->c)->param[c->b];
        t->base[c->a][c->b] = c->f;
        if (pd->kind == CH_SELECT) t->smooth[c->a][c->b] = c->f;
        break;
    }
    case OP_MODE: t->mode[c->a][c->b] = c->c; break;
    case OP_ACTION: {
        unsigned dev = t->device[c->a];
        if (dev == CH_DEV_POLY) ch_poly_action(t, c->t, c->b, c->c);
        else if ((dev == CH_DEV_DRUM || dev == CH_DEV_KIT || dev == CH_DEV_SYNTH) && c->c) { track_note(c->t, 60, 110); track_note(c->t, 60, 0); }
        break;
    }
    case OP_MODTYPE: ch_mods_init(&t->mod[c->a], c->b, t->mod[c->a].rng.s ^ 0x5bd1e995u); break;
    case OP_MODPARAM: t->mod[c->a].p[c->b] = c->f; break;
    case OP_MODMODE: t->mod[c->a].m[c->b] = c->c; break;
    case OP_MODSHAPE: t->mod[c->a].shape = c->b; break;
    case OP_MAP: {
        ch_map *m = &t->map[c->a];
        m->used = c->f != 0; m->mod = c->b; m->slot = c->c; m->param = c->d; m->depth = c->f; m->bipolar = c->g != 0;
        break;
    }
    case OP_NOTE: if (c->b && c->t < CH_TRACKS) { ch_tr[c->t].ss_new = 0; ch_tr[c->t].ss_set = 1; } track_note(c->t, c->a, c->b); break;   /* (a played note: the send knobs alone) */
    case OP_SEQ:
        if (c->a == 0) { t->seq.len = c->b; if (t->seq.pos >= c->b) t->seq.pos = -1; }
        else if (c->a == 1) t->seq.dir = c->b;
        else t->seq.rate = c->b;
        break;
    case OP_SWING: swing = c->f; break;
    case OP_MACRO: macro_v[c->b] = c->f; break;
    case OP_STUTTER: stutter_set(c->a, c->b, c->c); break;
    case OP_INIT:                      /* notes off, buffers given back (they come back cleared), then as new */
        seq_stop(c->t);
        for (unsigned k = 0; k < CH_FX; ++k) ch_fx_release(t, c->t, k);
        track_runtime(c->t);
        break;
    case OP_SEND: t->send[c->a] = c->f; break;
    case OP_MIX:
        if (c->a == CH_MIX_LEVEL) t->level = c->f; else if (c->a == CH_MIX_FILTER) t->filter = c->f;
        else if (c->a == CH_MIX_PAN) t->pan = c->f; else if (c->a == CH_MIX_MUTE) t->mute = c->f != 0;
        else if (c->a == CH_MIX_RUN) t->run = c->f != 0;
        else if (c->a == CH_MIX_SSDEST) t->ss_dest = (uint8_t)(c->f < 0.5f ? 0 : c->f > (float)CH_SS_LAST ? CH_SS_LAST : (int)(c->f + 0.5f));
        else if (c->a == CH_MIX_TOFX) t->tofx = ch_clamp(c->f, 0, 1);
        break;
    case OP_MASTER: master_comp = c->f; master_main = c->g; break;
    case OP_TRANSPORT:
        if (c->a && !ch_t.playing) { ch_t.beat = 0; ch_t.beat_c = 0; ch_t.started = 1; }
        if (!c->a && ch_t.playing) { for (unsigned i = 0; i < CH_TRACKS; ++i) { seq_stop(i); memset(ch_tr[i].drum.kit_oct, 0, sizeof ch_tr[i].drum.kit_oct); } stut_on = 0; stut_stop = -1; }   /* (stopped: a KIT's parts at their own pitch again) */
        ch_t.playing = c->a; count_on = 0;
        break;
    case OP_COUNT: if (!ch_t.playing && c->a) { count_on = 1; count_left = c->a; count_n = 0; count_wait = 0; } break;
    case OP_TEMPO: ch_t.bpm_target = c->f; break;
    case OP_METRO: metro_on = c->a; break;
    }
}

/* ---------------- parameters ---------------- */
static CH_AUDIO_CODE void eval_params(ch_trk *t)
{
    const float k = 0.08f;   /* ~9 ms at the sub-block rate */
    /* The glide of every value of every slot, each sub-block, was a fifth of an idle render. Once nothing has been set
     * for CH_PWAKE sub-blocks (the glides long at rest: the same numbers come out) only the values a map moves are made
     * again; a full pass every 64th call all the same, should a writer ever be missed */
    int awake = t->pwake != 0 || (++t->pbeat & 63u) == 0;
    if (t->pwake) --t->pwake;
    if (!awake) {
        for (unsigned i = 0; i < CH_MAPS; ++i) {
            const ch_map *m = &t->map[i];
            if (!m->used || m->slot >= CH_SLOTS || m->param >= CH_PARAMS) continue;
            float sm = t->smooth[ch_row(m->slot, t->device[m->slot])][m->param];
            t->v[m->slot][m->param].l = t->v[m->slot][m->param].r = sm;
        }
    }
    else for (unsigned s = 0; s < CH_SLOTS; ++s) {
        unsigned dev = t->device[s], row = ch_row(s, dev);
        const ch_device_desc *d = ch_device_info(dev);
        unsigned np = d->params < CH_PARAMS ? d->params : CH_PARAMS;   /* (the extras are read raw from their row) */
        for (unsigned p = 0; p < np; ++p) {
            float *sm = &t->smooth[row][p];
            float target = (t->lk_mask[s] >> p & 1u) ? t->lk_v[s][p] : t->base[row][p];   /* a parameter lock stands in for the knob */
            if (d->param[p].kind != CH_SELECT) *sm = ch_ftz(*sm + (target - *sm) * k);
            else if (t->lk_mask[s] >> p & 1u) *sm = target;
            t->v[s][p].l = t->v[s][p].r = *sm;
        }
    }
    for (unsigned q = 0; q < CH_MACROS; ++q) t->mac_s[q] = ch_ftz(t->mac_s[q] + (macro_v[q] - t->mac_s[q]) * k);
    uint16_t moved[CH_SLOTS];                          /* the parameters a map or a lock moved: only they can leave their range */
    for (unsigned s = 0; s < CH_SLOTS; ++s) moved[s] = t->lk_mask[s];
    t->mixmod[0] = t->mixmod[1] = t->mixmod[2] = 0;
    for (unsigned i = 0; i < CH_MAPS; ++i) {
        const ch_map *m = &t->map[i];
        if (!m->used) continue;
        if (m->slot == CH_MIXSLOT) {                     /* onto the mixer: LEVEL (0..1), PAN and FILTER (-1..1) */
            if (m->param < 3 && m->mod < CH_MODS + CH_MACROS) {
                float x = m->mod >= CH_MODS ? t->mac_s[m->mod - CH_MODS] : m->bipolar ? t->mod[m->mod].out_l : t->mod[m->mod].uni_l;
                t->mixmod[m->param] += m->depth * x * (m->param ? 2.0f : 1.0f);
            }
            continue;
        }
        if (m->slot < CH_SLOTS && m->param < CH_PARAMS) moved[m->slot] |= (uint16_t)(1u << m->param);
        if (m->mod >= CH_MODS) {                         /* a macro: its value, unipolar */
            if (m->mod >= CH_MODS + CH_MACROS || m->param >= ch_device_info(t->device[m->slot])->params || m->param >= CH_PARAMS) continue;
            const ch_param_desc *pq = &ch_device_info(t->device[m->slot])->param[m->param];
            float sp = pq->kind == CH_BI ? 2.0f : pq->kind == CH_SELECT ? (float)(pq->steps - 1) : 1.0f, mv = m->depth * t->mac_s[m->mod - CH_MODS] * sp;
            t->v[m->slot][m->param].l += mv; t->v[m->slot][m->param].r += mv;
            continue;
        }
        const ch_mod *md = &t->mod[m->mod];
        const ch_param_desc *pd = &ch_device_info(t->device[m->slot])->param[m->param];
        if (m->param >= ch_device_info(t->device[m->slot])->params || m->param >= CH_PARAMS) continue;
        float span = pd->kind == CH_BI ? 2.0f : pd->kind == CH_SELECT ? (float)(pd->steps - 1) : 1.0f;
        float dl = m->bipolar ? md->out_l : md->uni_l, dr = m->bipolar ? md->out_r : md->uni_r;
        t->v[m->slot][m->param].l += m->depth * dl * span;
        t->v[m->slot][m->param].r += m->depth * dr * span;
    }
    for (unsigned s = 0; s < CH_SLOTS; ++s) {
        if (!moved[s]) continue;
        const ch_device_desc *d = ch_device_info(t->device[s]);
        unsigned np = d->params < CH_PARAMS ? d->params : CH_PARAMS;
        for (unsigned p = 0; p < np; ++p) {
            if (!(moved[s] >> p & 1u)) continue;
            const ch_param_desc *pd = &d->param[p];
            float lo = pd->kind == CH_BI ? -1.0f : 0.0f, hi = pd->kind == CH_SELECT ? (float)(pd->steps - 1) : 1.0f;
            t->v[s][p].l = ch_clamp(t->v[s][p].l, lo, hi);
            t->v[s][p].r = ch_clamp(t->v[s][p].r, lo, hi);
        }
    }
}

/* ---------------- mixer pieces ---------------- */
/* one knob: CCW low-pass, CW high-pass, centre dead zone. Its coefficient once a block (dj_g): 0 none, else g with
 * its sign the side (- low-pass, + high-pass) */
static float dj_g(float v)
{
    if (v > -0.02f && v < 0.02f) return 0;
    float a = (ch_abs(v) - 0.02f) / 0.98f;
    return v < 0 ? -ch_svf_g(ch_exp_map(1 - a, 60, 20000)) : ch_svf_g(ch_exp_map(a, 20, 9000));
}
CH_SI float dj_filter(ch_svf_t *f, float x, float g)
{
    if (g == 0) return x;
    if (g < 0) return ch_svf_tick(f, x, -g, 1.1f).lp;
    return ch_svf_tick(f, x, g, 1.1f).hp;
}

/* the load page's clock and sums (ch_profile_clock): ticks per stage in this render call, their peaks this second */
static ch_clock_fn pclk;
static uint32_t ptpu = 1, pacc[CH_TRACKS][CH_SLOTS], ppk[CH_TRACKS][CH_SLOTS], prpk, mpk, pcount, pframes;
void ch_profile_clock(ch_clock_fn fn, unsigned ticks_per_us) { ptpu = ticks_per_us ? ticks_per_us : 1; pclk = fn; }
/* the guard after each stage: a broken value (NaN, infinity, or past 16384: none a sound makes) mutes the block
 * and resets that stage, so it can never reach the master as noise MASTER cannot turn down (NaN times 0 is NaN);
 * each counted for the LOAD page, to find the stage */
static uint16_t bad_n[CH_TRACKS + 1][CH_SLOTS];
static uint8_t bad_at[3];
static void stage_reset(ch_trk *t, unsigned ti, unsigned s)
{
    if (s == CH_MATERIAL) {
        for (unsigned i = 0; i < CH_DVOICES; ++i) { t->drum.v[i].on = 0; t->drum.v[i].last = t->drum.v[i].dc = 0; t->drum.v[i].dcp = 0; }
        for (unsigned i = 0; i < CH_SVOICES; ++i) { t->synth.v[i].on = 0; t->synth.v[i].last = t->synth.v[i].dc = 0; t->synth.v[i].dcp = 0; }   /* (their last sound too: a bad value must not come back as the hand-over) */
        t->grain.on = 0; for (unsigned i = 0; i < CH_GRAINS1; ++i) t->grain.g[i].left = 0;
    } else ch_fx_reset(t, s - CH_FX1);
    (void)ti;
}
static CH_AUDIO_CODE int stage_guard(ch_trk *t, unsigned ti, unsigned s, float *L, float *R, unsigned n, float *peak)
{
    uint32_t worst = 0;
    for (unsigned i = 0; i < n; ++i) {
        uint32_t a = ch_to_bits(L[i]) & 0x7FFFFFFFu, b = ch_to_bits(R[i]) & 0x7FFFFFFFu;
        if (a > worst) worst = a;
        if (b > worst) worst = b;
    }
    *peak = ch_from_bits(worst);                         /* (a float's bits without its sign order as its size: the largest is the peak) */
    if (worst <= 0x46800000u) return 0;                  /* |x| <= 16384 */
    *peak = 0;
    for (unsigned i = 0; i < n; ++i) L[i] = R[i] = 0;
    stage_reset(t, ti, s);
    if (bad_n[ti][s] < 65535u) ++bad_n[ti][s];
    if (s < CH_SLOTS) ++ch_guard_trips[s];
    bad_at[0] = (uint8_t)ti; bad_at[1] = (uint8_t)s;
    bad_at[2] = worst > 0x7F800000u ? 1 : worst == 0x7F800000u ? 2 : 3;
    return 1;
}

/* ---- the declick dips. A change that would cut a sounding stage at once (a new engine or effect, one bypassed, a stepped
 * setting such as a TYPE or MODEL, INIT) waits while that stage fades out over DIP_S on an S curve: an engine to
 * silence, an effect to its dry sound (the track never goes quiet for an effect), INIT the whole track. Then it is made,
 * and the stage fades back in. The commands behind it wait in the ring (a few ms); changes that come together share
 * one dip. A stage not sounding changes at once and fades in from nothing ---- */
#define DIP_S 0.005f
static CH_AUDIO_CODE void dip_run(float *dk, int8_t *dd, float *L, float *R, unsigned n, const float *dl, const float *dr)
{
    const float st = 1.0f / (DIP_S * (float)CH_RATE);
    float k = *dk; int d = *dd;
    for (unsigned i = 0; i < n; ++i) {
        float g = 1 - ch_fade_gain(k);
        if (dl) { L[i] = dl[i] + (L[i] - dl[i]) * g; R[i] = dr[i] + (R[i] - dr[i]) * g; }
        else { L[i] *= g; R[i] *= g; }
        k += (float)d * st;
        if (k >= 1) k = 1;                                   /* out: held there until the change is made */
        if (k <= 0) { k = 0; d = 0; }                        /* back: done */
    }
    *dk = k; *dd = (int8_t)d;
}
static CH_AUDIO_CODE CH_NOINLINE void track_sub(unsigned ti, const float *inL, const float *inR, float *mixL, float *mixR,
                                                 float (*next)[2][CH_SUB], unsigned n, uint8_t *next_live, float (*side)[2][CH_SUB], uint8_t *side_on)
{
    ch_trk *t = &ch_tr[ti];
    float L[CH_SUB], R[CH_SUB];
    float (*bus)[2][CH_SUB] = send_bus[send_phase];
    float pk = 0;
    if (!inL && !inR && !(bus_live[send_phase] >> ti & 1u)) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); }   /* nothing sent here, no input: silence */
    else for (unsigned i = 0; i < n; ++i) {
        L[i] = (inL ? inL[i] : 0) + bus[ti][0][i];
        R[i] = (inR ? inR[i] : 0) + bus[ti][1][i];
        pk = ch_max(pk, ch_max(ch_abs(L[i]), ch_abs(R[i])));
    }
    t->lvl_in = pk;
    CH_P(1);
    ch_mods_tick(t, ti, n);
    CH_P(2);
    eval_params(t);
    CH_P(3);
    uint32_t c0 = pclk ? pclk() : 0;
    /* MATERIAL */
    unsigned mat = t->device[CH_MATERIAL];
#ifdef CHISEL_HOST
    if (HOST_FX) mat = CH_DEV_BYPASS;
#endif
    if (!t->bypass[CH_MATERIAL]) {
        if (mat == CH_DEV_POLY) ch_poly_process(t, ti, L, R, n);
        else if (mat == CH_DEV_GRAIN) ch_grain_process(t, ti, L, R, n);
        else if (mat == CH_DEV_DRUM || mat == CH_DEV_KIT) ch_drum_process(t, ti, L, R, n);   /* (KIT: the drum voices, a part each) */
        else if (mat == CH_DEV_SYNTH) ch_synth_process(t, ti, L, R, n);
    }
    CH_P(4);
    stage_guard(t, ti, CH_MATERIAL, L, R, n, &pk);
    t->lvl_mat = pk;
    float outpk = pk;                                    /* the last stage's peak: 0, the track is silent and the sends and the mixer have nothing to do */
    if (t->dk[CH_MATERIAL] > 0 || t->dd[CH_MATERIAL]) dip_run(&t->dk[CH_MATERIAL], &t->dd[CH_MATERIAL], L, R, n, 0, 0);
    if (pclk) { uint32_t c = pclk(); pacc[ti][0] += c - c0; c0 = c; }
    CH_P(5);
    /* the effects, in the track's order; one not in it, or bypassed, gives its buffers back */
    int same = 0;                                        /* left and right known to be the same sound */
    for (unsigned s = CH_FX1; s <= CH_FX4; ++s) {         /* the FX slots, in order */
        if (t->device[s] == CH_DEV_BYPASS || t->bypass[s]) continue;
        {   /* FILTER and DRIVE treat the two sides alike: where the sides are the same sound (the drum and synth engines
             * give one voice path to both; looked at here, 32 compares) they work one and copy it */
            int cap = ch_fx_mono_ok(t, s);                /* (DISPERSE, DJ FILTER and COMB too, since: DISPERSE had the one-side path and was never given it) */
            if (cap && !same) { same = 1; for (unsigned i = 0; i < n; ++i) if (ch_to_bits(L[i]) != ch_to_bits(R[i])) { same = 0; break; } }
            if (!cap) same = 0;
            if (cap && !same) t->fx[s - CH_FX1].wide = 1;
            t->fx[s - CH_FX1].mono = (uint8_t)(cap && same && !t->fx[s - CH_FX1].wide && !(t->dk[s] > 0 || t->dd[s]));
        }
        if (t->dk[s] > 0 || t->dd[s]) {                  /* dipping: the effect against its dry sound */
            float dl[CH_SUB], dr[CH_SUB];
            for (unsigned i = 0; i < n; ++i) { dl[i] = L[i]; dr[i] = R[i]; }
            ch_fx_process(t, s, L, R, n); stage_guard(t, ti, s, L, R, n, &outpk);
            t->fx_quiet[s] = 0;
            dip_run(&t->dk[s], &t->dd[s], L, R, n, dl, dr);
        } else if (outpk == 0 && t->fx_quiet[s] >= CH_FX_SLEEP) { }   /* asleep: silence in, and nothing left in it to come out */
        else {
            float inpk = outpk;
            ch_fx_process(t, s, L, R, n); stage_guard(t, ti, s, L, R, n, &outpk);
            /* 2 s of silence in and under -100 dB out (longer than any line it holds: no echo is still on its way) and
             * the effect sleeps. One that makes sound of its own (TAPE's hiss, a COMB ringing on) never falls quiet */
            if (inpk == 0 && outpk < 1e-5f) { if (t->fx_quiet[s] < CH_FX_SLEEP) ++t->fx_quiet[s]; } else t->fx_quiet[s] = 0;
        }
        if (pclk) { uint32_t c = pclk(); pacc[ti][s] += c - c0; c0 = c; }
    }
    CH_P(6);
    if (t->dk[CH_SLOTS] > 0 || t->dd[CH_SLOTS]) dip_run(&t->dk[CH_SLOTS], &t->dd[CH_SLOTS], L, R, n, 0, 0);
    /* sends into next sub-block's inputs */
    if (t->ss_set) { t->ssend = t->ss_new; t->ss_set = 0; }
    const float ssg0 = t->ssg, ssg10 = t->ssg1;                           /* (the step's amount as this sub-block starts: FX SEND glides it again below) */
    /* the step's SEND: an amount on top of the track's send knob, for the hit that carries it (0, as every step starts:
     * the knob alone). It goes to the send the track has set for it (ss_dest), though that knob be at 0; or, with ALL,
     * to each send that is turned up. Quick to come and go (95 % in 1.4 ms: the next hit's attack is not thrown too) */
    const int stepped = t->ssend != 0 || t->ssg != 0 || t->ssg1 != 0;
    float ssg_end = t->ssg, ssg1_end = t->ssg1; int ran = 0;
    for (unsigned d = 0; d < CH_SENDS; ++d) {
        float s = t->send_s[d], s1 = t->send_s1[d], st = t->send[d];
        int tgt = stepped && (t->ss_dest ? t->ss_dest == d + 1u : st > 0);   /* (CH_SS_FX and above: no track send; FX SEND has it, after the mixer) */
        if (!tgt && s <= 0 && s1 <= 0 && st <= 0) { t->send_s[d] = t->send_s1[d] = 0; continue; }
        if (outpk == 0) { t->send_s[d] = t->send_s1[d] = st; continue; }   /* silence: nothing to send, the amount at its place */
        *next_live |= (uint8_t)(1u << d);
        if (tgt) {
            float g = t->ssg, g1 = t->ssg1; const float gt = t->ssend;
            for (unsigned i = 0; i < n; ++i) {
                s1 += (st - s1) * 0.012f; s += (s1 - s) * 0.012f;
                g1 += (gt - g1) * 0.05f; g += (g1 - g) * 0.05f;   /* a microfade in and out: 2 ms, S-shaped */
                float a = ch_min(s + g, 1);
                next[d][0][i] += ch_softclip(L[i] * a);
                next[d][1][i] += ch_softclip(R[i] * a);
            }
            ssg_end = g; ssg1_end = g1; ran = 1;
        } else
        for (unsigned i = 0; i < n; ++i) {             /* the amount gliding (2 ms): a send turned or switched never steps */
            s1 += (st - s1) * 0.012f; s += (s1 - s) * 0.012f;   /* two one-poles: no corner where it starts */
            /* soft limit so a self-send can build but never run away */
            next[d][0][i] += ch_softclip(L[i] * s);
            next[d][1][i] += ch_softclip(R[i] * s);
        }
        t->send_s[d] = s < 1e-5f && st <= 0 ? 0 : s; t->send_s1[d] = s1 < 1e-5f && st <= 0 ? 0 : s1;
    }
    if (stepped) {                                       /* (no send ran it: the amount goes to its place all the same) */
        if (!ran) { float g = t->ssg, g1 = t->ssg1; for (unsigned i = 0; i < n; ++i) { g1 += (t->ssend - g1) * 0.05f; g += (g1 - g) * 0.05f; } ssg_end = g; ssg1_end = g1; }
        if (ch_abs(ssg_end - t->ssend) < 1e-4f && ch_abs(ssg1_end - t->ssend) < 1e-4f) ssg_end = ssg1_end = t->ssend;
        t->ssg = ssg_end; t->ssg1 = ssg1_end;
    }
    CH_P(7);
    /* mixer: DJ filter, level, pan, mute */
    float lv = t->mute ? 0 : ch_level_gain(ch_clamp(t->level + t->mixmod[0], 0, 1));
    float pan = ch_clamp(t->pan + t->mixmod[1], -1, 1);
    float tl = lv * (pan > 0 ? 1 - pan : 1), tr = lv * (pan < 0 ? 1 + pan : 1);
    const float gk = 0.012f;                             /* the level, pan and mute glide: two one-poles of 1.9 ms, critically damped (95 % in ~9 ms, no corner) */
    pk = 0; float pl = 0, pr = 0;
    t->filter_s += (ch_clamp(t->filter + t->mixmod[2], -1, 1) - t->filter_s) * 0.08f; t->filter_s = ch_ftz(t->filter_s);   /* the mix filter's knob gliding (~9 ms) */
    float djg = dj_g(t->filter_s);                        /* its coefficient, once a block */
    if (outpk == 0 && djg == 0) {                        /* silent, no filter ringing on: nothing to mix, the gains at their places */
        t->g1_l = t->g_l = tl; t->g1_r = t->g_r = tr;
        memset(ch_tmix[ti][0], 0, sizeof ch_tmix[ti][0]); memset(ch_tmix[ti][1], 0, sizeof ch_tmix[ti][1]);
    } else if (djg == 0 && t->g1_l == tl && t->g_l == tl && t->g1_r == tr && t->g_r == tr) {   /* the gains at rest, the filter out: two multiplies a frame */
        uint32_t wl = 0, wr = 0;
        for (unsigned i = 0; i < n; ++i) {
            float l = L[i] * tl, r = R[i] * tr;
            mixL[i] += l; mixR[i] += r;
            ch_tmix[ti][0][i] = l; ch_tmix[ti][1][i] = r;
            uint32_t a = ch_to_bits(l) & 0x7FFFFFFFu, b = ch_to_bits(r) & 0x7FFFFFFFu;
            if (a > wl) wl = a;
            if (b > wr) wr = b;
        }
        pl = ch_from_bits(wl); pr = ch_from_bits(wr);
    } else
    for (unsigned i = 0; i < n; ++i) {
        float l = dj_filter(&t->dj[0], L[i], djg), r = dj_filter(&t->dj[1], R[i], djg);
        t->g1_l += (tl - t->g1_l) * gk; t->g1_r += (tr - t->g1_r) * gk;
        t->g_l += (t->g1_l - t->g_l) * gk; t->g_r += (t->g1_r - t->g_r) * gk;
        l *= t->g_l; r *= t->g_r;
        mixL[i] += l; mixR[i] += r;
        ch_tmix[ti][0][i] = l; ch_tmix[ti][1][i] = r;
        pl = ch_max(pl, ch_abs(l)); pr = ch_max(pr, ch_abs(r));
    }
    t->g_l = ch_ftz(t->g_l); t->g_r = ch_ftz(t->g_r); t->g1_l = ch_ftz(t->g1_l); t->g1_r = ch_ftz(t->g1_r);
    /* within a millionth of their places, the gains are put there (the glide never quite arrives; at rest the loop above is the short one) */
    if (ch_abs(t->g_l - tl) < 1e-6f && ch_abs(t->g1_l - tl) < 1e-6f && ch_abs(t->g_r - tr) < 1e-6f && ch_abs(t->g1_r - tr) < 1e-6f) { t->g1_l = t->g_l = tl; t->g1_r = t->g_r = tr; }
    t->lvl_out = ch_max(pl, pr);
    t->peak_l = ch_max(pl, t->peak_l * 0.97f); t->peak_r = ch_max(pr, t->peak_r * 0.97f);
    /* FX SEND: the track, as mixed, sent into the master chain's returns (its delays and reverbs take a send whole,
     * whatever their MIX; the track is in the mix all the same). The knob is the whole track's, a step's SEND adds to it
     * for its hit. side[0..3]: the sends that join at the master's 1st..4th effect (the track's SEND TO), each cleared
     * by the first to use it. Nothing sent (every track as it starts): none of this runs */
    {
        int fxstep = t->ss_dest >= CH_SS_FX && (t->ssend != 0 || ssg0 != 0 || ssg10 != 0);
        if (t->tofx != 0 || t->fx_s != 0 || t->fx_s1 != 0 || fxstep) {
            float s = t->fx_s, s1 = t->fx_s1, g = fxstep ? ssg0 : 0, g1 = fxstep ? ssg10 : 0; const float st = t->tofx, gt = fxstep ? t->ssend : 0;
            if (pl == 0 && pr == 0) { s = s1 = st; }       /* silent: nothing to send, the amount at its place */
            else {
                unsigned ent = t->ss_dest > CH_SS_FX && t->ss_dest <= CH_SS_LAST ? (unsigned)t->ss_dest - CH_SS_FX : 0;
                if (!(*side_on >> ent & 1u)) { memset(side[ent], 0, sizeof side[0]); *side_on |= (uint8_t)(1u << ent); }
                float *eL = side[ent][0], *eR = side[ent][1];
                const float *ol = ch_tmix[ti][0], *orr = ch_tmix[ti][1];
                for (unsigned i = 0; i < n; ++i) {
                    s1 += (st - s1) * 0.012f; s += (s1 - s) * 0.012f;
                    g1 += (gt - g1) * 0.05f; g += (g1 - g) * 0.05f;
                    float a = ch_min(s + g, 1);
                    eL[i] += ol[i] * a; eR[i] += orr[i] * a;
                }
            }
            if (ch_abs(s - st) < 1e-5f && ch_abs(s1 - st) < 1e-5f) s = s1 = st;
            t->fx_s = s; t->fx_s1 = s1;
        }
    }
    CH_P(8);
}

/* the shared memory (the effects' lines, tape), taken and given back on the first core only, before the tracks
 * render (two cores taking one at once could both have it) */
static void track_buffers(unsigned ti)
{
    ch_trk *t = &ch_tr[ti];
    uint8_t nm = 0;
    for (unsigned k = 0; k < CH_FX; ++k) { ch_fx_prepare(t, ti, k); if (t->fx[k].nomem) nm |= (uint8_t)(1u << k); }
    snap.fx_nomem[ti] = nm;
}

static void master_buffers(void)
{
    uint8_t nm = 0;
    for (unsigned k = 0; k < CH_FX; ++k) { ch_fx_prepare(&ch_mst, CH_MASTER, k); if (ch_mst.fx[k].nomem) nm |= (uint8_t)(1u << k); }
    snap.mfx_nomem = nm;
}
/* the master chain: the four FX slots on the tracks' mix (after the join, on the first core) */
void ch_return_set(float level) { ret_level = ch_clamp(level, 0, 1); }
float ch_return(void) { return ret_level; }
static CH_AUDIO_CODE void master_fx(float *L, float *R, unsigned n, float (*side)[2][CH_SUB], unsigned side_on, unsigned from)
{
    ch_trk *t = &ch_mst;
    eval_params(t);
    float eL[CH_SUB], eR[CH_SUB]; int have = 0;          /* the tracks' sends that have joined so far: into each return from there on */
    const float gt = ch_level_gain(ret_level); float g_end = gt;   /* (nothing sent: the gain at its place) */
    for (unsigned s = CH_FX1; s <= CH_FX4; ++s) {
        unsigned k = s - CH_FX1;
        if (side_on >> k & 1u) {                         /* (at the RETURN's level, gliding as a track's does) */
            float g = ret_g;
            if (!have) { for (unsigned i = 0; i < n; ++i) { g += (gt - g) * 0.012f; eL[i] = side[k][0][i] * g; eR[i] = side[k][1][i] * g; } have = 1; }
            else for (unsigned i = 0; i < n; ++i) { g += (gt - g) * 0.012f; eL[i] += side[k][0][i] * g; eR[i] += side[k][1][i] * g; }
            g_end = g;
        }
        if (t->device[s] == CH_DEV_BYPASS || t->bypass[s]) continue;
        /* a delay or a reverb is a return: MIX is how much of the mix goes into it, the sends go in whole, and with MIX
         * at 0 it runs on for its sends and its tail (asleep once 2 s have passed with neither) */
        float mx = 1; int ret = ch_fx_return(t, s, &mx);
        /* (a slot a send can reach becomes a return, once and for good: its lines start clean, since what they hold
         * was at another level. A slot no send can reach stays what it was, to the sample) */
        if (ret && k >= from && !(ch_fx_retmask >> k & 1u)) { ch_fx_retmask |= (uint8_t)(1u << k); if (t->fx[k].mem) ch_fx_reset(t, k); }
        if (!(ch_fx_retmask >> k & 1u)) ret = 0;
        int watch = ret && mx < 0.001f, dip = t->dk[s] > 0 || t->dd[s];
        if (watch && !have && t->fx_quiet[s] >= CH_FX_SLEEP) continue;
        float dl[CH_SUB], dr[CH_SUB];
        if (dip || watch) for (unsigned i = 0; i < n; ++i) { dl[i] = L[i]; dr[i] = R[i]; }
        if (ret) { ch_fx_el_g = have ? eL : 0; ch_fx_er_g = eR; ch_fx_pre_g = 1; }
        float pkm; ch_fx_process(t, s, L, R, n); stage_guard(t, CH_MASTER, s, L, R, n, &pkm);
        ch_fx_pre_g = 0; ch_fx_el_g = 0;
        if (watch) {                                     /* (MIX at 0: all it added is its own sound) */
            float w = 0;
            for (unsigned i = 0; i < n; ++i) { w = ch_max(w, ch_abs(L[i] - dl[i])); w = ch_max(w, ch_abs(R[i] - dr[i])); }
            if (!have && w < 1e-5f) { if (t->fx_quiet[s] < CH_FX_SLEEP) ++t->fx_quiet[s]; } else t->fx_quiet[s] = 0;
        } else if (ret) t->fx_quiet[s] = 0;
        if (dip) dip_run(&t->dk[s], &t->dd[s], L, R, n, dl, dr);
    }
    ret_g = ch_abs(g_end - gt) < 1e-5f ? gt : g_end;
    float pl = 0, pr = 0;
    if (have) for (unsigned i = 0; i < n; ++i) { pl = ch_max(pl, ch_abs(eL[i])); pr = ch_max(pr, ch_abs(eR[i])); }
    ret_pk[0] = ch_max(pl, ret_pk[0] * 0.97f); ret_pk[1] = ch_max(pr, ret_pk[1] * 0.97f);
    if (ret_pk[0] < 1e-6f) ret_pk[0] = 0;
    if (ret_pk[1] < 1e-6f) ret_pk[1] = 0;
}

/* the second core: its tracks, into its own mix and send sums, added in after */
static ch_job_run_fn job_run;
static ch_job_wait_fn job_wait;
void ch_worker(ch_job_run_fn run, ch_job_wait_fn wait) { job_run = run; job_wait = wait; }
static struct {
    unsigned mask, n;
    uint8_t live;                                      /* the sends it wrote this sub-block (a bit a destination) */
    uint8_t side_on; float side[4][2][CH_SUB];         /* its tracks' shares that join the master chain further on, or after it (side_on: a bit each) */
    const float *inL, *inR;
    float mixL[CH_SUB], mixR[CH_SUB];
    float bus[CH_TRACKS][2][CH_SUB];
} job CH_POOL;
static uint32_t tcost[CH_TRACKS];                     /* each track's ticks in the last render call */
static uint32_t mcost;                                /* and the master chain's, which the first core always has */
static CH_AUDIO_CODE void job_tracks(uint32_t arg)
{
    (void)arg;
    memset(job.mixL, 0, sizeof job.mixL); memset(job.mixR, 0, sizeof job.mixR);
    if (job.live) { memset(job.bus, 0, sizeof job.bus); job.live = 0; }   /* (only when the last one wrote a send) */
    job.side_on = 0;
    for (unsigned ti = 0; ti < CH_TRACKS; ++ti) if (job.mask >> ti & 1u) track_sub(ti, job.inL, job.inR, job.mixL, job.mixR, job.bus, job.n, &job.live, job.side, &job.side_on);
}
/* the second core's tracks: the split of the last call's times whose larger half is least (the first core also has
 * the master and the sequencer); only the drum and synth engines go (the others share voices between tracks) */
static uint8_t kit_c2 = 1;   /* (on: tried on an FM-1 2026-11-11, two kits, no dropout a hit) */
/* ch_kit_core2_set: a KIT may go to the second core (the LOAD page's K3). It was kept off it when its code ran late there; that was before the voices were remade, and two kits on the first core are now what a busy render waits for */
void ch_kit_core2_set(int on) { kit_c2 = on != 0; }
int ch_kit_core2(void) { return kit_c2; }
static unsigned split(void)
{
    uint32_t tot = 0;
    unsigned ok = 0;
    for (unsigned ti = 0; ti < CH_TRACKS; ++ti) {
        unsigned d = ch_tr[ti].device[CH_MATERIAL];
        if (ch_tr[ti].bypass[CH_MATERIAL] || d == CH_DEV_DRUM || d == CH_DEV_SYNTH || d == CH_DEV_BYPASS || (d == CH_DEV_KIT && kit_c2)) ok |= 1u << ti;   /* (KIT stays on the first: on the second its eight models' code ran late on the device, a dropout a hit) */
        tot += tcost[ti];
    }
    if (!tot) return ok & 0xCu;
    /* (the first core's share is its tracks, the master chain and output stage as last timed, which it works beside
     * the second core's tracks, and a little for the sequencer) */
    const uint32_t fixed = mcost + tot / 32;
    unsigned best = 0; uint32_t bmax = tot + fixed;
    for (unsigned m = 1; m < (1u << CH_TRACKS); ++m) {
        if (m & ~ok) continue;
        uint32_t a = 0;
        for (unsigned ti = 0; ti < CH_TRACKS; ++ti) if (m >> ti & 1u) a += tcost[ti];
        uint32_t b = tot - a + fixed, mx = a > b ? a : b;
        if (mx < bmax) { bmax = mx; best = m; }
    }
    return best;
}

/* the stage a command would change at once (-1: none): a device, a bypass, a stepped setting moved on the device in use,
 * INIT (CH_SLOTS: the whole track) */
static int disrupts(const ch_cmd *c)
{
    if (c->t > CH_MASTER) return -1;
    const ch_trk *t = c->t == CH_MASTER ? &ch_mst : &ch_tr[c->t];
    switch (c->op) {
    case OP_DEVICE: return c->a < CH_SLOTS && t->device[c->a] != c->b ? (int)c->a : -1;
    case OP_BYPASS: return c->a < CH_SLOTS && (t->bypass[c->a] != 0) != (c->b != 0) ? (int)c->a : -1;
    case OP_INIT: return c->t < CH_TRACKS ? CH_SLOTS : -1;
    case OP_PARAM: {
        if (c->d) return -1;                                   /* (an engine's extras, raw: none stepped that cut) */
        const ch_device_desc *d = ch_device_info(c->c);
        if (!d || c->b >= d->params || d->param[c->b].kind != CH_SELECT) return -1;
        if (c->c == CH_DEV_TRANCE) return -1;                  /* (its steps and rate are played: no fade, the gate has its own edges) */
        if ((int)(c->f + 0.5f) == (int)(t->base[c->a][c->b] + 0.5f)) return -1;
        for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2)              /* the slot whose device this row is, if it is in use */
            if (t->device[s2] == c->c && ch_row(s2, c->c) == c->a) return (int)s2;
        return -1;
    }
    default: return -1;
    }
}
/* the stage is making sound now (an effect in use always may: a tail) */
static int stage_sounds(const ch_trk *t, unsigned st)
{
    if (st == CH_SLOTS) return t->lvl_out > 1e-5f;
    if (st == CH_MATERIAL) return !t->bypass[CH_MATERIAL] && t->lvl_mat > 1e-5f;
    return t->device[st] != CH_DEV_BYPASS && !t->bypass[st];
}
static struct { float inL[CH_SUB], inR[CH_SUB], mixL[CH_SUB], mixR[CH_SUB], side[4][2][CH_SUB]; } rb CH_POOL;   /* ch_render's buffers for the sub-block in hand */
/* the mix kept for the master: a sub-block's, and the shares that join the chain further on */
static struct { float l[CH_SUB], r[CH_SUB], side[4][2][CH_SUB]; uint8_t side_on; } pm CH_POOL;
/* The master chain and the output stage for one sub-block's mix: the chain, the compressor, MAIN, the metronome, the
 * limiter, the spectrum's ring, the frames written out. Its own function since the tracks' sub-block and the master's
 * are no longer the same one (ch_render: the master works a sub-block behind, beside the second core's tracks) */
static uint32_t mall;                                 /* its ticks, this render call (the split's) */
static CH_AUDIO_CODE CH_NOINLINE void master_out(float *mixL, float *mixR, float (*side)[2][CH_SUB], unsigned side_on, unsigned n, int32_t *out, unsigned off, uint32_t *macc)
{
    const uint32_t t_in = pclk ? pclk() : 0;
    {   /* the master chain, timed */
        CH_P(11);
        uint32_t m0 = pclk ? pclk() : 0;
        unsigned from = 255;                         /* the first master slot a send can reach: the tracks' least SEND TO, once sends are in use */
        if (ch_fx_sent | side_on) {
            from = 3;
            for (unsigned ti = 0; ti < CH_TRACKS; ++ti) { unsigned e = ch_tr[ti].ss_dest > CH_SS_FX && ch_tr[ti].ss_dest <= CH_SS_LAST ? (unsigned)ch_tr[ti].ss_dest - CH_SS_FX : 0; if (e < from) from = e; }
        }
        master_fx(mixL, mixR, n, side, side_on, from);
        if (pclk) *macc += pclk() - m0;
    }
    /* the metronome: a beat crossed in this sub-block starts a blip (the bar's first, higher) */
    if (metro_on && ch_t.playing) {
        ch_tf b0 = ch_t.beat, b1 = b0 + (ch_tf)ch_t.beats_per_frame * n;
        int k0 = (int)b0, k1 = (int)b1;
        if (k1 != k0 || (ch_t.started && b0 == 0)) {
            int bar = (k1 % 4) == 0;
            metro_env = 1; metro_ph = 0; metro_inc = (bar ? 1800.0f : 1300.0f) / (float)CH_RATE;
        }
    }
    CH_P(12);
    float metro_dec = ((ch_t.playing && metro_on) || count_on) && metro_env > 1e-4f ? 0.9985f : 0;   /* ~15 ms */
    /* master: one-knob compressor, MAIN, the limiter and its knee */
    float thr = ch_exp2(-master_comp * 6.0f), ratio = 1 + master_comp * 7;
    float makeup = 1 + master_comp * 1.8f;
    float main = ch_level_gain(master_main);
    static float mg_s = -1;                            /* MAIN x the makeup, gliding (4 ms): no step when either moves */
    float mg_t = makeup * main * 0.8f; if (mg_s < 0) mg_s = mg_t;
    /* the master's running values in locals through the loop (they are statics: each was read and written in memory
     * every sample, some twenty of them; this loop runs after both cores are done, so its time is all of it the render's) */
    float me_ = master_env, mgn_ = master_g, mgs_ = mg_s, sl_ = sub_l, sr_ = sub_r, lr_ = lim_rel, ls_ = lim_sum, lg_ = lim_g, spv_ = scope_prev;
    unsigned lh_ = lim_hold, lw_ = lim_w, stk_ = scope_tick;
    const float mc_ = master_comp; const int lon_ = lim_on;
    float pk = 0, gmin = 1;
    for (unsigned i = 0; i < n; ++i) {
        float a = ch_max(ch_abs(mixL[i]), ch_abs(mixR[i]));
        me_ += (a - me_) * (a > me_ ? 0.12f : 0.0004f);
        float g = 1;
        if (mc_ > 0 && me_ > thr) g = ch_exp2(ch_log2(thr / me_) * (1 - 1 / ratio));
        mgn_ += (g - mgn_) * 0.01f;
        mgs_ += (mg_t - mgs_) * 0.0057f;
        float l = mixL[i] * mgn_ * mgs_, r = mixR[i] * mgn_ * mgs_;
        sl_ += SUB_K * (l - sl_); sr_ += SUB_K * (r - sr_); l -= sl_; r -= sr_;   /* the subsonic high-pass */
        if (metro_dec != 0) {                          /* the metronome, before the limiter: it can never push the output past full scale */
            float c = ch_sin_turns(metro_ph) * metro_env * metro_env * 0.14f;
            metro_ph += metro_inc; if (metro_ph >= 1) metro_ph -= 1;
            metro_env *= metro_dec;
            l += c; r += c;
        }
        {   /* the limiter */
            if (!lon_) { l = ch_knee(l); r = ch_knee(r); }   /* off: the knee and the clamp alone hold full scale */
            else {
            float a = ch_max(ch_abs(l), ch_abs(r));
            float need = a > LIM_CEIL ? LIM_CEIL / a : 1;
            /* down at once and held LIM_D samples (so every gain averaged below is at or under what this sample
             * needs when it comes out), then back over ~80 ms; a division only on a sample over the ceiling */
            if (need <= lr_) { lr_ = need; lh_ = LIM_D; }
            else if (lh_) --lh_;
            else lr_ += (1 - lr_) * LIM_REL;
            unsigned w = lw_;
            ls_ += lr_ - lim_held[w]; lim_held[w] = lr_;
            lg_ = ls_ * (1.0f / LIM_D);                   /* averaged: a ramp that reaches it as the peak comes out */
            unsigned o = (w + 1) & (LIM_D - 1);                 /* the sample LIM_D - 1 back */
            float dl = lim_dl[o], dr = lim_dr[o];
            lim_dl[w] = l; lim_dr[w] = r; lw_ = o;
            l = ch_knee(dl * lg_); r = ch_knee(dr * lg_);
            }
            if (lg_ < gmin) gmin = lg_;
        }
        {   /* never past full scale: a value over it held at it (not zeroed: a dropout is worse than the clip), only NaN silenced */
            uint32_t bl = ch_to_bits(l) & 0x7FFFFFFFu, br = ch_to_bits(r) & 0x7FFFFFFFu;
            if (bl > 0x7F800000u || br > 0x7F800000u) { l = r = 0; ++snap.master_bad; }
            else { if (bl > 0x3F800000u) l = l > 0 ? 1.0f : -1.0f; if (br > 0x3F800000u) r = r > 0 ? 1.0f : -1.0f; }
        }
        pk = ch_max(pk, ch_max(ch_abs(l), ch_abs(r)));
        {                                        /* the spectrum's ring: the master as heard, or a track as mixed (ch_tmix, x2); two frames averaged into one */
            unsigned src = scope_src;
            float v = src ? (ch_tmix[src - 1][0][i] + ch_tmix[src - 1][1][i]) * main : (l + r) * 0.5f;
            if (stk_) { unsigned w = scope_w; scope_buf[w] = (int8_t)(ch_clamp((v + spv_) * 0.5f, -1, 1) * 127); scope_w = (uint16_t)((w + 1) & (CH_SCOPE_N - 1)); }
            else spv_ = v;
            stk_ ^= 1;
        }
        out[2 * (off + i)] = (int32_t)(l * 8388607.0f);
        out[2 * (off + i) + 1] = (int32_t)(r * 8388607.0f);
    }
    master_env = me_; master_g = mgn_; mg_s = mgs_; sub_l = sl_; sub_r = sr_; lim_rel = lr_; lim_sum = ls_; lim_g = lg_; scope_prev = spv_;
    lim_hold = lh_; lim_w = lw_; scope_tick = (uint8_t)stk_;
    ch_mix_level = pk;
    CH_P(13);
    master_env = ch_ftz(master_env); sub_l = ch_ftz(sub_l); sub_r = ch_ftz(sub_r);
    snap.lim_gr = gmin;
    { float t = 0; for (unsigned k = 0; k < LIM_D; ++k) t += lim_held[k]; lim_sum = t; }
    if ((ch_to_bits(master_env) & 0x7FFFFFFFu) > 0x46800000u || (ch_to_bits(master_g) & 0x7FFFFFFFu) > 0x46800000u
        || !(lim_g > 0 && lim_g <= 1.001f) || lim_on != lim_was) {   /* (the switch moved: its state as new) */
        if (lim_on != lim_was) { lim_was = lim_on; lim_reset(); }
        else {
        master_env = 0; master_g = 1; lim_reset(); ++snap.master_bad;
        for (unsigned i = 0; i < 2 * n; ++i) out[2 * off + i] = 0;
        }
    }
    if (pclk) mall += pclk() - t_in;
}

CH_AUDIO_CODE void ch_render(int32_t *out, unsigned frames, const int32_t *in)
{
#ifdef CHISEL_HOST
    if (!in) in = ch_host_in;
#endif
    CH_P(15);
    uint32_t r0 = pclk ? pclk() : 0;
    unsigned rd = ring_r;
    while (rd != ring_w) {
        const ch_cmd *c = &ring[rd];
        int st = disrupts(c);
        if (st >= 0) {
            ch_trk *t = c->t == CH_MASTER ? &ch_mst : &ch_tr[c->t];
            if (t->dk[st] < 1 && (stage_sounds(t, (unsigned)st) || t->dd[st] > 0)) { t->dd[st] = 1; break; }   /* fading out: the rest waits */
            apply(c);
            t->dk[st] = 1; t->dd[st] = -1;               /* made: back in, from silence (or from dry) */
        } else apply(c);
        rd = (rd + 1) & (RING - 1);
    }
    ring_r = rd;
    CH_P(0);
    for (unsigned ti = 0; ti < CH_TRACKS; ++ti) track_buffers(ti);
    master_buffers();
    CH_P(9);
    uint32_t macc = 0;
    if (sub_want != ch_sub) { ch_sub = sub_want; pm_reset = 1; }   /* the sub-block's length changed: what was kept for the master was the other length's */
    const int piped = frames % ch_sub == 0;              /* whole sub-blocks: the master a sub-block behind (below) */
    mall = 0;
    if (pm_reset) { memset(&pm, 0, sizeof pm); memset(&job, 0, sizeof job); memset(&rb, 0, sizeof rb); pm_reset = 0; }   /* (job and rb are in the pool since a sub-block can be 64 frames: no room beside the rest for twice as much; the pool is not cleared at power-on) */
    unsigned c2 = job_run && pclk ? split() : 0, c2_ran = 0;
    uint32_t sub_t = r0, sub_first = 0, sub_rest = 0, sub_n = 0;   /* the first sub-block's time against the others' (its code and tables fetched from the flash again: the cache's cost) */
    for (unsigned off = 0; off < frames; off += ch_sub) {
        unsigned n = frames - off < ch_sub ? frames - off : ch_sub;
        if (pclk && off) { uint32_t c = pclk(); if (off == ch_sub) sub_first = c - sub_t; else { sub_rest += c - sub_t; ++sub_n; } sub_t = c; }
        ch_t.bpm += (ch_t.bpm_target - ch_t.bpm) * 0.05f;
        ch_t.beats_per_frame = ch_t.bpm / (60.0f * CH_RATE);
        if (count_on) count_step(n);                       /* the count-in (in the flash: it runs for a bar, not always) */
        /* (this call's own buffers a sub-block long: in the pool, off the stack, which they were 1500 bytes of and would be 3000) */
        float *const inL = rb.inL, *const inR = rb.inR, *const mixL = rb.mixL, *const mixR = rb.mixR;
        memset(rb.mixL, 0, sizeof rb.mixL); memset(rb.mixR, 0, sizeof rb.mixR);
        float (*const side)[2][CH_SUB] = rb.side; uint8_t side_on = 0;   /* the tracks' shares that join the master chain at its 2nd, 3rd, 4th effect, or after it (each cleared by the first to use it) */
        if (in) for (unsigned i = 0; i < n; ++i) { inL[i] = in[2 * (off + i)] * (1.0f / 8388608); inR[i] = in[2 * (off + i) + 1] * (1.0f / 8388608); }
        ch_sub_off = off; ch_frames = frames;
        if (bus_live[send_phase ^ 1]) { memset(send_bus[send_phase ^ 1], 0, sizeof send_bus[0]); bus_live[send_phase ^ 1] = 0; }   /* (the sends' buses: cleared only when written) */
        if (pat_kind) {                                  /* a pattern switch: at once, or on the bar */
            ch_tf b1 = ch_t.beat + (ch_tf)ch_t.beats_per_frame * n;
            if (!ch_t.playing || pat_now || ch_t.started || (int32_t)(b1 * 0.25f) != (int32_t)(ch_t.beat * 0.25f)) pattern_apply();
        }
        if (ch_t.playing) {
            ch_tf b1 = ch_t.beat + (ch_tf)ch_t.beats_per_frame * n;
            if (stut_on && stut_stop >= 0 && ch_t.beat >= stut_stop - (ch_tf)1e-4f) { stut_on = 0; stut_stop = -1; }   /* the stutter's end reached */
            for (unsigned ti = 0; ti < CH_TRACKS; ++ti) seq_run(ti, ch_t.beat, b1);
        }
        CH_P(10);
        float (*next)[2][CH_SUB] = send_bus[send_phase ^ 1];
        int handed = 0;
        if (c2) {
            job.mask = c2; job.n = n; job.inL = in ? inL : 0; job.inR = in ? inR : 0;
            handed = job_run(job_tracks, 0) == 1;
        }
        if (piped) master_out(pm.l, pm.r, pm.side, pm.side_on, n, out, off, &macc);   /* the sub-block before's, while the second core works */
        for (unsigned ti = 0; ti < CH_TRACKS; ++ti)
            if (!(c2 >> ti & 1u)) track_sub(ti, in ? HOST_IN(ti, inL) : 0, in ? HOST_IN(ti, inR) : 0, mixL, mixR, next, n, &bus_live[send_phase ^ 1], side, &side_on);
        CH_P(0);
        if (handed && job_wait() != 0) handed = 0;           /* given up, the job dropped */
        if (c2) {
            if (!handed) job_tracks(0);                      /* (no second core after all: here) */
            else c2_ran = 1;
            for (unsigned i = 0; i < n; ++i) { mixL[i] += job.mixL[i]; mixR[i] += job.mixR[i]; }
            for (unsigned k = 0; k < 4; ++k) {
                if (!(job.side_on >> k & 1u)) continue;
                if (!(side_on >> k & 1u)) { memset(side[k], 0, sizeof side[k]); side_on |= (uint8_t)(1u << k); }
                for (unsigned i = 0; i < n; ++i) { side[k][0][i] += job.side[k][0][i]; side[k][1][i] += job.side[k][1][i]; }
            }
            for (unsigned d = 0; d < CH_TRACKS; ++d) {
                if (!(job.live >> d & 1u)) continue;
                for (unsigned i = 0; i < n; ++i) { next[d][0][i] += job.bus[d][0][i]; next[d][1][i] += job.bus[d][1][i]; }
            }
            bus_live[send_phase ^ 1] |= job.live;
        }
        send_phase ^= 1;
        /* The master. It ran here, after both cores' tracks, so a block took the slower core's tracks AND the master
         * chain AND the output stage one after another: with four master effects, 1240 us of 5800 that nothing ran
         * beside (RENDER 6188 with the cores at 4860 and 3790). Now the tracks' mix is kept, and the master works on the
         * mix kept the sub-block before, at the top of this loop while the second core does its tracks: the whole
         * output 32 frames (0.7 ms) later, and the block the slower of two sides, not their sum. (A call that is not
         * whole sub-blocks has no place for a sub-block kept over: it works as it did.) */
        if (piped) {
            for (unsigned i = 0; i < n; ++i) { pm.l[i] = mixL[i]; pm.r[i] = mixR[i]; }
            for (unsigned k = 0; k < 4; ++k) if (side_on >> k & 1u) memcpy(pm.side[k], side[k], sizeof side[k]);
            pm.side_on = side_on;
        } else master_out(mixL, mixR, side, side_on, n, out, off, &macc);
        if (ch_t.playing) {                    /* compensated sum: float on the FM-1 keeps its tempo */
            ch_tf y = (ch_tf)ch_t.beats_per_frame * n - ch_t.beat_c, s1 = ch_t.beat + y;
            ch_t.beat_c = (s1 - ch_t.beat) - y; ch_t.beat = s1;
        }
        ch_t.started = 0;
    }
    /* snapshot for the UI */
    snap.ret_level[0] = ret_pk[0]; snap.ret_level[1] = ret_pk[1];
    for (unsigned ti = 0; ti < CH_TRACKS; ++ti) {
        ch_trk *t = &ch_tr[ti];
        snap.level[ti][0] = t->peak_l; snap.level[ti][1] = t->peak_r;
        for (unsigned s = 0; s < CH_SLOTS; ++s) for (unsigned p = 0; p < CH_PARAMS; ++p) snap.value[ti][s][p] = t->v[s][p].l;
        for (unsigned p = 0; p < 3; ++p) snap.mixmod[ti][p] = t->mixmod[p];
        for (unsigned m = 0; m < CH_MODS; ++m) { snap.mod[ti][m] = t->mod[m].out_l; snap.mod_ph[ti][m] = (uint8_t)(ch_clamp(t->mod[m].hold_phase, 0, 0.999f) * 256); snap.mod_cyc[ti][m] = t->mod[m].cyc; }
        snap.locked[ti] = 0;
        snap.step[ti] = (int8_t)(ch_t.playing ? t->seq.shown : -1);
        {
            float sb = RATE_BEATS[t->seq.rate < 5 ? t->seq.rate : 1];
            float f = (float)((ch_t.beat - t->seq.shown_at) / sb);
            snap.step_frac[ti] = f < 0 ? 0 : f > 1 ? 1 : f;
        }
        snap.hit[ti] = t->drum.flash; snap.hits[ti] = ch_hits[ti];
        { uint8_t m = 0; for (unsigned l = 0; l < CH_LANES; ++l) if (t->seq.ln[l].gate_note) m |= (uint8_t)(1u << l); snap.lanes[ti] = m; }
        {   /* the envelope display: the loudest voice's level */
            float e = 0; unsigned dev = t->device[CH_MATERIAL];
            if (dev == CH_DEV_SYNTH) { for (unsigned i = 0; i < CH_SVOICES; ++i) if (t->synth.v[i].on && t->synth.v[i].env > e) e = t->synth.v[i].env; }
            else if (dev == CH_DEV_POLY) { for (unsigned i = 0; i < CH_POLY_VOICES; ++i) if (ch_voices[i].active && ch_voices[i].track == ti && ch_voices[i].env > e) e = ch_voices[i].env; }
            else if (dev == CH_DEV_GRAIN) e = t->grain.env;
            snap.env[ti] = ch_clamp(e, 0, 1);
        }
    }
    unsigned v = 0;
    for (unsigned i = 0; i < CH_POLY_VOICES; ++i) v += ch_voices[i].active != 0;
    snap.voices = (uint8_t)v;
    snap.playing = ch_t.playing;
    snap.metronome = metro_on;
    snap.count = count_on ? count_n : 0;
    CH_P(14);
    snap.pattern_pending = pat_kind != 0;
    snap.bar = (uint16_t)(ch_t.beat > 0 ? (uint32_t)(ch_t.beat * 0.25f) : 0);
    snap.beat = (float)ch_t.beat;
    snap.master[0] = snap.master[1] = ch_mix_level;
    ++snap.blocks;
    if (pclk) {                                          /* the load: this call's, into the peaks; published each second */
        uint32_t tot = (pclk() - r0) / ptpu;
        if (tot > prpk) prpk = tot;
        if (sub_n) {                                     /* (the last sub-block is left out: the master's publishing rides on it) */
            static float s0a, sna;
            s0a += ((float)sub_first / (float)ptpu - s0a) * 0.05f; sna += ((float)sub_rest / (float)(sub_n * ptpu) - sna) * 0.05f;
            snap.sub0_us = (uint16_t)s0a; snap.subn_us = (uint16_t)sna;
        }
        { uint32_t mus = macc / ptpu; if (mus > mpk) mpk = mus; }
        mcost = mcost ? (mcost * 7u + mall) / 8u : mall;   /* (averaged as the tracks' are; the chain and the output stage both) */
        for (unsigned ti = 0; ti < CH_TRACKS; ++ti) {
            uint32_t sum = 0;
            for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) { sum += pacc[ti][s2]; uint32_t us = pacc[ti][s2] / ptpu; if (us > ppk[ti][s2]) ppk[ti][s2] = us; pacc[ti][s2] = 0; }
            tcost[ti] = tcost[ti] ? (tcost[ti] * 7u + sum) / 8u : sum;   /* averaged over ~8 calls (46 ms): the split follows what a track costs, not which had a hit in the last block (it sent one kit and its effects to the second core and left three tracks on the first) */
        }
        snap.cores = c2_ran ? 2 : 1; snap.core2_tracks = (uint8_t)(c2_ran ? c2 : 0);
        /* the load governor: a render past 80 % of its time fades the oldest voice out and lowers the limit (then
         * waits 4 calls, for the fade); one under 55 % for half a second raises it again. So many notes at once never
         * miss the deadline (the fuzz): the voices that would have, fade instead */
        {
            static unsigned hold, calm; static float lavg;
            uint32_t bud = (uint32_t)frames * 1000000u / CH_RATE;
            float lr = bud ? (float)tot / (float)bud : 0;   /* this render's share of its time */
            lavg += (lr - lavg) * 0.25f;                     /* ~4 renders (about 20 ms): load, not one slow block */
            if (hold) --hold;
            if (lavg > 0.75f || lr > 0.95f) {                /* held high, or one about to miss its deadline */
                calm = 0;
                if (!hold) {
                    if (vb_cap > 3) --vb_cap;
                    unsigned k = voice_shed(vb_cap);           /* down to the limit only (young notes are never taken) */
                    snap.shed = (uint16_t)(snap.shed + k);
                    hold = 4;
                }
            } else if (lavg < 0.55f) {
                if (++calm * frames >= CH_RATE / 4) { calm = 0; if (vb_cap < CH_VOICE_BUDGET) ++vb_cap; }
            } else calm = 0;
            snap.voice_cap = (uint8_t)vb_cap;
            ch_grain_cap = (uint8_t)(vb_cap >= CH_VOICE_BUDGET ? CH_GRAINS1 : vb_cap > 4 ? vb_cap - 2 : 2);   /* a cloud thins too while the render runs late */
        }
        for (unsigned ti = 0; ti < CH_TRACKS; ++ti) for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) snap.bad[ti][s2] = bad_n[ti][s2];
        snap.bad_track = bad_at[0]; snap.bad_slot = bad_at[1]; snap.bad_kind = bad_at[2];
        for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) for (unsigned p = 0; p < CH_PARAMS; ++p) snap.mvalue[s2][p] = ch_mst.v[s2][p].l;
        pframes += frames;
        if (pframes >= CH_RATE) {
            for (unsigned ti = 0; ti < CH_TRACKS; ++ti)
                for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) { snap.load_us[ti][s2] = (uint16_t)(ppk[ti][s2] > 65535u ? 65535u : ppk[ti][s2]); ppk[ti][s2] = 0; }
            snap.render_us = (uint16_t)(prpk > 65535u ? 65535u : prpk); prpk = 0;
            snap.master_us = (uint16_t)(mpk > 65535u ? 65535u : mpk); mpk = 0;
            snap.budget_us = (uint16_t)((uint32_t)frames * 1000000u / CH_RATE);
            pframes = 0; ++pcount;
        }
    }
}

void ch_render_far(int32_t *out, unsigned frames, const int32_t *in) { ch_render(out, frames, in); }
#if defined(CH_PROF)
unsigned long long ch_prof[32], ch_prof_t;
#endif
