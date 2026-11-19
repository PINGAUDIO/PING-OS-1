/* Modulators: LFO (a wave, or STEPS / DRIFT random, by its SHAPE), TRIG RND (a random value drawn on each note,
 * held or glided to); FOLLOW and ADSR kept for stored projects.
 * Evaluated once per sub-block.
 * Each produces a stereo pair; SPREAD offsets the right channel's phase. */
#include "ch_internal.h"

void ch_mods_init(ch_mod *m, unsigned type, uint32_t seed)
{
    memset(m, 0, sizeof *m);
    m->type = (uint8_t)type;
    m->rng.s = seed ? seed : 1;
    for (unsigned i = 0; i < CH_MOD_PARAMS; ++i) m->p[i] = ch_mod_param_info(type, i)->def;
    if (type == CH_MOD_WAVE || type == CH_MOD_RANDOM) m->m[0] = 1;   /* RATE: STRAIGHT */
    for (unsigned i = 0; i < 16; ++i) m->steps[i] = ch_rands(&m->rng);
    m->last_step = -1;
}

CH_AUDIO_CODE void ch_mod_note(ch_trk *t, int on)
{
    for (unsigned i = 0; i < CH_MODS; ++i) {
        ch_mod *m = &t->mod[i];
        if (m->type == CH_MOD_TRIG) {                    /* a note: a new value, CHANCE deciding whether this one draws */
            if (on && (m->p[0] >= 0.999f || ch_randf(&m->rng) < m->p[0])) {
                m->rnd_l = ch_rands(&m->rng);
                m->rnd_r = ch_lerp(m->rnd_l, ch_rands(&m->rng), m->p[4]);   /* SPREAD: the right side its own draw */
            }
            continue;
        }
        if (m->type != CH_MOD_ADSR) continue;
        if (on) { m->gate = 1; m->stage = 1; m->from = m->level; }
        else if (!t->notes_held) { m->gate = 0; m->stage = 4; m->from = m->level; }
    }
}

/* Rate in cycles per frame: FREE 0.01..50 Hz, synced divisions. */
static float rate_cycles(const ch_mod *m)
{
    if (m->m[0] == 0) return ch_exp_map(m->p[0], 0.01f, 50.0f) / CH_RATE;
    return ch_t.beats_per_frame / ch_division_beats(m->p[0], m->m[0]);
}
float ch_mod_rate_hz(unsigned track, unsigned mod)
{
    if (track >= CH_TRACKS || mod >= CH_MODS) return 0;
    return rate_cycles(&ch_tr[track].mod[mod]) * (float)CH_RATE;
}
static float synced_phase(const ch_mod *m)
{
    return (float)(ch_t.beat / ch_division_beats(m->p[0], m->m[0]) - (ch_tf)(int32_t)(ch_t.beat / ch_division_beats(m->p[0], m->m[0])));
}

static float wave_at(const ch_mod *m, unsigned shape, float ph)
{
    ph -= (float)(int)ph; if (ph < 0) ph += 1;
    /* SKEW warps the phase: at the ends the cycle becomes a ramp, an envelope-like loop */
    float k = 0.5f + 0.49f * m->p[5];
    float q = ph < k ? 0.5f * ph / k : 0.5f + 0.5f * (ph - k) / (1 - k);
    switch (shape) {
    case CH_LFO_TRI: return q < 0.5f ? 4 * q - 1 : 3 - 4 * q;
    case CH_LFO_SAWUP: return 2 * q - 1;
    case CH_LFO_SAWDN: return 1 - 2 * q;
    case CH_LFO_SQUARE: return q < 0.5f ? 1.0f : -1.0f;
    default: return ch_sin_turns(q - 0.25f);            /* starts at the bottom, like an envelope */
    }
}

static void finish(ch_mod *m, float l, float r)
{
    /* AMOUNT and OFFSET shape the output; clamp, never wrap */
    m->out_l = ch_clamp(l * m->p[1] + m->p[3], -1, 1);
    m->out_r = ch_clamp(r * m->p[1] + m->p[3], -1, 1);
    m->uni_l = m->out_l * 0.5f + 0.5f;
    m->uni_r = m->out_r * 0.5f + 0.5f;
}

