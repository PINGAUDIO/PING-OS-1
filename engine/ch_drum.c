/* DRUM: a percussion synthesiser for the MATERIAL slot. One device, 49
 * models, all driven by the same thirteen controls; SHAPE and COLOR take a
 * model-specific meaning (ch_drum_label). Two voices per track: CHOKE cuts
 * the last hit, RING lets it sound on under the next one. Retriggers are
 * declicked by carrying the old output in as a short decaying offset.
 *
 * Oscillator models (kicks, snares, toms, metal) are classic analogue
 * topologies; resonator models (rim, clave, conga, membrane, bell, ping) are
 * banks of two-pole resonators struck by an impulse plus a noise burst;
 * PLUCK is a Karplus-Strong string/drum; SHAKER is a particle model.
 *
 * The later hats and snares (HAT 606 .. SNARE LOFI) came from a look at how the
 * classics and the current modules do it: the 606's six squares (246..627 Hz)
 * through a narrow band, Plaits' ring-modulated square pairs, FM and clocked
 * noise hats, a wire (a short Karplus-Strong loop fed with noise), cymbals as
 * resonator banks under a faster-decaying wash, the Plaits analogue snare's
 * modes (1, 2, 3.18, 4.16, 5.62) with a 1 ms high-passed strike, a two-head
 * membrane with wires that rattle with the body, and a sample-held snare.
 *
 * The last eight (SNARE BAND .. RIDE 909), a second look, at the current drum machines' way: a drum as a body and a
 * layer of noise or a transient each shaped on its own, the noise through a band set by where it sits and how wide
 * it is (SNARE BAND, CLAP BAND, NOISE PERC: a band-pass, its damping the WIDTH, levelled so a narrow band is as loud
 * as a wide one), a clean kick with a click tuned apart from it (KICK CLICK), three FM pairs at inharmonic ratios
 * (METAL), a click through a comb (IMPULSE), and two rides: the 808's cymbal circuit voiced long (its six squares
 * through the circuit's two bands, 3.44 and 7.1 kHz, each with its own decay) and one after the 909's, which was a
 * 6-bit recording: ring-modulated square pairs high-passed, a struck ping under them, held at a low rate and cut to
 * few bits by GRIT. They are at the list's end: a model's number is what a pattern and a preset keep. */
#include "ch_internal.h"

enum { D_MODEL, D_PITCH, D_DECAY, D_SHAPE, D_COLOR, D_TONE, D_SNAP, D_BEND, D_NOISE, D_DRIVE, D_VARI, D_LEVEL, D_VEL, D_TAILS };

enum {   /* in the order shown: kicks, snares, claps and wood, hats, cymbals, toms, tonal, noise and digital */
    K808, K909, KFM, KHARD, K606, KDEEP, KSKIN, KDUB,
    S808, S909, SFM, SMODAL, SSKIN, SLOFI, CLAP, RIM,
    CLAVE, COWBELL, HAT808, HATNOISE, HAT606, HATRING, HATFM, HATCLOCK,
    HATWIRE, CYMBAL, CRASH, RIDE, TOM, CONGA, MEMBRANE, BELL,
    PLUCK, FMPERC, FM2, PING, SHAKER, ZAP, BIT, GLITCH, BOOM,
    SBAND, CLAPBAND, NOISEP, KCLICK, METALFM, IMPULSE, RIDE808, RIDE909
};
typedef struct { const char *name, *shape, *color; float hz, decay, bend, bend_t, gain; } model_t;
static const model_t M[CH_DRUM_MODELS] = {
    {"KICK 808",  "SWEEP",    "HARMONIC",   47,   0.60f, 20, 0.040f, 0.95f},
    {"KICK 909",  "ATTACK",   "WAVE",       50,   0.40f, 28, 0.025f, 0.93f},   /* its own pitch envelope (below) */
    {"KICK FM",   "RATIO",    "INDEX",      50,   0.45f, 24, 0.030f, 1.30f},
    {"KICK HARD", "FOLD",     "GRIT",       50,   0.50f, 36, 0.020f, 0.79f},
    {"KICK 606",  "CLICK",    "TONE",       64,   0.25f, 9,  0.012f, 1.00f},
    {"KICK DEEP", "PUNCH",    "WARMTH",     41,   1.20f, 14, 0.070f, 0.86f},
    {"KICK SKIN", "BEATER",   "DAMP",       58,   0.35f, 4,  0.030f, 0.80f},
    {"KICK DUB",  "RUMBLE",   "DARK",       50,   0.45f, 24, 0.030f, 0.86f},
    {"SNARE 808", "MIX",      "SNAPPY",     185,  0.20f, 3,  0.015f, 0.85f},
    {"SNARE 909", "SPREAD",   "NOISE TONE", 190,  0.22f, 10, 0.012f, 0.98f},   /* two triangles, soft-clipped; LP>HP noise held ~10 ms */
    {"SNARE FM",  "RATIO",    "NOISE",      200,  0.20f, 5,  0.020f, 1.19f},
    {"SNARE MODAL","MODES",   "SNARES",     180,  0.25f, 0,  0.010f, 0.60f},
    {"SNARE SKIN","STRIKE",   "WIRES",      190,  0.25f, 4,  0.030f, 0.27f},
    {"SNARE LOFI","RATE",     "BITS",       200,  0.25f, 12, 0.030f, 0.54f},
    {"CLAP",      "SPREAD",   "TAIL",       1100, 0.25f, 0,  0.010f, 1.38f},
    {"RIM",       "BODY",     "CLICK",      1700, 0.035f,0,  0.010f, 0.77f},
    {"CLAVE",     "WOOD",     "HARD",       2400, 0.07f, 0,  0.010f, 0.77f},
    {"COWBELL",   "RATIO",    "DECAY",      560,  0.30f, 0,  0.010f, 1.00f},
    {"HAT 808",   "METAL",    "BAND",       410,  0.09f, 0,  0.010f, 3.64f},
    {"HAT NOISE", "RESO",     "BAND",       7000, 0.07f, 0,  0.010f, 1.27f},
    {"HAT 606",   "SPREAD",   "BAND",       246,  0.08f, 0,  0.010f, 3.50f},
    {"HAT RING",  "RATIO",    "BAND",       200,  0.09f, 0,  0.010f, 0.90f},
    {"HAT FM",    "RATIO",    "INDEX",      1800, 0.10f, 0,  0.010f, 0.63f},
    {"HAT CLOCK", "RATE",     "BAND",       4000, 0.08f, 0,  0.010f, 0.72f},
    {"HAT WIRE",  "BRIGHT",   "BAND",       1200, 0.15f, 0,  0.010f, 0.78f},
    {"CYMBAL",    "METAL",    "NOISE",      300,  1.00f, 0,  0.010f, 0.66f},
    {"CRASH",     "METAL",    "WASH",       900,  1.20f, 0,  0.010f, 0.52f},
    {"RIDE",      "BELL",     "SIZZLE",     1000, 1.20f, 0,  0.010f, 0.80f},
    {"TOM",       "SWEEP",    "SKIN",       110,  0.40f, 9,  0.060f, 1.26f},
    {"CONGA",     "SLAP",     "MUTE",       220,  0.30f, 2,  0.030f, 1.00f},
    {"MEMBRANE",  "STRIKE",   "DAMP",       140,  0.60f, 1.5f,0.080f,0.60f},
    {"BELL",      "MATERIAL", "POSITION",   440,  1.50f, 0,  0.010f, 0.56f},
    {"PLUCK",     "BRIGHT",   "DRUM",       110,  0.80f, 0,  0.010f, 1.09f},
    {"FM PERC",   "RATIO",    "INDEX",      300,  0.35f, 0,  0.010f, 1.04f},
    {"FM THUMP",  "RATIO",    "FM AMT",     52,   0.45f, 30, 0.028f, 1.25f},
    {"PING",      "SWEEP",    "SUB",        900,  0.30f, 12, 0.030f, 0.86f},
    {"SHAKER",    "DENSITY",  "RESO",       6000, 0.18f, 0,  0.010f, 1.65f},
    {"ZAP",       "SWEEP",    "FEEDBACK",   70,   0.25f, 60, 0.030f, 1.28f},
    {"BIT",       "NOISE",    "TONE MIX",   2000, 0.15f, 24, 0.040f, 1.19f},
    {"GLITCH",    "CHAOS",    "RATE",       300,  0.20f, 0,  0.010f, 1.12f},
    {"808 BOOM",  "PUNCH",    "SATURATE",   41,   2.40f, 12, 0.045f, 0.92f},   /* the long 808 sub: a sine diving an octave to E1, PUNCH a click of harmonics, SATURATE its growl */
    {"SNARE BAND","BAND",     "WIDTH",      190,  0.22f, 6,  0.015f, 1.15f},
    {"CLAP BAND", "BAND",     "WIDTH",      1100, 0.25f, 0,  0.010f, 1.70f},
    {"NOISE PERC","SWEEP",    "RESO",       2000, 0.20f, 24, 0.030f, 2.20f},
    {"KICK CLICK","CLICK TONE","CLICK LEN", 50,   0.45f, 18, 0.035f, 0.95f},
    {"METAL",     "RATIO",    "SPREAD",     400,  0.90f, 0,  0.010f, 1.05f},
    {"IMPULSE",   "HARD",     "DAMP",       600,  0.12f, 0,  0.010f, 2.30f},
    {"RIDE 808",  "METAL",    "BAND",       205,  1.20f, 0,  0.010f, 1.35f},
    {"RIDE 909",  "BELL",     "GRIT",       320,  1.40f, 0,  0.010f, 0.55f},
};
/* The list as it is shown and kept (the MODEL a pattern, a preset and a kit hold is a place in THIS list): by kind,
 * kicks, snares, claps and wood, hats, cymbals and metal, skins and shakers, tuned things, noise and digital. The
 * models themselves stay numbered as they were made (the enum above: the voice's code is in that order and is not
 * touched); ch_drum_order is the model at each place */
const uint8_t ch_drum_order[CH_DRUM_MODELS] = {
    K808, K909, KFM, KHARD, K606, KDEEP, KSKIN, KDUB, KCLICK, BOOM,
    S808, S909, SFM, SMODAL, SSKIN, SLOFI, SBAND,
    CLAP, CLAPBAND, RIM, CLAVE, COWBELL,
    HAT808, HATNOISE, HAT606, HATRING, HATFM, HATCLOCK, HATWIRE,
    CYMBAL, CRASH, RIDE, RIDE808, RIDE909, METALFM,
    TOM, CONGA, MEMBRANE, SHAKER,
    BELL, PLUCK, PING, IMPULSE, FMPERC, FM2,
    NOISEP, ZAP, BIT, GLITCH,
};
static inline unsigned model_at(unsigned place) { return ch_drum_order[place < CH_DRUM_MODELS ? place : CH_DRUM_MODELS - 1]; }
const char *const ch_drum_names[CH_DRUM_MODELS + 1] = {
    "KICK 808", "KICK 909", "KICK FM", "KICK HARD", "KICK 606", "KICK DEEP", "KICK SKIN", "KICK DUB", "KICK CLICK", "808 BOOM",
    "SNARE 808", "SNARE 909", "SNARE FM", "SNARE MODAL", "SNARE SKIN", "SNARE LOFI", "SNARE BAND",
    "CLAP", "CLAP BAND", "RIM", "CLAVE", "COWBELL",
    "HAT 808", "HAT NOISE", "HAT 606", "HAT RING", "HAT FM", "HAT CLOCK", "HAT WIRE",
    "CYMBAL", "CRASH", "RIDE", "RIDE 808", "RIDE 909", "METAL",
    "TOM", "CONGA", "MEMBRANE", "SHAKER",
    "BELL", "PLUCK", "PING", "IMPULSE", "FM PERC", "FM THUMP",
    "NOISE PERC", "ZAP", "BIT", "GLITCH",
    0};
