/* ch_fe: PING FM-1's host for the Felucca engines (ch_fe.h has the API). Its own code: the per-voice block
 * buffer (the engines run in 32-sample blocks), the pitch and the block's modulation, a plain ADSR per engine
 * (FM6 brings its own envelopes), velocity, the model-switch fade and the float edge. */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

static const ch_fe_engine *const ENG[CH_FE_MODELS] = {&ch_fe_eng_wheel, &ch_fe_eng_phase, &ch_fe_eng_trio,
                                                      &ch_fe_eng_lofi, &ch_fe_eng_fm6};
const char *const ch_fe_names[CH_FE_MODELS] = {"WHEEL", "PHASE", "TRIO", "LOFI", "FM6"};
/* output scale per model: Felucca's 32768 = 1.0, then this (levels matched by the bench, bench/fe_test.c) */
static const float FE_GAIN[CH_FE_MODELS] = {0.385f, 0.30f, 0.45f, 0.25f, 0.165f};

const char *ch_fe_label(unsigned model, unsigned knob)
{
    return model < CH_FE_MODELS && knob < 3u ? ENG[model]->label[knob] : 0;
}

const char *ch_fe_value(unsigned model, unsigned knob, float x)
{
    if (model >= CH_FE_MODELS || knob >= 3u || !ENG[model]->value)
        return 0;
    return ENG[model]->value(knob, ch_clamp(x, 0.0f, 1.0f));
}

void ch_fe_init(ch_fe_voice *v, unsigned seed)
{
    memset(v, 0, sizeof *v);
    v->seed = seed;
    v->model = 0xFF;
    v->pos = CH_FE_BLOCK;
    v->fade = 1.0f;
}

static void fe_knobs(const float *in, float *k)
{
    unsigned i;
    for (i = 0; i < 3u; i++)
        k[i] = in ? ch_clamp(in[i], 0.0f, 1.0f) : 0.5f;
}

/* the block's pitch: the exact increment from hz, pitch16 its floor in 1/16 semitone, fine the rest */
static void fe_mod(const ch_fe_voice *v, float hz, ch_fe_mod *m)
{
    float p16;
    int32_t p;
    uint32_t base;
    hz = ch_clamp(hz, 8.2f, 12500.0f);
    m->hz = hz;
    m->inc = (uint32_t)(int32_t)(hz * (4294967296.0f / (float)CH_RATE));
    p16 = 16.0f * (69.0f + 12.0f * ch_log2(hz * (1.0f / 440.0f)));
    p = (int32_t)p16;
    p = ch_fe_clamp(p, 0, 2047);
    m->pitch16 = p;
    base = ch_fe_pitch_inc((uint32_t)p);
    m->fine = (base >> 12) ? (int32_t)(m->inc - base) / (int32_t)(base >> 12) : 0;
    m->amp0 = m->amp1 = 0;
    m->cutoff = 0;
    m->shape = 64 << 8;
    m->envq15 = (int32_t)(v->env * 32767.0f);
    m->vel127 = v->vel127;
    m->gate = v->gate;
}

void ch_fe_note_on(ch_fe_voice *v, unsigned model, float hz, float vel, const float knob[3])
{
    const ch_fe_engine *e;
    ch_fe_mod m;
    float k[3];
    int fresh;
    if (model >= CH_FE_MODELS)
        return;
    e = ENG[model];
    fe_knobs(knob, k);
    fresh = !v->active || v->kill || v->model != model;
    if (v->model != model)
        memset(v->st, 0, sizeof v->st);   /* an engine's state starts at zero (Felucca's voices do) */
    if (fresh) {
        v->env = 0;
        v->dc = 0;
        v->pos = CH_FE_BLOCK;             /* a silent voice's leftover tail is dropped */
    }
    vel = ch_clamp(vel, 0.0f, 1.0f);
    v->model = (uint8_t)model;
    v->active = 1;
    v->gate = 1;
    v->stage = 1;
    v->kill = 0;
    v->fade = v->fade_to = 1.0f;
    v->vgain = 0.25f + 0.75f * vel;
    v->vel127 = (uint8_t)(vel * 127.0f + 0.5f);
    v->age++;
    fe_mod(v, hz, &m);
    e->note_on(v->st, &m, k, v->seed * 0x9E3779B9u + v->age, fresh);
}