/* SMOOTH on a wave or a drift: a slew of 2 ms .. 1 s (rnd_l / rnd_r hold the slewed values) */
static void slew(ch_mod *m, float *l, float *r, unsigned n)
{
    float sm = m->p[6];
    if (sm <= 0.001f) { m->rnd_l = *l; m->rnd_r = *r; return; }
    float coef = 1 - ch_exp2(-1.442695f * (float)n / (ch_exp_map(sm, 0.002f, 1.0f) * (float)CH_RATE));
    m->rnd_l += (*l - m->rnd_l) * coef; m->rnd_r += (*r - m->rnd_r) * coef;
    *l = m->rnd_l; *r = m->rnd_r;
}
static float random_read(ch_mod *m, float ph, unsigned len, float *state, float coef)
{
    ph -= (float)(int)ph; if (ph < 0) ph += 1;
    unsigned s = (unsigned)(ph * (float)len) % len;
    *state += (m->steps[s] - *state) * coef;
    return *state;
}
static float drift_read(const ch_mod *m, float ph, unsigned len)
{
    ph -= (float)(int)ph; if (ph < 0) ph += 1;
    float pos = ph * (float)len, fr = pos - (float)(int)pos;
    unsigned s = (unsigned)pos % len;
    return ch_lerp(m->steps[s], m->steps[(s + 1) % len], fr);
}
/* the LFO: SHAPE a wave (PHASE, SKEW, SMOOTH a slew), or STEPS / DRIFT: a loop of 16 random values, one a rate
 * cycle, VARIATION redrawing steps as they pass, SMOOTH a glide (STEPS) or a slew (DRIFT) */
static CH_AUDIO_CODE void tick_lfo(ch_mod *m, unsigned n, int restart)
{
    unsigned shape = (unsigned)(m->p[2] + 0.5f);
    if (shape >= CH_LFO_SHAPES) shape = CH_LFO_SINE;
    float spread = m->p[7] * 0.5f;                       /* 0..180 degrees, or half the loop */
    if (shape < CH_LFO_STEPS) {
        float ph;
        if (m->m[0]) {
            ph = synced_phase(m);
            m->wraps = (uint8_t)(int32_t)(ch_t.beat / ch_division_beats(m->p[0], m->m[0]));
        } else {
            if (restart) m->phase = 0;
            m->phase += rate_cycles(m) * (float)n;
            if (m->phase >= 1) m->wraps = (uint8_t)(m->wraps + (unsigned)m->phase);
            m->phase -= (float)(int)m->phase;
            ph = m->phase;
        }
        ph += m->p[4];
        m->cyc = (uint8_t)(m->wraps + (unsigned)(int)ph);   /* (PHASE can carry it into the next) */
        m->hold_phase = ph - (float)(int)ph;            /* (FOLLOW's field, free on an LFO: the phase, for the screen) */
        float l = wave_at(m, shape, ph), r = wave_at(m, shape, ph + spread);
        slew(m, &l, &r, n);
        finish(m, l, r);
        return;
    }
    unsigned len = 16;
    float cyc = rate_cycles(m);                          /* one step per rate cycle */
    float ph;                                            /* position within the whole loop, 0..1 */
    if (m->m[0]) {
        ch_tf steps = ch_t.beat / ch_division_beats(m->p[0], m->m[0]);
        ph = (float)(steps / len - (ch_tf)(int32_t)(steps / len));
    } else {
        if (restart) m->phase = 0;
        m->phase += cyc * (float)n / (float)len; m->phase -= (float)(int)m->phase;
        ph = m->phase;
    }
    ph += m->p[4]; ph -= (float)(int)ph; if (ph < 0) ph += 1;
    int step = (int)(ph * (float)len) % (int)len;
    if (step != m->last_step) {
        float var = ch_abs(m->p[5]);                     /* VARIATION: chance that the step being left gets a new value */
        if (m->last_step >= 0 && ch_randf(&m->rng) < var * var) m->steps[m->last_step] = ch_rands(&m->rng);
        m->last_step = (int8_t)step;
    }
    float l, r;
    if (shape == CH_LFO_DRIFT) {
        l = drift_read(m, ph, len); r = drift_read(m, ph + spread, len);
        slew(m, &l, &r, n);
    } else {
        /* SMOOTH: a glide over a fraction of the step; past ~70% it never arrives */
        float sm = m->p[6], coef = 1;
        if (sm > 0.001f) {
            float step_frames = 1.0f / ch_max(cyc, 1e-7f);
            float tau = step_frames * (sm < 0.7f ? sm * 0.35f : 0.25f + (sm - 0.7f) * 10);
            coef = 1 - ch_exp2(-1.442695f * (float)n / ch_max(tau, 1.0f));
        }
        l = random_read(m, ph, len, &m->rnd_l, coef);
        r = random_read(m, ph + spread, len, &m->rnd_r, coef);
    }
    finish(m, l, r);
}