const char *ch_drum_label(unsigned model, unsigned param)
{
    if (model >= CH_DRUM_MODELS) return 0;
    model = model_at(model);
    return param == D_SHAPE ? M[model].shape : param == D_COLOR ? M[model].color : 0;
}

static int16_t ks_pool[CH_TRACKS][CH_DVOICES][CH_KS_LEN];   /* plain RAM: the pool is full */

/* membrane (circular, Bessel zeros), bar and bell partials */
static const float MEMB[8] = {1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f};
static const float BAR[6] = {1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f};
static const float BELLR[6] = {0.5f, 1.0f, 1.19f, 1.5f, 2.0f, 2.74f};
static const float METAL[6] = {1.0f, 1.483f, 1.800f, 2.546f, 2.630f, 3.897f};   /* 808 hat squares / 205.3 Hz */
static const float METAL606[6] = {1.0f, 1.252f, 1.492f, 1.699f, 1.789f, 2.549f};   /* 606 hat squares / 246 Hz */
static const float RINGR[6] = {1.0f, 2.55f, 3.65f, 37.65f, 40.38f, 52.5f};     /* ring-mod pairs (k, k+3), after Plaits: 200/7530, 510/8075, 730/10500 */
static const float CRASHR[6] = {1.0f, 1.44f, 1.93f, 2.47f, 3.21f, 4.12f};      /* a crash's shimmer modes */
static const float RIDER[4] = {1.0f, 1.52f, 2.07f, 2.96f};                     /* a ride: the ping and its bell */
static const float SNMOD[5] = {1.0f, 2.0f, 3.18f, 4.16f, 5.62f};               /* a snare drum's modes (Plaits' analogue snare) */

static inline float dcoef(float sec) { return ch_exp2(-9.966f / (ch_max(sec, 0.0003f) * (float)CH_RATE)); }
static inline float wrap1(float p) { p -= (float)(int)p; return p < 0 ? p + 1 : p; }
static inline float tri(float p) { return 4.0f * ch_abs(p - 0.5f) - 1.0f; }
static inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
static inline float blep_i(float t, float dt, float idt)
{
    if (t < dt) { t *= idt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) * idt; return t * t + t + t + 1; }
    return 0;
}
/* a square with its edges smoothed, the step's reciprocal given (worked out once a block, not divided each sample) */
static inline float sqr_i(float *ph, float inc, float iinc)
{
    float p = *ph, s = p < 0.5f ? 1.0f : -1.0f;
    s += blep_i(p, inc, iinc);
    float q = p + 0.5f; if (q >= 1) q -= 1;
    s -= blep_i(q, inc, iinc);
    p += inc; if (p >= 1) p -= 1;
    *ph = p;
    return s;
}
static inline float sqr(float *ph, float inc)
{
    float p = *ph, s = p < 0.5f ? 1.0f : -1.0f;
    s += blep(p, inc);
    float q = p + 0.5f; if (q >= 1) q -= 1;
    s -= blep(q, inc);
    p += inc; if (p >= 1) p -= 1;
    *ph = p;
    return s;
}
static inline float osc_sin(float *ph, float inc) { float p = *ph; *ph = wrap1(p + inc); return ch_sin_turns(p); }

/* resonator bank: set mode k to hz with a -60 dB time and a strike gain. Each mode a rotation (the coupled form): its
 * state turned by the angle of its pitch and shrunk by its decay each sample. (It was the two-pole direct form, which at
 * a low pitch with a long ring, a kick tuned down, a membrane, sits within a rounding of its stability edge in single
 * precision, and could grow until the engine's guard silenced the track; and which, retuned while ringing, a BEND, could
 * swell. The rotation's poles are its decay exactly, at any pitch, and a retune keeps the ring as loud as it was. The
 * same sound: a struck mode rings g r^n sin(wn), and the gain to a sustained drive at the mode is the same) */
static void res_set(ch_dvoice *v, unsigned k, float hz, float t60, float g)
{
    float w = hz / (float)CH_RATE;
    if (w > 0.45f || g == 0) { v->rb1[k] = v->rb2[k] = v->rg[k] = 0; return; }
    float r = dcoef(ch_max(t60, 0.002f));   /* -60 dB after t60 */
    v->rb1[k] = r * ch_sin_turns(w + 0.25f);   /* r cos w */
    v->rb2[k] = r * ch_sin_turns(w);           /* r sin w */
    v->rg[k] = g;
}
static inline float res_tick(ch_dvoice *v, unsigned nm, float x)
{
    float y = 0;
    for (unsigned k = 0; k < nm; ++k) {
        float c = v->rb1[k], s = v->rb2[k], a = v->ry1[k], b = v->ry2[k];
        v->ry1[k] = c * a - s * b + x * v->rg[k];
        v->ry2[k] = s * a + c * b;                       /* the sine phase: the mode's output */
        y += v->ry2[k];
    }
    return y;
}

typedef struct { float pitch, dec, shape, color, tone, snap, bend, noise, drive, var, lvl, vels; int chroma; } dparams;

void ch_drum_init(ch_trk *t, unsigned ti)
{
    for (unsigned i = 0; i < CH_DVOICES; ++i) {
        ch_dvoice *v = &t->drum.v[i];
        memset(v, 0, sizeof *v);
        v->ks = ks_pool[ti][i];
        v->rng.s = 0x2545F491u * (ti * 2 + i + 1);
        v->lfsr = 0x7fff;
    }
}

static void strike(ch_dvoice *v, unsigned model, unsigned note, unsigned vel, float var, float bright)   /* (the notes' starts in the flash: see ch_synth_note) */
{
    v->dc = v->on ? v->last : 0; v->dcp = 1;            /* a voice still sounding: its last sound let go, not cut */
    v->on = 1; v->model = (uint8_t)model; v->note = (uint8_t)note;
    v->vel = (float)vel * (1.0f / 127);
    v->n = 0;
    v->amp = v->aux = v->aux2 = v->pit = v->snap = 1;
    v->fb = v->hold = v->sh = 0;
    v->chaos = 0.37f + 0.2f * ch_randf(&v->rng);
    v->var_p = ch_rands(&v->rng) * var * 0.8f;
    v->var_d = 1 + ch_rands(&v->rng) * var * 0.35f;
    v->var_s = ch_rands(&v->rng) * var * 0.2f;
    int squares = (model >= HAT808 && model <= RIDE) || model == RIDE808 || model == RIDE909;   /* the hats and cymbals: their squares start anywhere */
    for (unsigned k = 0; k < 6; ++k) v->ph[k] = squares ? ch_randf(&v->rng) : 0;
    for (unsigned k = 0; k < CH_DRUM_MODES; ++k) v->ry1[k] = v->ry2[k] = 0;
    memset(v->f, 0, sizeof v->f);
    v->lp = 0;
    if (model == PLUCK) {
        /* the excitation: noise, darker with less BRIGHT */
        float b = 0.1f + 0.9f * ch_clamp(bright, 0, 1), z = 0;
        for (unsigned i = 0; i < CH_KS_LEN; ++i) {
            z += (ch_rands(&v->rng) - z) * b;
            v->ks[i] = (int16_t)(ch_clamp(z * 1.6f, -1, 1) * 30000);
        }
        v->ks_w = 0; v->ks_prev = 0;
    }
    if (model == HATWIRE || model == IMPULSE) { memset(v->ks, 0, sizeof(int16_t) * CH_KS_LEN); v->ks_w = 0; v->ks_prev = 0; }
}

void ch_drum_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel)
{
    (void)ti;
    if (!vel) return;
    ch_drum *d = &t->drum;
    unsigned model = model_at((unsigned)(PV(t, CH_MATERIAL, D_MODEL) + 0.5f));
    int ring = PV(t, CH_MATERIAL, D_TAILS) > 0.5f;
    /* CHOKE: earlier hits fade out; hits of the same chord (struck within ~1.5 ms) ring together */
    if (!ring) for (unsigned k = 0; k < CH_DVOICES; ++k) if (d->v[k].on && d->v[k].n > 64) d->v[k].kill = 1;
    /* a free voice, else a fading one, else the oldest */
    unsigned pick = CH_DVOICES, best = 0;
    for (unsigned k = 0; k < CH_DVOICES && pick == CH_DVOICES; ++k) if (!d->v[k].on) pick = k;
    for (unsigned k = 0; k < CH_DVOICES && pick == CH_DVOICES; ++k) if (d->v[k].kill && d->v[k].n > 64) pick = k;
    (void)best;
    if (pick == CH_DVOICES) {                            /* all four sounding: the one that matters least (not the oldest: a kick rings longest) */
        float least = 1e30f;
        for (unsigned k = 0; k < CH_DVOICES; ++k) { float w = ch_dvoice_worth(&d->v[k]); if (w < least) { least = w; pick = k; } }
    }
    d->cur = (uint8_t)pick;
    ch_dvoice *v = &d->v[pick];
    v->kill = 0; v->fade = 1; v->part = 0; v->quiet = 0;
    strike(v, model, note, vel, PV(t, CH_MATERIAL, D_VARI), PV(t, CH_MATERIAL, D_SHAPE));
    d->flash = 1;
}
/* KIT: a part struck. Its model and level from the kit's extras (CH_KIT_XROW), the rest of its sound the DRUM's defaults;
 * the part's own last hit makes way (a part is one drum), and the closed hat (part 3) chokes the open one (part 4), as
 * on an 808. The track's four voices are shared: a free one, else the part's own, else a fading one, else the oldest */
float ch_dvoice_worth(const ch_dvoice *v)
{
    float w = v->amp * v->fade;
    int low = M[v->model < CH_DRUM_MODELS ? v->model : 0].hz < 150;
    if (low) w *= 2;                                     /* the low end: what a beat stands on */
    return w + (v->n < (low ? 13230u : 6615u) ? 2.0f : 0);   /* its body kept whole: 150 ms of a hit, 300 of a low one; only a tail may go */
}
void ch_kit_note(ch_trk *t, unsigned ti, unsigned part, unsigned vel)
{
    (void)ti;
    if (!vel || part >= CH_KIT_PARTS) return;
    ch_drum *d = &t->drum;
    unsigned model = model_at((unsigned)(t->base[CH_KIT_XROW][part] + 0.5f));
    for (unsigned k = 0; k < CH_DVOICES; ++k)
        if (d->v[k].on && d->v[k].n > 64 && (d->v[k].part == part || (part == 2 && d->v[k].part == 3))) d->v[k].kill = 1;
    unsigned pick = CH_DVOICES, best = 0;
    for (unsigned k = 0; k < CH_DVOICES && pick == CH_DVOICES; ++k) if (!d->v[k].on) pick = k;
    for (unsigned k = 0; k < CH_DVOICES && pick == CH_DVOICES; ++k) if (d->v[k].part == part && d->v[k].kill) pick = k;
    for (unsigned k = 0; k < CH_DVOICES && pick == CH_DVOICES; ++k) if (d->v[k].kill && d->v[k].n > 64) pick = k;
    (void)best;
    if (pick == CH_DVOICES) {                            /* all four sounding: the one that matters least (not the oldest: a kick rings longest) */
        float least = 1e30f;
        for (unsigned k = 0; k < CH_DVOICES; ++k) { float w = ch_dvoice_worth(&d->v[k]); if (w < least) { least = w; pick = k; } }
    }
    d->cur = (uint8_t)pick;
    ch_dvoice *v = &d->v[pick];
    if (v->on && !v->kill && v->amp > 0.1f && v->part < 8) ++ch_cut_kit[v->part];
    const ch_device_desc *dd = ch_device_info(CH_DEV_DRUM);
    v->kill = 0; v->fade = 1; v->part = (uint8_t)part; v->quiet = 0;
    strike(v, model, (unsigned)(60 + d->kit_oct[part]), vel, 0, dd->param[D_SHAPE].def);   /* (its step's octave: the drum that much higher or lower) */
    d->flash = 1;
}