void ch_fe_note_off(ch_fe_voice *v)
{
    v->gate = 0;
    if (v->active && v->stage && v->stage < 3u)
        v->stage = 3;
}

int ch_fe_active(const ch_fe_voice *v) { return v->active || v->pos < CH_FE_BLOCK; }

/* the host envelope over one block: env from its last value to the next, per stage */
static void fe_env(ch_fe_voice *v, const ch_fe_engine *e)
{
    const float blk = (float)CH_FE_BLOCK / (float)CH_RATE;
    switch (v->stage) {
    case 1:
        v->env += blk / ch_max(e->atk, 0.0005f);
        if (v->env >= 1.0f) {
            v->env = 1.0f;
            v->stage = 2;
        }
        break;
    case 2:     /* to the sustain level: -60 dB of the way in dec seconds */
        v->env = e->sus + (v->env - e->sus) * ch_exp2(-9.966f * blk / ch_max(e->dec, 0.001f));
        break;
    case 3:
        v->env *= ch_exp2(-9.966f * blk / ch_max(e->rel, 0.001f));
        break;
    default:
        v->env = 0;
        break;
    }
}

static void fe_block(ch_fe_voice *v, float hz, const float *k)
{
    const ch_fe_engine *e = ENG[v->model % CH_FE_MODELS];
    ch_fe_mod m;
    float e0 = v->env, f0 = v->fade;
    unsigned i;
    int last = 0;
    memset(v->buf, 0, sizeof v->buf);
    if (e->atk < 0) {                     /* the engine's own envelopes (FM6) */
        if (e->done(v->st)) {
            v->active = 0;
            return;
        }
        fe_mod(v, hz, &m);
        m.amp0 = m.amp1 = 32767;
    } else {
        fe_env(v, e);
        fe_mod(v, hz, &m);
        m.amp0 = (int32_t)(e0 * v->vgain * 32767.0f);
        m.amp1 = (int32_t)(v->env * v->vgain * 32767.0f);
        if (v->stage == 3u && v->env < 0.0002f)
            last = 1;
    }
    e->render(v->st, v->buf, &m, k);
    if (e->atk >= 0) {                    /* a ~7 Hz high-pass: PLS / RING / the wave tables carry DC */
        int32_t dc = v->dc;               /* the mean, Q10 (a plain x >> 10 step would leave up to 1023 of DC) */
        for (i = 0; i < CH_FE_BLOCK; i++) {
            int32_t x = v->buf[i] - (dc >> 10);
            dc += x;
            v->buf[i] = x;
        }
        v->dc = dc;
    }
    if (v->kill) {                        /* the model changed under the voice: fade out over this block */
        v->fade = 0;
        last = 1;
    }
    if (v->fade != f0 || f0 != 1.0f) {
        float d = (v->fade - f0) * (1.0f / (float)CH_FE_BLOCK), g = f0;
        for (i = 0; i < CH_FE_BLOCK; i++) {
            g += d;
            v->buf[i] = (int32_t)((float)v->buf[i] * g);
        }
    }
    if (last) {
        v->active = 0;
        v->stage = 0;
        v->env = 0;
    }
}

void ch_fe_render(ch_fe_voice *v, unsigned model, float hz, const float knob[3], float *out, unsigned n)
{
    float k[3], g;
    if (v->model >= CH_FE_MODELS)
        return;
    if (model != v->model && v->active)
        v->kill = 1;
    fe_knobs(knob, k);
    g = FE_GAIN[v->model] * (1.0f / 32768.0f);
    while (n) {
        unsigned take, i;
        const int32_t *b;
        if (v->pos >= CH_FE_BLOCK) {
            if (!v->active)
                return;
            fe_block(v, hz, k);
            v->pos = 0;
        }
        take = CH_FE_BLOCK - v->pos;
        if (take > n)
            take = n;
        b = v->buf + v->pos;
        for (i = 0; i < take; i++)
            out[i] += (float)b[i] * g;
        out += take;
        n -= take;
        v->pos = (uint8_t)(v->pos + take);
    }
}