static CH_AUDIO_CODE void tick_follow(ch_mod *m, ch_trk *t, unsigned n)
{
    static const char dummy = 0; (void)dummy;
    unsigned src = m->m[4];
    float x;
    switch (src) {
    case 0: x = t->lvl_in; break;
    case 1: x = t->lvl_mat; break;
    case 2: case 3: case 4: case 5: x = ch_tr[src - 2].lvl_out; break;
    default: x = ch_mix_level; break;
    }
    x *= m->p[4] * 4;                                    /* INPUT gain, 0.5 = 2x */
    float rise = ch_exp_map(m->p[2], 0.001f, 2.0f), fall = ch_exp_map(m->p[3], 0.005f, 4.0f);
    float dt = (float)n / CH_RATE;
    float cu = 1 - ch_exp2(-1.442695f * dt / rise), cd = 1 - ch_exp2(-1.442695f * dt / fall);
    m->env_l += (x - m->env_l) * (x > m->env_l ? cu : cd);
    /* DELAY: up to ~0.75 s, stored at a quarter of the control rate */
    if (++m->dly_div >= 4) { m->dly_div = 0; m->dly[m->dly_w++ & 63] = m->env_l; }
    unsigned back = (unsigned)(m->p[0] * 63);
    float e = back ? m->dly[(m->dly_w - 1 - back) & 63] : m->env_l;
    /* HOLD: sample and hold at a rate */
    if (m->m[6]) {
        float cyc = m->m[6] == 1 ? ch_exp_map(m->p[6], 0.1f, 50) / CH_RATE : ch_t.beats_per_frame / ch_division_beats(m->p[6], m->m[6] - 1);
        m->hold_phase += cyc * (float)n;
        if (m->hold_phase >= 1) { m->hold_phase -= (float)(int)m->hold_phase; m->hold_l = e; }
        e = m->hold_l;
    }
    e = ch_clamp(e, 0, 1);
    /* the right channel trails by SPREAD: a slower follower */
    float cr = cd * (1 - m->p[7] * 0.9f);
    m->env_r += (e - m->env_r) * (e > m->env_r ? cu * (1 - m->p[7] * 0.9f) : cr);
    float r = m->p[7] > 0 ? m->env_r : e;
    m->uni_l = ch_clamp(e * m->p[1], 0, 1); m->uni_r = ch_clamp(r * m->p[1], 0, 1);
    m->out_l = m->uni_l; m->out_r = m->uni_r;
}

static float curve(float x, float c)
{
    /* 0 linear, - logarithmic (fast start), + exponential (slow start) */
    if (c == 0) return x;
    float e = ch_exp2(c * 2.5f);
    return ch_exp2(e * ch_log2(ch_max(x, 1e-6f)));
}
static CH_AUDIO_CODE void tick_adsr(ch_mod *m, unsigned n)
{
    float dt = (float)n / CH_RATE;
    float a = ch_exp_map(m->p[0], 0.001f, 8), d = ch_exp_map(m->p[1], 0.002f, 8), s = m->p[2], r = ch_exp_map(m->p[3], 0.002f, 12);
    switch (m->stage) {
    case 1: {  /* attack from the current level: no click on retrigger */
        m->phase += dt / a;
        if (m->phase >= 1) { m->phase = 0; m->stage = 2; m->level = 1; break; }
        m->level = m->from + (1 - m->from) * curve(m->phase, m->p[4]);
        break;
    }
    case 2:
        m->phase += dt / d;
        if (m->phase >= 1) { m->phase = 0; m->stage = 3; m->level = s; break; }
        m->level = 1 - (1 - s) * curve(m->phase, -m->p[5]);
        break;
    case 3: m->level = s; break;
    case 4:
        m->phase += dt / r;
        if (m->phase >= 1) { m->phase = 0; m->stage = 0; m->level = 0; break; }
        m->level = m->from * (1 - curve(m->phase, -m->p[6]));
        break;
    default: m->level = 0; break;
    }
    if (m->stage == 1 && m->phase == 0 && m->from == m->level) m->phase = 1e-6f;
    /* SPREAD: the right channel lags */
    float k = 1 - m->p[7] * 0.95f;
    m->env_r += (m->level - m->env_r) * k;
    m->uni_l = m->out_l = m->level;
    m->uni_r = m->out_r = m->p[7] > 0 ? m->env_r : m->level;
}