static CH_NOINLINE void voice_run_a(ch_dvoice *v, const dparams *P, float *out, unsigned n)
{
    const model_t *m = &M[v->model];
    float semis = P->pitch + (float)((int)v->note - 60) + v->var_p;
    float hz = ch_clamp(m->hz * ch_semis(semis), 8, 18000);
    float dec = m->decay * P->dec * v->var_d;
    float shape = ch_clamp(P->shape + v->var_s, 0, 1), color = P->color;
    float bend = m->bend + P->bend * 36;
    float bt = m->bend_t;
    float ca = dcoef(dec), csnap = dcoef(0.006f), cn = dcoef(dec * 0.5f);
    float caux = dcoef(0.08f), cp;
    float nb_g = ch_svf_g(ch_clamp(hz * 6, 300, 12000));
    float tg = P->tone < -0.02f ? ch_svf_g(ch_exp_map(1 + P->tone, 150, 20000)) : P->tone > 0.02f ? ch_svf_g(ch_exp_map(P->tone, 20, 4000)) : 0;
    float vg = 1 - P->vels * (1 - v->vel * v->vel);
    float gain = m->gain * P->lvl * vg;
    float g0 = 0, g1 = 0, k0 = 1, k1 = 1, r = 1, x0 = 0;
    unsigned nm = 0;
    int res_bend = 0;
    float fstep = v->kill ? CH_KILL_STEP : 0;          /* stopped: out over 8 ms on an S curve */
    /* model setup for this sub-block */
    switch (v->model) {
    case K808: bt = 0.006f + shape * 0.08f; break;
    /* the 909: a triangle VCO through a diode-ish shaper, a two-stage pitch drop (a plunge in the first
     * milliseconds, then a glide down to the note over ~50 ms), and the click: a short pulse and a noise burst */
    case K909: k0 = dcoef(0.028f); k1 = dcoef(0.25f); r = dcoef(0.006f); x0 = dcoef(0.014f); g0 = ch_svf_g(4200); break;
    case K606: k0 = dcoef(0.05f); r = dcoef(0.004f); g0 = ch_svf_g(6000); break;
    case KDEEP: k0 = dcoef(0.35f); k1 = dcoef(0.06f); break;
    case KSKIN: nm = 4; res_bend = 1; caux = dcoef(0.004f + 0.008f * (1 - shape)); g0 = ch_svf_g(1500 + shape * 6000); break;
    case KDUB: k0 = dcoef(0.12f); caux = dcoef(dec * 3); g0 = ch_svf_g(ch_exp_map(1 - color, 50, 220)); break;
    case KFM: r = ch_exp_map(shape, 0.5f, 4); caux = dcoef(dec * 0.3f); break;
    case KHARD: break;
    case S808: caux = dcoef(dec * 0.6f); ca = dcoef(dec * (0.5f + color * 1.5f)); g0 = ch_svf_g(1800 + color * 3000); break;
    case S909: caux = dcoef(dec * 0.5f); r = 1.5f + shape * 0.7f; g0 = ch_svf_g(7000); g1 = ch_svf_g(ch_exp_map(color, 400, 3000)); k0 = dcoef(dec * 0.45f); break;
    case SFM: caux = dcoef(dec * 0.5f); r = 1.41f + shape * 2; g0 = ch_svf_g(ch_clamp(hz * 8, 500, 12000)); break;
    case CLAP: ca = dcoef(dec * (0.3f + color * 1.7f)); g0 = ch_svf_g(hz); g1 = ch_svf_g(500); r = 0.005f + shape * 0.015f; break;
    case RIM: nm = 2; res_set(v, 0, hz, dec, 1); res_set(v, 1, hz * 0.28f, dec * 1.5f, shape * 1.2f); caux = dcoef(0.002f); break;
    case CLAVE: nm = 2; res_set(v, 0, hz, dec, 1); res_set(v, 1, hz * 2.1f, dec * 0.5f, shape * 0.5f); caux = dcoef(0.0015f); break;
    case COWBELL: r = 1.2f + shape * 0.7f; g0 = ch_svf_g(ch_clamp(hz * 1.4f, 200, 15000)); caux = dcoef(0.03f); break;
    case HAT808: case CYMBAL: {
        g0 = ch_svf_g(v->model == HAT808 ? ch_exp_map(color, 5000, 12000) : 3500);
        g1 = ch_svf_g(v->model == HAT808 ? 6500 : 8000);
        if (v->model == CYMBAL) caux = dcoef(dec * 0.35f);
        break;
    }
    case HATNOISE: g0 = ch_svf_g(ch_min(hz, 16000)); k0 = 2 - shape * 1.85f; g1 = ch_svf_g(ch_min(hz * 1.3f, 17000)); break;
    case TOM: bt = 0.06f * (0.3f + shape * 1.4f); caux = dcoef(dec * 0.25f); g0 = ch_svf_g(ch_min(hz * 2.5f, 15000)); break;
    case CONGA: nm = 2; res_bend = 1; caux = dcoef(0.01f); g0 = ch_svf_g(2500); break;
    case MEMBRANE: nm = 8; res_bend = 1; caux = dcoef(0.003f); break;
    case BELL: nm = 6; caux = dcoef(0.002f); break;
    case PLUCK: break;
    case FMPERC: r = ch_exp_map(shape, 0.5f, 8); caux = dcoef(dec * 0.4f); break;
    case FM2: {                                      /* RATIO on the musical steps; the FM amount's own, faster, decay */
        static const float R2[8] = {0.5f, 1, 1.5f, 2, 3, 4, 5.5f, 7};
        r = R2[(int)(ch_clamp(shape, 0, 0.999f) * 8)];
        caux = dcoef(dec * (0.12f + 0.3f * color));
        break;
    }
    case SHAKER: x0 = ch_exp_map(shape, 0.002f, 0.08f); g0 = ch_svf_g(ch_min(hz, 16000)); k0 = 2 - color * 1.8f; caux = dcoef(0.02f); break;
    case ZAP: bt = 0.005f + shape * 0.15f; break;
    case BIT: caux = 1; break;
    case PING: nm = 1; res_bend = 1; bt = 0.005f + shape * 0.1f; caux = dcoef(dec * 1.5f); break;
    case GLITCH: r = 3.5f + shape * 0.5f; x0 = hz * 0.25f * (0.5f + color * 4) / (float)CH_RATE; break;
    case HAT606: g0 = ch_svf_g(ch_exp_map(color, 5000, 11000)); g1 = ch_svf_g(7500); break;
    case HATRING: g0 = ch_svf_g(ch_exp_map(color, 2500, 9000)); g1 = ch_svf_g(ch_exp_map(color, 4000, 11000)); break;
    case HATFM: r = 1.37f + shape * 6; caux = dcoef(dec * 0.6f); g0 = ch_svf_g(ch_clamp(hz * 1.5f, 2000, 12000)); break;
    case HATCLOCK: x0 = ch_min(hz * ch_exp_map(shape, 0.5f, 8) * (1.0f / (float)CH_RATE), 1); g0 = ch_svf_g(ch_exp_map(color, 3000, 10000)); g1 = ch_svf_g(5000); break;
    case HATWIRE: caux = dcoef(dec * 0.3f); g0 = ch_svf_g(ch_exp_map(color, 1000, 8000)); break;
    case CRASH: nm = 6; caux = dcoef(dec * 0.08f); k0 = dcoef(dec * 0.35f); g0 = ch_svf_g(ch_exp_map(color, 4000, 9000)); break;
    case RIDE: nm = 4; caux = dcoef(dec * 0.5f); g0 = ch_svf_g(ch_exp_map(color, 5000, 10000)); break;
    case SMODAL: nm = 5; k0 = dcoef(dec * 0.6f); g0 = ch_svf_g(2000); g1 = ch_svf_g(ch_clamp(hz * 16, 1500, 9000)); break;
    case SSKIN: nm = 4; res_bend = 1; caux = dcoef(0.004f + 0.006f * (1 - shape)); k0 = dcoef(dec * 0.8f); g0 = ch_svf_g(1500 + shape * 6000); g1 = ch_svf_g(2500); break;
    case SLOFI: caux = dcoef(dec * 0.6f); x0 = ch_exp_map(shape, 22050, 3000) * (1.0f / (float)CH_RATE); r = ch_exp_map(1 - color, 4, 256); g0 = ch_svf_g(1500); break;
    }
    cp = dcoef(bt);
    if (nm && (res_bend || v->n == 0)) {
        float pm = bend != 0 ? ch_semis(bend * v->pit) : 1, f = hz * pm;
        if (v->model == KSKIN) {                         /* a kick drum's head: its first four modes */
            for (unsigned k = 0; k < 4; ++k)
                res_set(v, k, f * MEMB[k], dec * (1 - color * 0.7f) / (1 + (float)k * 1.5f), k ? 0.45f / (float)(k + 1) : 1.0f);
        } else if (v->model == CONGA) {
            float mute = 1 - color * 0.8f;
            res_set(v, 0, f, dec * mute, 1); res_set(v, 1, f * 1.98f, dec * 0.3f * mute, 0.25f);
        } else if (v->model == MEMBRANE) {
            for (unsigned k = 0; k < 8; ++k) {
                float g = ch_lerp(1.0f / (float)(k + 1), 1.0f, shape) * ((k & 1) ? -1.0f : 1.0f);
                res_set(v, k, f * MEMB[k], dec / (1 + (float)k * color * 1.2f), g);
            }
        } else if (v->model == BELL) {
            for (unsigned k = 0; k < 6; ++k) {
                float ratio = ch_exp2(ch_lerp(ch_log2(BAR[k]), ch_log2(BELLR[k]), shape));
                float g = ch_abs(ch_sin_turns((float)(k + 1) * (0.07f + color * 0.4f))) + 0.05f;
                res_set(v, k, f * ratio, dec / (1 + (float)k * 0.35f), g * 0.8f);
            }
        } else if (v->model == PING) res_set(v, 0, f, dec, 1);
        else if (v->model == CRASH) {                   /* METAL: the modes squeezed together or stretched apart */
            float e = 0.6f + shape * 0.8f;
            for (unsigned k = 0; k < 6; ++k) res_set(v, k, f * ch_exp2(ch_log2(CRASHR[k]) * e), dec / (1 + (float)k * 0.3f), 0.5f / (1 + (float)k * 0.25f));
        } else if (v->model == RIDE) {                  /* the ping, then BELL brings its bell partials up */
            res_set(v, 0, f, dec, 0.7f);
            for (unsigned k = 1; k < 4; ++k) res_set(v, k, f * RIDER[k], dec / (1 + (float)k * 0.4f), shape * 0.5f / (float)k);
        } else if (v->model == SMODAL) {                /* MODES: the upper modes against the fundamental */
            for (unsigned k = 0; k < 5; ++k) res_set(v, k, f * SNMOD[k], dec / (1 + (float)k * 0.5f), k ? (0.15f + shape * 0.7f) / (float)k : 1.0f);
        } else if (v->model == SSKIN) {                 /* the batter head's first four modes, the strike damping the upper ones */
            for (unsigned k = 0; k < 4; ++k) res_set(v, k, f * MEMB[k], dec * (1 - shape * 0.4f) / (1 + (float)k * 1.2f), k ? 0.5f / (float)(k + 1) : 1.0f);
        }
    }
    float L_ks = 0, g_ks = 1;
    if (v->model == PLUCK || v->model == HATWIRE) {
        L_ks = ch_clamp((float)CH_RATE / hz, 4, CH_KS_LEN - 4);
        g_ks = ch_exp2(-9.966f * L_ks / (dec * (float)CH_RATE));
    }
    float pk = 0;
    /* what the samples share: the hats' six rates and their reciprocals, the kicks' saturation's fixed parts.
     * (On the FM-1 every per-sample division and exponential counts: a hat voice was ~400 us of a 2.9 ms block.) */
    float hinc[6], hiinc[6], sat_n = 1, sat_o = 0;
    if (v->model == HAT808 || v->model == CYMBAL || v->model == HAT606) {
        const float *rt = v->model == HAT606 ? METAL606 : METAL;
        float i0 = ch_min(hz * (1.0f / (float)CH_RATE), 0.45f), st = (shape - 0.5f) * (v->model == HAT606 ? 0.16f : 0.1f);
        for (unsigned k = 0; k < 6; ++k) { hinc[k] = ch_min(i0 * rt[k] * (1 + st * (float)k), 0.45f); hiinc[k] = 1 / hinc[k]; }
    } else if (v->model == HATRING) {   /* RATIO moves the high partners of the three pairs */
        float i0 = hz * (1.0f / (float)CH_RATE), hi = 0.6f + shape * 0.8f;
        for (unsigned k = 0; k < 6; ++k) { hinc[k] = ch_clamp(i0 * RINGR[k] * (k >= 3 ? hi : 1), 1e-4f, 0.45f); hiinc[k] = 1 / hinc[k]; }
    }
    if (v->model == K909) { float drv = 1.4f + color * 3.5f; sat_n = 1 / ch_tanh(drv); }
    else if (v->model == KDEEP) { float g = 1 + color * 3; sat_n = 1 / ch_tanh(g); }
    else if (v->model == KHARD) { float g = 1 + color * 6, o = color * 0.3f; sat_n = 1 / ch_tanh(g); sat_o = ch_tanh(o * g); }
    else if (v->model == S909) sat_n = 1 / ch_tanh(1.6f);
    else if (v->model == BOOM) sat_n = 1 / ch_tanh(1 + color * 7);   /* (its saturation's make-up, once a block, not a division a sample) */
    /* Two loops, each with few values in hand (on the FM-1 a read or a write of memory costs twice a multiply-add, and a
     * loop holding more values than the chip has registers reads its own stack all the time): the first makes the
     * model's sound and moves the envelopes, the voice's running values in locals; the second is TONE, DRIVE and level */
    float yb[CH_SUB];
    float amp_ = v->amp, pit_ = v->pit, snap_ = v->snap, aux2_ = v->aux2, aux_ = v->aux, hold_ = v->hold;
    uint32_t vn_ = v->n;
    const float psnap = P->snap * 0.9f, pnoise = P->noise * 0.8f;
    const int has_snap = P->snap > 0, has_noise = P->noise > 0.001f, not_clap = v->model != CLAP;
    /* The metal models' six squares, an oscillator at a time over the block, its phase and step in hand (in the sample
     * loop the six were read from the voice and written back every sample, 12 of them: half the cost of a hat, and the
     * hats are most of a beat's hits). The same sums in the same order */
    float sb[CH_SUB];
    if (v->model == HAT808 || v->model == CYMBAL || v->model == HAT606) {
        for (unsigned i = 0; i < n; ++i) sb[i] = 0;
        for (unsigned k = 0; k < 6; ++k) {
            float ph = v->ph[k]; const float hi = hinc[k], hii = hiinc[k];
            for (unsigned i = 0; i < n; ++i) sb[i] += sqr_i(&ph, hi, hii);
            v->ph[k] = ph;
        }
    } else if (v->model == HATRING) {   /* three pairs, each a product */
        float tb[CH_SUB];
        for (unsigned i = 0; i < n; ++i) sb[i] = 0;
        for (unsigned k = 0; k < 3; ++k) {
            float ph = v->ph[k]; const float hi = hinc[k], hii = hiinc[k];
            for (unsigned i = 0; i < n; ++i) tb[i] = sqr_i(&ph, hi, hii);
            v->ph[k] = ph;
            float ph2 = v->ph[k + 3]; const float hi2 = hinc[k + 3], hii2 = hiinc[k + 3];
            for (unsigned i = 0; i < n; ++i) sb[i] += tb[i] * sqr_i(&ph2, hi2, hii2);
            v->ph[k + 3] = ph2;
        }
    }
    /* the noise is drawn only where something reads it (a model that has none, no SNAP, no NOISE layer: not at all) */
    int need_noise = has_snap || has_noise;
    switch (v->model) {
    case BOOM: case K808: case KDEEP: case KFM: case KHARD: case COWBELL: case PLUCK: case FM2: case FMPERC: case ZAP: case BIT: case PING: case GLITCH: case HAT606: case HATRING: case HATFM: break;
    default: need_noise = 1; break;
    }
#if defined(CH_NULLTEST)
    need_noise = 1;                                      /* (the bench's null test: every draw made, as before) */
#endif
    const int bends = bend != 0 && !nm;                  /* the pitch envelope moves the note (every 4 samples); else the step is the block's */
    float f = hz, inc = ch_min(f * (1.0f / (float)CH_RATE), 0.45f);
    for (unsigned i = 0; i < n; ++i) {
        float noise = need_noise ? ch_rands(&v->rng) : 0;
        if (bends && (i & 3u) == 0) { f = hz * ch_semis(bend * pit_); inc = ch_min(f * (1.0f / (float)CH_RATE), 0.45f); }
        float y = 0, e;
        switch (v->model) {
        case BOOM: {                                    /* the 808 boom: a sine (its dive the pitch envelope), driven into a tanh by SATURATE,
                                                         * PUNCH a short burst of its second and third harmonics */
            float p = v->ph[0], s = osc_sin(&v->ph[0], inc), g = 1 + color * 7;
            float body = ch_tanh(s * g) * sat_n;
            y = (body + shape * aux_ * (0.5f * ch_sin_turns(2 * p) + 0.3f * ch_sin_turns(3 * p))) * amp_;
            aux_ *= caux;
            break;
        }
        case K808: {
            float p = v->ph[0], s = osc_sin(&v->ph[0], inc);
            y = (s + color * 0.6f * ch_sin_turns(2 * p) * aux_) * amp_;
            aux_ *= caux;
            break;
        }
        case K909: {
            if (vn_ == 0) { v->ph[0] = 0; v->ph[2] = v->ph[3] = v->ph[4] = v->ph[5] = 1; }
            float bk = ch_max(bend, 0) * (1.0f / 28);
            float fq = hz * (1 + bk * (3.2f * v->ph[2] + 1.3f * v->ph[3]));     /* ~260 Hz down to the note */
            v->ph[2] *= k0; v->ph[3] *= k1;
            float p = v->ph[0]; v->ph[0] = wrap1(p + ch_min(fq * (1.0f / (float)CH_RATE), 0.45f));
            float q = p + 0.75f; if (q >= 1) q -= 1;                            /* a triangle in sine phase */
            float drv = 1.4f + color * 3.5f;                                    /* WAVE: rounded .. driven */
            float body = ch_tanh(drv * tri(q)) * sat_n;
            float pulse = vn_ < 22 ? v->ph[4] : 0;                             /* the click: ~0.5 ms pulse */
            float burst = ch_svf_tick(&v->f[0], noise, g0, 1.2f).bp * v->ph[5]; /* and a noise burst */
            v->ph[4] *= r; v->ph[5] *= x0;
            y = body * amp_ + shape * (0.9f * pulse + 1.1f * burst);
            break;
        }
        case K606: {
            if (vn_ == 0) { v->ph[0] = 0; v->ph[2] = v->ph[4] = 1; }
            float fq = hz * (1 + ch_max(bend, 0) * (1.0f / 9) * 1.6f * v->ph[2]);
            v->ph[2] *= k0;
            float p = v->ph[0]; v->ph[0] = wrap1(p + ch_min(fq * (1.0f / (float)CH_RATE), 0.45f));
            float s = ch_sin_turns(p) + color * 0.35f * ch_sin_turns(2 * p);    /* TONE: a second harmonic */
            float cl = ch_svf_tick(&v->f[0], noise, g0, 1.2f).hp * v->ph[4];
            v->ph[4] *= r;
            y = s * amp_ + shape * (0.9f * cl + (vn_ < 15 ? 0.8f : 0));
            break;
        }
        case KDEEP: {
            if (vn_ == 0) { v->ph[0] = 0; v->ph[2] = v->ph[3] = 1; }
            float fq = hz * (1 + ch_max(bend, 0) * (1.0f / 14) * v->ph[2]);    /* an octave, gliding down */
            v->ph[2] *= k0;
            float p = v->ph[0]; v->ph[0] = wrap1(p + ch_min(fq * (1.0f / (float)CH_RATE), 0.45f));
            float g = 1 + color * 3;                                            /* WARMTH: soft saturation */
            float s = ch_tanh(ch_sin_turns(p) * g) * sat_n;
            y = s * amp_ * (1 + shape * 1.2f * v->ph[3]);                     /* PUNCH: the first ~10 ms up */
            v->ph[3] *= k1;
            break;
        }
        case KSKIN: {
            float beat = (vn_ < 3 ? 0.8f : 0) + ch_svf_tick(&v->f[0], noise, g0, 1.0f).lp * aux_;   /* the beater */
            aux_ *= caux;
            y = res_tick(v, 4, beat) * 0.6f + beat * shape * 0.3f;
            break;
        }
        case KDUB: {
            if (vn_ == 0) { v->ph[0] = 0; v->ph[2] = 1; v->ph[4] = 0; }
            float fq = hz * (1 + ch_max(bend, 0) * (1.0f / 24) * 2.5f * v->ph[2]);
            v->ph[2] *= k0;
            float p = v->ph[0]; v->ph[0] = wrap1(p + ch_min(fq * (1.0f / (float)CH_RATE), 0.45f));
            float s = ch_sin_turns(p) * amp_;
            /* the rumble: the kick, driven and smeared with a little noise, low-passed, swelling in after it */
            v->ph[4] += (1 - v->ph[4]) * 0.0025f;
            float lp = ch_svf_tick(&v->f[0], ch_tanh(s * 3 + noise * 0.25f), g0, 0.8f).lp;
            y = s + lp * v->ph[4] * aux_ * shape * 1.6f;
            aux_ *= caux;
            break;
        }
        case KFM: {
            float pc = v->ph[0], pmod = v->ph[1];
            v->ph[0] = wrap1(pc + inc); v->ph[1] = wrap1(pmod + ch_min(inc * r, 0.45f));
            float idx = color * 8 * aux_;
            y = ch_sin_turns(pc + idx * 0.159f * ch_sin_turns(pmod)) * amp_;
            aux_ *= caux;
            break;
        }
        case KHARD: {
            float s = osc_sin(&v->ph[0], inc);
            float fold = ch_sin_turns(0.25f * s * (1 + shape * 5));
            float g = 1 + color * 6, o = color * 0.3f;
            y = (ch_tanh((fold + o) * g) - sat_o) * sat_n * amp_;
            break;
        }
        case S808: {
            float b = osc_sin(&v->ph[0], inc) + 0.6f * osc_sin(&v->ph[1], ch_min(inc * 1.78f, 0.45f));
            float h = ch_svf_tick(&v->f[0], noise, g0, 1.2f).hp;
            y = b * aux_ * (1 - shape * 0.8f) * 0.7f + h * amp_ * (0.2f + shape * 0.9f);
            aux_ *= caux;
            break;
        }
        case S909: {
            if (vn_ == 0) hold_ = 1;
            float p0 = v->ph[0], p1 = v->ph[1];
            v->ph[0] = wrap1(p0 + inc); v->ph[1] = wrap1(p1 + ch_min(inc * r, 0.45f));
            float b = ch_tanh((tri(p0) + 0.7f * tri(p1)) * 1.6f * 0.6f) * sat_n;            /* the two triangles, soft-clipped */
            float lo = ch_svf_tick(&v->f[0], noise, g0, 1.4f).lp;                           /* the noise: low-passed, then NOISE TONE's high-pass */
            float hi = ch_svf_tick(&v->f[1], lo, g1, 1.0f).hp;
            if (vn_ > 441) hold_ *= k0;                                                  /* held ~10 ms before it falls */
            y = b * aux_ * 0.8f + hi * hold_ * 1.1f;
            aux_ *= caux;
            break;
        }
        case SFM: {
            float pc = v->ph[0], pmod = v->ph[1];
            v->ph[0] = wrap1(pc + inc); v->ph[1] = wrap1(pmod + ch_min(inc * r, 0.45f));
            float fm = ch_sin_turns(pc + 3 * aux_ * 0.159f * ch_sin_turns(pmod));
            float bp = ch_svf_tick(&v->f[0], noise, g0, 0.8f).bp;
            y = fm * aux_ * 0.8f + bp * amp_ * (0.3f + color * 1.0f);
            aux_ *= caux;
            break;
        }
        case CLAP: {
            float ts = (float)vn_ * (1.0f / (float)CH_RATE), sp = r;
            if (ts < 3 * sp) { float loc = ts - (float)(int)(ts / sp) * sp; e = ch_exp2(-loc * 700); }
            else e = amp_;
            float bp = ch_svf_tick(&v->f[0], noise, g0, 0.6f).bp;
            bp = ch_svf_tick(&v->f[1], bp, g1, 1.4f).hp;
            y = bp * e * 1.6f;
            if (ts >= 3 * sp) amp_ *= ca;
            break;
        }
        case RIM: case CLAVE: {
            float x = (vn_ == 0 ? 1.0f : 0.0f) + noise * color * 0.5f * aux_;
            y = res_tick(v, nm, x) * 0.9f;
            aux_ *= caux;
            break;
        }
        case COWBELL: {
            float s = sqr(&v->ph[0], inc) + sqr(&v->ph[1], ch_min(inc * r, 0.45f));
            float bp = ch_svf_tick(&v->f[0], s, g0, 0.9f).bp;
            y = bp * ch_lerp(amp_, aux_, color * 0.7f) * 0.6f;
            aux_ *= caux;
            break;
        }
        case HAT808: case CYMBAL: {
            float s = sb[i];
            s *= (1.0f / 6);
            if (v->model == HAT808) {
                float bp = ch_svf_tick(&v->f[0], s, g0, 1.2f).bp;
                y = ch_svf_tick(&v->f[1], bp, g1, 1.4f).hp * amp_ * 2.2f;
            } else {
                float lo = ch_svf_tick(&v->f[0], s + noise * color * 0.4f, g0, 0.8f).bp;
                float hi = ch_svf_tick(&v->f[1], s + noise * color * 0.6f, g1, 1.2f).hp;
                y = lo * amp_ * 0.9f + hi * aux_ * 1.2f;
                aux_ *= caux;
            }
            break;
        }
        case HATNOISE: {
            float hp = ch_svf_tick(&v->f[0], noise, g0, k0).hp;
            float bp = ch_svf_tick(&v->f[1], noise, g1, 0.5f).bp;
            y = (hp * 0.7f + bp * color * 0.8f) * amp_;
            break;
        }
        case TOM: {
            float s = osc_sin(&v->ph[0], inc);
            float sk = ch_svf_tick(&v->f[0], noise, g0, 0.8f).bp;
            y = s * amp_ + sk * color * aux_;
            aux_ *= caux;
            break;
        }
        case CONGA: {
            float slap = ch_svf_tick(&v->f[0], noise, g0, 1.2f).hp * shape * aux_;
            y = res_tick(v, nm, (vn_ == 0 ? 1.0f : 0.0f) + slap * 0.6f) + slap * 0.5f;
            aux_ *= caux;
            break;
        }
        case MEMBRANE: case BELL: {
            float x = (vn_ == 0 ? 1.0f : 0.0f) + noise * 0.25f * aux_;
            y = res_tick(v, nm, x) * (v->model == BELL ? 0.5f : 0.6f);
            aux_ *= caux;
            break;
        }
        case PLUCK: {
            float rp = (float)v->ks_w - L_ks;
            while (rp < 0) rp += CH_KS_LEN;
            unsigned a = (unsigned)rp; float fr = rp - (float)a;
            float s0 = v->ks[a & (CH_KS_LEN - 1)] * (1.0f / 32768), s1 = v->ks[(a + 1) & (CH_KS_LEN - 1)] * (1.0f / 32768);
            float s = s0 + (s1 - s0) * fr;
            float nx = 0.5f * (s + v->ks_prev) * g_ks;
            if (color > 0 && ch_randf(&v->rng) < color * 0.5f) nx = -nx;
            v->ks_prev = s;
            v->ks[v->ks_w] = (int16_t)(ch_clamp(nx, -1, 1) * 32767);
            v->ks_w = (uint16_t)((v->ks_w + 1) & (CH_KS_LEN - 1));
            y = s;
            break;
        }
        case FM2: {
            float pc = v->ph[0], pmod = v->ph[1];
            v->ph[0] = wrap1(pc + inc); v->ph[1] = wrap1(pmod + ch_min(inc * r, 0.45f));
            float idx = color * color * 14 * aux_;         /* the FM amount, falling away on its own envelope */
            float m = ch_sin_turns(pmod + v->fb * 0.06f * idx);   /* the modulator, fed back a little: the grit */
            v->fb = m;
            y = ch_sin_turns(pc + idx * 0.159f * m) * amp_;
            aux_ *= caux;
            break;
        }
        case FMPERC: {
            float pc = v->ph[0], pmod = v->ph[1];
            v->ph[0] = wrap1(pc + inc); v->ph[1] = wrap1(pmod + ch_min(inc * r, 0.45f));
            y = ch_sin_turns(pc + color * 10 * aux_ * 0.159f * ch_sin_turns(pmod)) * amp_;
            aux_ *= caux;
            break;
        }
        case SHAKER: {
            if (ch_randf(&v->rng) < x0) v->sh = ch_min(v->sh + ch_randf(&v->rng) * 0.7f, 1.5f);
            v->sh *= 0.985f;
            float bp = ch_svf_tick(&v->f[0], noise * v->sh, g0, k0).bp;
            y = bp * amp_ * (1 - aux_) * 1.5f;
            aux_ *= caux;
            break;
        }
        case ZAP: {
            float p = v->ph[0];
            v->ph[0] = wrap1(p + inc);
            y = ch_sin_turns(p + color * 0.3f * v->fb);
            v->fb = y;
            y *= amp_;
            break;
        }
        case BIT: {
            int shortmode = shape >= 0.5f;
            float rate = 0.25f + 1.5f * (shortmode ? shape - 0.5f : shape);
            v->ph[1] += ch_min(hz * rate * (1.0f / (float)CH_RATE), 1);
            if (v->ph[1] >= 1) {
                v->ph[1] -= 1;
                unsigned bit = (v->lfsr ^ (v->lfsr >> (shortmode ? 6 : 1))) & 1u;
                v->lfsr = (uint16_t)((v->lfsr >> 1) | (bit << 14));
                hold_ = (v->lfsr & 1u) ? 1.0f : -1.0f;
            }
            float sq = 0;
            { float p = v->ph[0]; v->ph[0] = wrap1(p + inc * 0.0625f); sq = p < 0.5f ? 1.0f : -1.0f; }
            y = ch_lerp(hold_, sq, color) * amp_;
            y = (float)(int)(y * 7.0f) * (1.0f / 7);
            break;
        }
        case PING: {
            float x = vn_ == 0 ? 1.0f : 0.0f;
            y = res_tick(v, 1, x) + color * 0.8f * osc_sin(&v->ph[0], inc * 0.5f) * aux_;
            aux_ *= caux;
            break;
        }
        case GLITCH: {
            v->ph[1] += x0;
            if (v->ph[1] >= 1) { v->ph[1] -= 1; v->chaos = r * v->chaos * (1 - v->chaos); hold_ = hz * (0.5f + 2 * v->chaos); }
            if (hold_ <= 0) hold_ = hz;
            float p = v->ph[0];
            v->ph[0] = wrap1(p + ch_min(hold_ * (1.0f / (float)CH_RATE), 0.45f));
            y = (p < 0.5f ? 1.0f : -1.0f) * (0.6f + 0.4f * v->chaos) * amp_;
            break;
        }
        case HAT606: {                                   /* six squares through a narrow band and a high-pass: thin and spitting */
            float s = sb[i];
            float bp = ch_svf_tick(&v->f[0], s * (1.0f / 6), g0, 0.7f).bp;
            y = ch_svf_tick(&v->f[1], bp, g1, 1.2f).hp * amp_ * 2.4f;
            break;
        }
        case HATRING: {                                  /* three ring-modulated square pairs */
            float s = 0;
            s = sb[i];
            float bp = ch_svf_tick(&v->f[0], s * (1.0f / 3), g0, 1.4f).bp;
            y = ch_svf_tick(&v->f[1], bp, g1, 1.0f).hp * amp_ * 2.0f;
            break;
        }
        case HATFM: {                                    /* a sine pair at an off ratio, the index falling, the modulator fed back */
            float pc = v->ph[0], pmod = v->ph[1];
            v->ph[0] = wrap1(pc + inc); v->ph[1] = wrap1(pmod + ch_min(inc * r, 0.45f));
            float idx = (3 + color * 12) * aux_;
            float m = ch_sin_turns(pmod + v->fb * 0.08f * idx);
            v->fb = m;
            float s = ch_sin_turns(pc + idx * 0.159f * m);
            y = ch_svf_tick(&v->f[0], s, g0, 1.0f).hp * amp_;
            aux_ *= caux;
            break;
        }
        case HATCLOCK: {                                 /* noise clocked at an audible rate: the clock rings through it */
            v->ph[1] += x0;
            if (v->ph[1] >= 1) {
                v->ph[1] -= 1;
                unsigned bit = (v->lfsr ^ (v->lfsr >> 1)) & 1u;
                v->lfsr = (uint16_t)((v->lfsr >> 1) | (bit << 14));
                hold_ = (v->lfsr & 1u) ? 1.0f : -1.0f;
            }
            float bp = ch_svf_tick(&v->f[0], hold_, g0, 0.8f).bp;
            float hp = ch_svf_tick(&v->f[1], hold_, g1, 1.0f).hp;
            y = (bp * 1.2f + hp * 0.5f) * amp_;
            break;
        }
        case HATWIRE: {                                  /* a short string loop, inverted (odd partials), fed with noise while it rings */
            float rp = (float)v->ks_w - L_ks;
            while (rp < 0) rp += CH_KS_LEN;
            unsigned a = (unsigned)rp; float fr = rp - (float)a;
            float s0 = v->ks[a & (CH_KS_LEN - 1)] * (1.0f / 32768), s1 = v->ks[(a + 1) & (CH_KS_LEN - 1)] * (1.0f / 32768);
            float s = s0 + (s1 - s0) * fr;
            float lpf = ch_lerp(0.5f * (s + v->ks_prev), s, shape);           /* BRIGHT: how little the loop dulls each pass */
            float nx = -lpf * g_ks + noise * aux_ * 0.6f;
            v->ks_prev = s;
            v->ks[v->ks_w] = (int16_t)(ch_clamp(nx, -1, 1) * 32767);
            v->ks_w = (uint16_t)((v->ks_w + 1) & (CH_KS_LEN - 1));
            y = ch_svf_tick(&v->f[0], s, g0, 1.0f).hp * 1.4f;
            aux_ *= caux;
            break;
        }
        case CRASH: {                                    /* a burst into six shimmering modes under a wash that falls away first */
            if (vn_ == 0) hold_ = 1;
            float x = noise * (0.12f * aux_ + 0.006f * amp_);
            float sh = res_tick(v, 6, x);
            float wash = ch_svf_tick(&v->f[0], noise, g0, 1.0f).hp * hold_;
            y = sh * 0.9f + wash * (0.15f + color * 0.6f) + noise * aux_ * 0.5f;
            aux_ *= caux; hold_ *= k0;
            break;
        }
        case RIDE: {                                     /* a ping and its bell, the stick's sizzle over it */
            float x = (vn_ == 0 ? 1.0f : 0.0f) + noise * 0.04f * aux_;
            float bell = res_tick(v, 4, x);
            float sz = ch_svf_tick(&v->f[0], noise, g0, 1.2f).hp * aux_;
            y = bell * 0.8f + sz * (0.1f + color * 0.8f);
            aux_ *= caux;
            break;
        }
        case SMODAL: {                                   /* five modes struck by a 1 ms high-passed pulse; the snares: a band of noise at 16 f0 */
            if (vn_ == 0) hold_ = 1;
            float pulse = vn_ < 44 ? 1.0f - (float)vn_ * (1.0f / 44) : 0.0f;
            float x = ch_svf_tick(&v->f[0], pulse, g0, 1.0f).hp * 1.5f;
            float body = res_tick(v, 5, x);
            float sn = ch_svf_tick(&v->f[1], noise, g1, 0.9f).bp;
            y = body * 0.9f + sn * hold_ * (0.2f + color * 1.2f);
            hold_ *= k0;
            break;
        }
        case SSKIN: {                                    /* the head, struck; the wires rattle with the body and fall on their own */
            if (vn_ == 0) hold_ = 1;
            float beat = (vn_ < 3 ? 0.8f : 0) + ch_svf_tick(&v->f[0], noise, g0, 1.0f).lp * aux_;
            float body = res_tick(v, 4, beat);
            float wires = ch_svf_tick(&v->f[1], noise, g1, 0.8f).bp;
            y = body * 0.6f + beat * 0.25f + wires * color * (0.5f * hold_ + 2.5f * ch_abs(body));
            aux_ *= caux; hold_ *= k0;
            break;
        }
        case SLOFI: {                                    /* a snare sampled badly: held at RATE, cut to BITS */
            float p = v->ph[0]; v->ph[0] = wrap1(p + inc);
            float pre = ch_sin_turns(p) * aux_ * 0.9f + ch_svf_tick(&v->f[0], noise, g0, 1.0f).hp * amp_ * 0.8f;
            v->ph[1] += x0;
            if (v->ph[1] >= 1) { v->ph[1] -= 1; hold_ = pre; }
            y = (float)(int)(hold_ * r + (hold_ < 0 ? -0.5f : 0.5f)) / r;
            aux_ *= caux;
            break;
        }
        }
        /* common: SNAP click, the NOISE layer, envelopes */
        if (has_snap) y += noise * snap_ * psnap;
        if (has_noise) y += pnoise * ch_svf_tick(&v->f[3], noise, nb_g, 1.0f).bp * aux2_;
        if (not_clap) amp_ *= ca;
        pit_ *= cp; snap_ *= csnap; aux2_ *= cn;
        yb[i] = y;
        ++vn_;
    }
    v->amp = amp_; v->pit = pit_; v->snap = snap_; v->aux2 = aux2_; v->aux = aux_; v->hold = hold_; v->n = vn_;
    {   /* TONE, DRIVE, level */
        const int tmode = P->tone < -0.02f ? 1 : P->tone > 0.02f ? 2 : 0, driven = P->drive > 0.005f;
        const float dg = 1 + P->drive * P->drive * 20, dm = ch_min(P->drive * 4, 1);
        ch_svf_t tf = v->f[2];
        float fade = v->fade, dcp = v->dcp, last = v->last; const float dc = v->dc;
        if (tmode == 0 && !driven && fstep == 0 && dcp <= 0) {   /* the usual case: a gain, into the sum */
            const float g2 = gain * ch_fade_gain(fade);
            for (unsigned i = 0; i < n; ++i) {
                float y = yb[i] * g2;
                out[i] += y;
                pk = ch_max(pk, ch_abs(y));
            }
            if (n) last = yb[n - 1] * g2;
        } else
        for (unsigned i = 0; i < n; ++i) {
            float y = yb[i];
            if (tmode == 1) y = ch_svf_tick(&tf, y, tg, 1.4f).lp;
            else if (tmode == 2) y = ch_svf_tick(&tf, y, tg, 1.4f).hp;
            if (driven) y = ch_lerp(y, ch_tanh(y * dg) * 0.8f, dm);
            y *= gain * ch_fade_gain(fade);
            fade = ch_max(fade - fstep, 0);
            if (dcp > 0) { y += dc * ch_fade_gain(dcp); dcp -= 1.0f / (0.003f * (float)CH_RATE); }
            out[i] += y;
            last = y;
            pk = ch_max(pk, ch_abs(y));
        }
        v->f[2] = tf; v->fade = fade; v->dcp = dcp; v->last = last;
    }
    if (v->kill && v->fade <= 0) v->on = 0;
    /* the decays that never reach zero in single precision, flushed once a block (a denormal is slow on some FPUs) */
    v->amp = ch_ftz(v->amp); v->snap = ch_ftz(v->snap); v->aux = ch_ftz(v->aux); v->aux2 = ch_ftz(v->aux2); v->pit = ch_ftz(v->pit);
    v->hold = ch_ftz(v->hold); v->sh = ch_ftz(v->sh);
    for (unsigned k = 2; k < 6; ++k) v->ph[k] = ch_ftz(v->ph[k]);
    for (unsigned k = 0; k < CH_DRUM_MODES; ++k) { v->ry1[k] = ch_ftz(v->ry1[k]); v->ry2[k] = ch_ftz(v->ry2[k]); }
    if (v->n > 2205 && pk < 2e-5f && v->amp < 1e-3f) v->on = 0;
    /* A tail under -66 dB for 23 ms is let go (over 8 ms, on the stopped voice's curve). It used to ring on to -94 dB at
     * a full voice's cost: a third of a decaying drum's life spent where no one hears it, and on the FM-1 the voices
     * sounding at once are what a block's time goes to */
    if (!v->kill && v->n > 2205 && pk < CH_TAIL_END && v->amp < 0.02f) {   /* (its envelope down too: quiet for now is not over) */ if (++v->quiet >= CH_TAIL_SUBS) v->kill = 1; } else v->quiet = 0;
    if (v->n > (uint32_t)CH_RATE * 20) v->on = 0;
}