static CH_AUDIO_CODE void tick_trig(ch_mod *m, unsigned n)
{
    float k = 1;                                         /* SMOOTH: 0 at once, else a glide of 5 ms .. 2 s */
    if (m->p[2] > 0.001f) k = 1 - ch_exp2(-1.4427f * (float)n / (ch_exp_map(m->p[2], 0.005f, 2.0f) * (float)CH_RATE));
    m->hold_l += (m->rnd_l - m->hold_l) * k; m->hold_r += (m->rnd_r - m->hold_r) * k;
    finish(m, m->hold_l, m->hold_r);
}

CH_AUDIO_CODE void ch_mods_tick(ch_trk *t, unsigned ti, unsigned n)
{
    (void)ti;
    /* modulators run while playing (WHEN PLAYING), and restart with playback */
    /* a modulator no map reads moves nothing: it runs once a render call (for its dot on the MOD page), not every
     * sub-block. The maps are read again while the track's values are awake (any command) */
    if (t->pwake) {
        unsigned u = 0;
        for (unsigned i = 0; i < CH_MAPS; ++i) if (t->map[i].used && t->map[i].mod < CH_MODS) u |= 1u << t->map[i].mod;
        t->mod_used = (uint8_t)u;
    }
    static float k32; static unsigned k32_n;            /* (for a sub-block as long as they are now) */
    if (k32_n != ch_sub) { k32_n = ch_sub; k32 = 1 - ch_exp2(-1.4427f * (float)ch_sub / (0.002f * (float)CH_RATE)); }
    unsigned n_sub = n;
    for (unsigned i = 0; i < CH_MODS; ++i) {
        ch_mod *m = &t->mod[i];
        n = n_sub;
        if (!(t->mod_used >> i & 1u)) { if (ch_sub_off) continue; n = ch_frames; }
        switch (m->type) {
        case CH_MOD_WAVE: case CH_MOD_RANDOM: if (ch_t.playing) tick_lfo(m, n, ch_t.started); break;
        case CH_MOD_FOLLOW: tick_follow(m, t, n); break;
        case CH_MOD_ADSR: tick_adsr(m, n); break;
        case CH_MOD_TRIG: tick_trig(m, n); break;
        }
        if (m->type == CH_MOD_WAVE || m->type == CH_MOD_RANDOM || m->type == CH_MOD_TRIG) {
            /* at least ~2 ms between any two values: an edge in the shape is still an edge to the ear, not a click on the parameter */
            float k = n == k32_n ? k32 : 1 - ch_exp2(-1.4427f * (float)n / (0.002f * (float)CH_RATE));
            if (!m->sl_on) { m->sl_l = m->out_l; m->sl_r = m->out_r; m->sl_on = 1; }
            m->sl_l += (m->out_l - m->sl_l) * k; m->sl_r += (m->out_r - m->sl_r) * k;
            m->sl_l = ch_ftz(m->sl_l); m->sl_r = ch_ftz(m->sl_r);
            m->out_l = m->sl_l; m->out_r = m->sl_r;
            if (m->type == CH_MOD_TRIG) { m->uni_l = m->out_l; m->uni_r = m->out_r; }
            else { m->uni_l = m->out_l * 0.5f + 0.5f; m->uni_r = m->out_r * 0.5f + 0.5f; }
            if (m->m[1]) { const float h = 0.5f * m->p[1]; m->out_l = m->out_l * 0.5f + h; m->out_r = m->out_r * 0.5f + h; }   /* AMOUNT UNIPOLAR (an LFO's or a TRIG RND's): 0 .. AMOUNT where it was -AMOUNT .. AMOUNT */
        }
    }
}