/* The last eight models (SNARE BAND .. RIDE 909), in a routine of their own. They were eight more cases in voice_run's
 * sample loop, and on the FM-1 that cost every drum: the loop is one function, and each case added to it is more values
 * for the compiler to keep in hand through all of it (more of them on the stack, read and written each sample), and
 * 9 KB more code for the instruction cache to hold. Here each model has its own short loop with only its own values,
 * its running state in locals, and nothing a sample that a block can work out (no division, no exponential); the old
 * routine is again what it was. SNAP, the NOISE layer and the shared envelopes are a second short loop; TONE, DRIVE,
 * the level and the voice's end are voice_run_a's, word for word */
static CH_NOINLINE void voice_run_b(ch_dvoice *v, const dparams *P, float *out, unsigned n)
{
    const model_t *m = &M[v->model];
    const float semis = P->pitch + (float)((int)v->note - 60) + v->var_p;
    const float hz = ch_clamp(m->hz * ch_semis(semis), 8, 18000);
    const float dec = m->decay * P->dec * v->var_d;
    const float shape = ch_clamp(P->shape + v->var_s, 0, 1), color = P->color;
    const float bend = m->bend + P->bend * 36;
    const float ca = dcoef(dec);
    const float irate = 1.0f / (float)CH_RATE;
    const float inc0 = ch_min(hz * irate, 0.45f);
    float bt = m->bend_t;
    float yb[CH_SUB], sb[CH_SUB];
    float amp_ = v->amp, aux_ = v->aux;
    const uint32_t vn = v->n;
    if (v->model == NOISEP) bt = 0.004f + shape * 0.2f;
    const float cp = dcoef(bt);
    switch (v->model) {
    case SBAND: {                                        /* a body (two sines, a fifth and a bit apart) and the noise through its band */
        const float fc = ch_exp_map(shape, 400, 9000), g = ch_svf_g(fc), k = ch_exp_map(color, 0.12f, 2.0f);
        /* (a band of noise holds less the lower it sits, at one WIDTH: made up, so BAND moves the tone and not the level) */
        const float mk = ch_sqrt_fast(k * ch_clamp(2500 / fc, 0.3f, 4)) * 0.9f, caux = dcoef(dec * 0.5f);
        float p0 = v->ph[0], p1 = v->ph[1], pit = v->pit, inc = inc0, inc1 = ch_min(inc0 * 1.62f, 0.45f);
        ch_svf_t f = v->f[0]; ch_rng_t rg = v->rng;
        for (unsigned i = 0; i < n; ++i) {
            if (bend != 0 && (i & 3u) == 0) { inc = ch_min(hz * ch_semis(bend * pit) * irate, 0.45f); inc1 = ch_min(inc * 1.62f, 0.45f); }
            float b = ch_sin_turns(p0) + 0.5f * ch_sin_turns(p1);
            p0 += inc; if (p0 >= 1) p0 -= 1;
            p1 += inc1; if (p1 >= 1) p1 -= 1;
            yb[i] = b * aux_ * 0.55f + ch_svf_tick(&f, ch_rands(&rg), g, k).bp * mk * amp_;
            aux_ *= caux; amp_ *= ca; pit *= cp;
        }
        v->ph[0] = p0; v->ph[1] = p1; v->f[0] = f; v->rng = rg;
        break;
    }
    case CLAPBAND: {                                     /* the clap's three bursts (11 ms apart) and tail, through the band */
        const float fc = ch_clamp(ch_exp_map(shape, 500, 5000) * hz * (1.0f / 1100), 200, 12000), g = ch_svf_g(fc), k = ch_exp_map(color, 0.2f, 2.0f);
        const float mk = ch_sqrt_fast(k * ch_clamp(1500 / fc, 0.25f, 5)) * 1.3f;
        const uint32_t sp = (uint32_t)(0.011f * (float)CH_RATE);
        const float ce = ch_exp2(-700.0f * irate);       /* a burst's fall, a sample's worth */
        uint32_t t = vn, loc = t < 3 * sp ? t % sp : 0;
        float e = ch_exp2(-700.0f * irate * (float)loc);
        ch_svf_t f = v->f[0]; ch_rng_t rg = v->rng;
        for (unsigned i = 0; i < n; ++i, ++t) {
            float x = ch_svf_tick(&f, ch_rands(&rg), g, k).bp * mk;
            if (t < 3 * sp) { yb[i] = x * e; e *= ce; if (++loc >= sp) { loc = 0; e = 1; } }
            else { yb[i] = x * amp_; amp_ *= ca; }
        }
        v->f[0] = f; v->rng = rg;
        break;
    }
    case NOISEP: {                                       /* noise through a band that rides the pitch envelope (a block at a time), SWEEP its time */
        const float fc = ch_clamp(hz * ch_semis(bend * v->pit), 40, 16000), g = ch_svf_g(fc), k = 2.0f - color * 1.9f;
        const float mk = ch_sqrt_fast(k * ch_clamp(2000 / fc, 0.25f, 6));
        ch_svf_t f = v->f[0]; ch_rng_t rg = v->rng;
        for (unsigned i = 0; i < n; ++i) { yb[i] = ch_svf_tick(&f, ch_rands(&rg), g, k).bp * mk * amp_; amp_ *= ca; }
        v->f[0] = f; v->rng = rg;
        break;
    }
    case KCLICK: {                                       /* a sine and its dive; the click: a band of noise on its own short envelope */
        const float g = ch_svf_g(ch_exp_map(shape, 800, 9000)), cc = dcoef(0.001f + color * 0.02f);
        float p0 = v->ph[0], ck = vn == 0 ? 1.0f : v->ph[5], pit = v->pit, inc = inc0;
        ch_svf_t f = v->f[0]; ch_rng_t rg = v->rng;
        for (unsigned i = 0; i < n; ++i) {
            if (bend != 0 && (i & 3u) == 0) inc = ch_min(hz * ch_semis(bend * pit) * irate, 0.45f);
            float y = ch_sin_turns(p0) * amp_;
            p0 += inc; if (p0 >= 1) p0 -= 1;
            if (ck > 1e-4f) { y += ch_svf_tick(&f, ch_rands(&rg), g, 0.9f).bp * ck * 1.3f; ck *= cc; }   /* (the click over: its noise and band no longer run) */
            yb[i] = y;
            amp_ *= ca; pit *= cp;
        }
        v->ph[0] = p0; v->ph[5] = ck; v->f[0] = f; v->rng = rg;
        break;
    }
    case METALFM: {                                      /* three carriers spread apart, one modulator at an off ratio into all of them (each
                                                          * a different depth, so they do not move as one), the index falling */
        const float sp = 0.3f + color * 1.4f, caux = dcoef(dec * 0.45f);
        const float i1 = ch_min(inc0 * (1 + 0.47f * sp), 0.45f), i2 = ch_min(inc0 * (1 + 1.09f * sp), 0.45f), im = ch_min(inc0 * ch_exp_map(shape, 1.21f, 7.3f), 0.45f);
        float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], pm = v->ph[3];
        for (unsigned i = 0; i < n; ++i) {
            float md = (1.5f + 4.5f * aux_) * 0.159f * ch_sin_turns(pm);
            yb[i] = (ch_sin_turns(p0 + md) + ch_sin_turns(p1 + md * 1.7f) + ch_sin_turns(p2 - md * 2.3f)) * (1.0f / 3) * amp_;
            p0 += inc0; if (p0 >= 1) p0 -= 1;
            p1 += i1; if (p1 >= 1) p1 -= 1;
            p2 += i2; if (p2 >= 1) p2 -= 1;
            pm += im; if (pm >= 1) pm -= 1;
            aux_ *= caux; amp_ *= ca;
        }
        v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->ph[3] = pm;
        break;
    }
    case IMPULSE: {                                      /* a strike (a sample wide .. 1.4 ms, HARD) into a comb: the loop dulled by DAMP */
        const float x0 = 1 + (1 - shape) * (1 - shape) * 60, ix0 = 1 / x0, hg = 1.8f / ch_sqrt_fast(x0);
        const float L_ks = ch_clamp((float)CH_RATE / hz, 4, CH_KS_LEN - 4), g_ks = ch_exp2(-9.966f * L_ks / (dec * (float)CH_RATE));
        float prev = v->ks_prev; unsigned w = v->ks_w; uint32_t t = vn;
        for (unsigned i = 0; i < n; ++i, ++t) {
            float rp = (float)w - L_ks;
            if (rp < 0) rp += CH_KS_LEN;
            unsigned a = (unsigned)rp; float fr = rp - (float)a;
            float s0 = v->ks[a & (CH_KS_LEN - 1)] * (1.0f / 32768), s1 = v->ks[(a + 1) & (CH_KS_LEN - 1)] * (1.0f / 32768);
            float s = s0 + (s1 - s0) * fr;
            float hit = (float)t < x0 ? (1 - (float)t * ix0) * hg : 0;
            float nx = ch_lerp(s, 0.5f * (s + prev), color) * g_ks + hit;
            prev = s;
            v->ks[w] = (int16_t)(ch_clamp(nx, -1, 1) * 32767);
            w = (w + 1) & (CH_KS_LEN - 1);
            yb[i] = s + hit;
            amp_ *= ca;
        }
        v->ks_prev = prev; v->ks_w = (uint16_t)w;
        break;
    }
    case RIDE808: {                                      /* the 808's six squares through the cymbal's two bands (3.44 and 7.1 kHz): the low
                                                          * one rings, the high one falls first */
        const float st = (shape - 0.5f) * 0.1f, caux = dcoef(dec * 0.45f), g0 = ch_svf_g(3440), g1 = ch_svf_g(7100);
        const float wl = (1.3f - color * 0.9f) * (1.0f / 6), wh = (0.5f + color * 1.3f) * (1.0f / 6);
        for (unsigned i = 0; i < n; ++i) sb[i] = 0;
        for (unsigned k = 0; k < 6; ++k) {
            float ph = v->ph[k]; const float hi = ch_min(inc0 * METAL[k] * (1 + st * (float)k), 0.45f), hii = 1 / hi;
            for (unsigned i = 0; i < n; ++i) sb[i] += sqr_i(&ph, hi, hii);
            v->ph[k] = ph;
        }
        ch_svf_t f0 = v->f[0], f1 = v->f[1];
        for (unsigned i = 0; i < n; ++i) {
            float s = sb[i];
            float lo = ch_svf_tick(&f0, s, g0, 0.7f).bp, hi = ch_svf_tick(&f1, s, g1, 0.9f).bp;
            yb[i] = lo * amp_ * wl + hi * (0.35f * amp_ + aux_) * wh;
            aux_ *= caux; amp_ *= ca;
        }
        v->f[0] = f0; v->f[1] = f1;
        break;
    }
    case CYMBAL: case CRASH: case RIDE: {                /* The cymbals. CRASH and RIDE were banks of four to six ringing modes, and a few
                                                          * sines ringing is a mallet on a bar: a cymbal has hundreds, too close to hear
                                                          * apart. So: noise, and under it three pairs of squares multiplied together (each
                                                          * pair a cloud of sums and differences, none a note; plain squares, their folding
                                                          * over only more of the same), high-passed, then a low-pass that closes as the hit
                                                          * dies (the top of a cymbal goes first). A burst of noise for the stick, and a
                                                          * level that falls fast and then rings. RIDE's BELL: two modes, only as asked for */
        const int ride = v->model == RIDE, crash = v->model == CRASH;
        const float base = hz * irate * (ride ? 0.26f : crash ? 0.2222f : 0.6667f);
        const float hi = ride ? 0.72f : 0.6f + shape * 0.8f;
        const float wn = ride ? 0.10f + 0.7f * color : crash ? 0.25f + 0.9f * color : 0.15f + 0.8f * color;
        const float wm = (ride ? 0.8f : crash ? 1.1f - 0.7f * color : 1.0f - 0.5f * color) * (1.0f / 3);
        const float burst = ride ? 1.6f : crash ? 0.9f : 0.6f, e0 = ride ? 0.35f : crash ? 0.5f : 0.6f;
        const float caux = dcoef(ride ? 0.004f : dec * 0.08f), chold = dcoef(ride ? 0.06f : dec * (crash ? 0.25f : 0.3f));
        const float ghp = ch_svf_g(ch_clamp((ride ? 4.5f : crash ? 3.1f : 18.0f) * hz, 800, 10000));
        const float glp = ch_svf_g(ch_exp_map(ch_sqrt_fast(ch_clamp(amp_, 1e-6f, 1)), ride ? 7000 : 5000, 17000));
        for (unsigned i = 0; i < n; ++i) sb[i] = 0;
        for (unsigned k = 0; k < 3; ++k) {
            float pa = v->ph[k], pb = v->ph[k + 3];
            const float ia = ch_clamp(base * RINGR[k], 1e-4f, 0.49f), ib = ch_clamp(base * RINGR[k + 3] * hi, 1e-4f, 0.49f);
            for (unsigned i = 0; i < n; ++i) {
                float a = pa < 0.5f ? 1.0f : -1.0f;
                sb[i] += pb < 0.5f ? a : -a;
                pa += ia; if (pa >= 1) pa -= 1;
                pb += ib; if (pb >= 1) pb -= 1;
            }
            v->ph[k] = pa; v->ph[k + 3] = pb;
        }
        float hold = vn == 0 ? 1.0f : v->hold;
        const int bell = ride && shape > 0.03f;
        if (vn == 0 && ride) { res_set(v, 0, hz, dec * 0.6f, shape * shape * 0.5f); res_set(v, 1, hz * 1.52f, dec * 0.4f, shape * shape * 0.3f); }
        ch_svf_t f0 = v->f[0], f1 = v->f[1]; ch_rng_t rg = v->rng;
        for (unsigned i = 0; i < n; ++i) {
            float x = sb[i] * wm + ch_rands(&rg) * wn * (1 + burst * aux_);
            float h = ch_svf_tick(&f0, x, ghp, 1.0f).hp;
            yb[i] = ch_svf_tick(&f1, h, glp, 1.3f).lp * amp_ * (e0 + (1 - e0) * hold);
            aux_ *= caux; hold *= chold; amp_ *= ca;
        }
        v->f[0] = f0; v->f[1] = f1; v->rng = rg; v->hold = hold;
        if (bell) { for (unsigned i = 0; i < n; ++i) yb[i] += res_tick(v, 2, vn + i == 0 ? 1.0f : 0.0f); }
        break;
    }
    default: {                                           /* RIDE909: ring-modulated square pairs, high-passed, over a ping (two modes, 3.3 and
                                                          * 4.9 kHz at the model's pitch); then the recording: held at a low rate, few bits */
        const float g = ch_svf_g(6000), step = ch_exp_map(color, 1.0f, 0.2f), q = ch_exp_map(1 - color, 24, 4096), iq = 1 / q, bl = shape * 1.6f;
        if (vn == 0) {
            const float f = hz * (bend != 0 ? ch_semis(bend * v->pit) : 1);
            res_set(v, 0, f * 10.2f, dec * 0.5f, 0.5f); res_set(v, 1, f * 15.3f, dec * 0.35f, 0.35f);
        }
        for (unsigned i = 0; i < n; ++i) sb[i] = 0;
        /* (plain squares, as the cymbals': the pairs were band-limited ones, six of them a sample each with their
         * edges worked out, most of this model's cost; what they fold over is lost in the hold and the few bits that follow) */
        for (unsigned k = 0; k < 3; ++k) {
            float pa = v->ph[k], pb = v->ph[k + 3];
            const float ia = ch_clamp(hz * irate * RINGR[k], 1e-4f, 0.49f), ib = ch_clamp(hz * irate * RINGR[k + 3] * 0.72f, 1e-4f, 0.49f);
            for (unsigned i = 0; i < n; ++i) {
                float a = pa < 0.5f ? 1.0f : -1.0f;
                sb[i] += pb < 0.5f ? a : -a;
                pa += ia; if (pa >= 1) pa -= 1;
                pb += ib; if (pb >= 1) pb -= 1;
            }
            v->ph[k] = pa; v->ph[k + 3] = pb;
        }
        ch_svf_t f = v->f[0]; float sh = v->sh, hold = v->hold;
        float a = v->ry1[0], b = v->ry2[0], c = v->ry1[1], d = v->ry2[1];
        const float c0 = v->rb1[0], s0 = v->rb2[0], c1 = v->rb1[1], s1 = v->rb2[1];
        if (vn == 0) { a += v->rg[0]; c += v->rg[1]; }   /* the strike, into both modes */
        /* The hold and the few bits are taken of the sound at full level, and the envelope comes after them. They came
         * after it, and a long DECAY then spent seconds a step or two tall: the tail sputtered on and off between two
         * levels and cut out early. (The 909 did it this way round: the recording played whole, its level an analogue
         * envelope after the converter.) The ping has its own, faster fall, so it is brought up by what the envelope
         * will take off again: once a block, not a division a sample */
        const float iamp = 1 / ch_max(amp_, 1e-3f);
        for (unsigned i = 0; i < n; ++i) {
            float hp = ch_svf_tick(&f, sb[i] * (1.0f / 3), g, 1.0f).hp;
            float na = c0 * a - s0 * b, nc = c1 * c - s1 * d;
            b = s0 * a + c0 * b; d = s1 * c + c1 * d; a = na; c = nc;
            float pre = hp * 1.4f + (b + d) * bl * iamp;
            sh += step;
            if (sh >= 1) { sh -= 1; hold = pre; }
            yb[i] = (float)(int)(hold * q + (hold < 0 ? -0.5f : 0.5f)) * iq * amp_;
            amp_ *= ca;
        }
        v->f[0] = f; v->sh = sh; v->hold = hold;
        v->ry1[0] = a; v->ry2[0] = b; v->ry1[1] = c; v->ry2[1] = d;
        break;
    }
    }
    v->amp = amp_; v->aux = aux_; v->n = vn + n;
    {   /* SNAP's click and the NOISE layer over the model, and the envelopes every model shares */
        const float csnap = dcoef(0.006f), cn = dcoef(dec * 0.5f), psnap = P->snap * 0.9f, pnoise = P->noise * 0.8f;
        const float nb_g = ch_svf_g(ch_clamp(hz * 6, 300, 12000));
        float snap_ = v->snap, aux2_ = v->aux2, pit_ = v->pit;
        if (P->snap > 0 || P->noise > 0.001f) {
            ch_svf_t f = v->f[3]; ch_rng_t rg = v->rng;
            for (unsigned i = 0; i < n; ++i) {
                float noise = ch_rands(&rg);
                yb[i] += noise * snap_ * psnap + pnoise * ch_svf_tick(&f, noise, nb_g, 1.0f).bp * aux2_;
                snap_ *= csnap; aux2_ *= cn; pit_ *= cp;
            }
            v->f[3] = f; v->rng = rg;
        } else for (unsigned i = 0; i < n; ++i) { snap_ *= csnap; aux2_ *= cn; pit_ *= cp; }
        v->snap = snap_; v->aux2 = aux2_; v->pit = pit_;
    }
    float pk = 0;
    {   /* TONE, DRIVE, level */
        const float vg = 1 - P->vels * (1 - v->vel * v->vel), gain = m->gain * P->lvl * vg;
        const float tg = P->tone < -0.02f ? ch_svf_g(ch_exp_map(1 + P->tone, 150, 20000)) : P->tone > 0.02f ? ch_svf_g(ch_exp_map(P->tone, 20, 4000)) : 0;
        const float fstep = v->kill ? CH_KILL_STEP : 0;
        const int tmode = P->tone < -0.02f ? 1 : P->tone > 0.02f ? 2 : 0, driven = P->drive > 0.005f;
        const float dg = 1 + P->drive * P->drive * 20, dm = ch_min(P->drive * 4, 1);
        ch_svf_t tf = v->f[2];
        float fade = v->fade, dcp = v->dcp, last = v->last; const float dc = v->dc;
        if (tmode == 0 && !driven && fstep == 0 && dcp <= 0) {   /* the usual case: a gain, into the sum */
            const float g2 = gain * ch_fade_gain(fade);
            for (unsigned i = 0; i < n; ++i) {
                float y = yb[i] * g2;
                out[i] += y;
                pk = ch_max(pk, ch_abs(y));
            }
            if (n) last = yb[n - 1] * g2;
        } else
        for (unsigned i = 0; i < n; ++i) {
            float y = yb[i];
            if (tmode == 1) y = ch_svf_tick(&tf, y, tg, 1.4f).lp;
            else if (tmode == 2) y = ch_svf_tick(&tf, y, tg, 1.4f).hp;
            if (driven) y = ch_lerp(y, ch_tanh(y * dg) * 0.8f, dm);
            y *= gain * ch_fade_gain(fade);
            fade = ch_max(fade - fstep, 0);
            if (dcp > 0) { y += dc * ch_fade_gain(dcp); dcp -= 1.0f / (0.003f * (float)CH_RATE); }
            out[i] += y;
            last = y;
            pk = ch_max(pk, ch_abs(y));
        }
        v->f[2] = tf; v->fade = fade; v->dcp = dcp; v->last = last;
    }
    if (v->kill && v->fade <= 0) v->on = 0;
    v->amp = ch_ftz(v->amp); v->snap = ch_ftz(v->snap); v->aux = ch_ftz(v->aux); v->aux2 = ch_ftz(v->aux2); v->pit = ch_ftz(v->pit);
    v->hold = ch_ftz(v->hold); v->sh = ch_ftz(v->sh);
    for (unsigned k = 2; k < 6; ++k) v->ph[k] = ch_ftz(v->ph[k]);
    for (unsigned k = 0; k < 2; ++k) { v->ry1[k] = ch_ftz(v->ry1[k]); v->ry2[k] = ch_ftz(v->ry2[k]); }
    if (v->n > 2205 && pk < 2e-5f && v->amp < 1e-3f) v->on = 0;
    if (!v->kill && v->n > 2205 && pk < CH_TAIL_END && v->amp < 0.02f) { if (++v->quiet >= CH_TAIL_SUBS) v->kill = 1; } else v->quiet = 0;
    if (v->n > (uint32_t)CH_RATE * 20) v->on = 0;
}
/* a voice's sub-block: the routine its model is in */
static void voice_run(ch_dvoice *v, const dparams *P, float *out, unsigned n)
{
    if (v->model >= SBAND || (v->model >= CYMBAL && v->model <= RIDE)) voice_run_b(v, P, out, n); else voice_run_a(v, P, out, n);   /* (the cymbals too: remade there, their old cases left in voice_run_a unreached, so that routine stays the one that was proven) */
}

CH_AUDIO_CODE void ch_drum_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n)
{
    (void)ti;
    ch_drum *d = &t->drum;
    if (t->device[CH_MATERIAL] == CH_DEV_KIT) {          /* KIT: each voice its part's settings */
        const ch_device_desc *dd = ch_device_info(CH_DEV_DRUM);
        dparams B;                                        /* what the parts share: the DRUM's defaults */
        B.chroma = 1;
        B.shape = dd->param[D_SHAPE].def; B.color = dd->param[D_COLOR].def; B.tone = dd->param[D_TONE].def;
        B.snap = ch_sq(dd->param[D_SNAP].def); B.bend = dd->param[D_BEND].def; B.noise = ch_sq(dd->param[D_NOISE].def);
        B.drive = dd->param[D_DRIVE].def; B.var = 0; B.vels = dd->param[D_VEL].def;
        float buf[CH_SUB];
        for (unsigned i = 0; i < n; ++i) buf[i] = 0;
        for (unsigned k = 0; k < CH_DVOICES; ++k) {
            ch_dvoice *v = &d->v[k];
            if (!v->on) continue;
            unsigned pt = v->part < CH_KIT_PARTS ? v->part : 0;
            dparams P = B;
            P.pitch = PV(t, CH_MATERIAL, 2 * pt) * 24;
            if (PMODE(t, CH_DEV_KIT, 2 * pt) == 0) P.pitch = (float)(int)(P.pitch + (P.pitch < 0 ? -0.5f : 0.5f));
            P.dec = ch_exp2(PV(t, CH_MATERIAL, 2 * pt + 1) * 6 - 3);
            P.lvl = ch_level_gain(t->base[CH_KIT_XROW][8 + pt]);
            voice_run(v, &P, buf, n);
        }
        for (unsigned i = 0; i < n; ++i) { L[i] += buf[i]; R[i] += buf[i]; }
        d->flash *= 0.9f;
        return;
    }
    dparams P;
    P.chroma = PMODE(t, CH_DEV_DRUM, D_PITCH) == 0;
    P.pitch = PV(t, CH_MATERIAL, D_PITCH) * 24;
    if (P.chroma) P.pitch = (float)(int)(P.pitch + (P.pitch < 0 ? -0.5f : 0.5f));
    P.dec = ch_exp2(PV(t, CH_MATERIAL, D_DECAY) * 6 - 3);
    P.shape = PV(t, CH_MATERIAL, D_SHAPE); P.color = PV(t, CH_MATERIAL, D_COLOR);
    P.tone = PV(t, CH_MATERIAL, D_TONE); P.snap = ch_sq(PV(t, CH_MATERIAL, D_SNAP));
    P.bend = PV(t, CH_MATERIAL, D_BEND); P.noise = ch_sq(PV(t, CH_MATERIAL, D_NOISE));
    P.drive = PV(t, CH_MATERIAL, D_DRIVE); P.var = PV(t, CH_MATERIAL, D_VARI);
    P.lvl = ch_level_gain(PV(t, CH_MATERIAL, D_LEVEL)); P.vels = PV(t, CH_MATERIAL, D_VEL);
    float buf[CH_SUB];
    for (unsigned i = 0; i < n; ++i) buf[i] = 0;
    for (unsigned k = 0; k < CH_DVOICES; ++k) if (d->v[k].on) voice_run(&d->v[k], &P, buf, n);
    for (unsigned i = 0; i < n; ++i) { L[i] += buf[i]; R[i] += buf[i]; }
    d->flash *= 0.9f;
}
