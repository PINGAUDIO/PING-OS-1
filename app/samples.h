/* PING OS-1: the sample bank in the flash. Up to eight mono samples (blocked IMA ADPCM, ch_adpcm.h, at their own
 * rates) in the data area's sectors 0xC9000..0xDC000: a commit record in the first (magic, the payload's length and
 * CRC-32, the record's CRC-32), the payload from 0xCA000 (at most 18 sectors, 73,728 bytes): a table (magic SMPB,
 * the count, eight entries: offset, bytes, frames, rate, tempo x10, a name) then the samples. The engine reads
 * them in place (ch_sample_bind). The host writes a bank with the app's USB command 0x50 (the installer's SysEx
 * framing: the body the payload's length and CRC-32), then the FM-1 pulls the payload with read requests, as an
 * update does, checks it and commits it; the host learns the result from a read of 0xC0000000 (ok) or
 * 0xCF000000 | a code. app/web/loader.html (tools/make_installer.py) is the page that does it; tools/
 * make_samplebank.py makes the same file from WAVs on the PC. */
#pragma once

enum { MX_REQ = 0x51 };                                 /* the Transfer page: send the pattern, for a MIDI file */
enum { PX_REQ = 0x52 };                                 /* and: send a piece of the saved project, for the plugin */
enum { SMP_REQ = 0x50, SMP_BASE = 0xC9000u, SMP_DATA = 0xCA000u, SMP_SECTORS = 18, SMP_MAX = SMP_SECTORS * 4096u,
       SMP_MAGIC = 0x31504D53u /* "SMP1" */, SMP_TABLE = 0x42504D53u /* "SMPB" */, SMP_ENTRY = 32, SMP_HEAD = 8 + 8 * SMP_ENTRY };
enum { SMP_OK_ADDR = 0xC0000000u, SMP_FAIL_ADDR = 0xCF000000u };
typedef struct { uint32_t magic, len, crc, hcrc; } smp_rec;
static ch_sample smp_tab[CH_SAMPLES];
static char smp_name[CH_SAMPLES][16];
static uint8_t smp_count;

static uint32_t smp_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t smp_crc32(uint32_t c, const uint8_t *p, uint32_t n)   /* zlib's, as the host computes it */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    c = ~c;
    while (n--) { c ^= *p++; c = (c >> 4) ^ T[c & 15u]; c = (c >> 4) ^ T[c & 15u]; }
    return ~c;
}
/* the bank as the flash holds it, bound to the engine's slots (none: the slots empty) */
static void smp_bind_all(void)
{
    smp_count = 0;
    for (unsigned i = 0; i < CH_SAMPLES; ++i) { smp_name[i][0] = '-'; smp_name[i][1] = 0; ch_sample_bind(i, 0); ch_sample_name_set(i, smp_name[i]); }
    const uint8_t *r = plat_data(SMP_BASE), *p = plat_data(SMP_DATA);
    if (!r || !p) return;
    smp_rec h; memcpy(&h, r, sizeof h);
    if (h.magic != SMP_MAGIC || h.len < SMP_HEAD || h.len > SMP_MAX || h.hcrc != smp_crc32(0, (const uint8_t *)&h, 12)) return;
    if (smp_rd32(p) != SMP_TABLE) return;
    unsigned count = smp_rd32(p + 4); if (count > CH_SAMPLES) count = CH_SAMPLES;
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *e = p + 8 + i * SMP_ENTRY;
        uint32_t off = smp_rd32(e), bytes = smp_rd32(e + 4), frames = smp_rd32(e + 8), rate = smp_rd32(e + 12);
        unsigned bpm10 = e[16] | (unsigned)e[17] << 8;
        if (!frames || off < SMP_HEAD || off + bytes > h.len || rate < 8000 || rate > 48000) continue;
        if (bytes < ch_adpcm_bytes(frames)) continue;
        smp_tab[i].adpcm = p + off; smp_tab[i].frames = frames; smp_tab[i].rate = rate; smp_tab[i].bpm = bpm10 / 10.0f;
        unsigned k = 0; for (; k < 13 && e[18 + k]; ++k) smp_name[i][k] = (char)e[18 + k]; smp_name[i][k] = 0;
        if (!k) { smp_name[i][0] = 'S'; smp_name[i][1] = (char)('1' + i); smp_name[i][2] = 0; }
        ch_sample_name_set(i, smp_name[i]);
        ch_sample_bind(i, &smp_tab[i]);
        ++smp_count;
    }
}
/* the host's bank (SMP_REQ): pulled, written, checked, committed; the result read back to the host */
static void midi_export(void);                         /* (app.c: the pattern out to the Transfer page, which makes the MIDI file) */
static void project_export(const uint8_t *d, uint32_t n);   /* (app.c: the saved project out, a piece a request) */
void app_usb_cmd(const uint8_t *d, uint32_t n)
{
    if (n >= 3 && d[2] == MX_REQ) { midi_export(); return; }
    if (n >= 3 && d[2] == PX_REQ) { project_export(d, n); return; }
    if (n < 14 || d[2] != SMP_REQ) return;
    uint32_t len = smp_rd32(d + 6), crc = smp_rd32(d + 10);
    uint8_t tmp[8]; int rc = 0;
    static uint8_t buf[512];
    if (len < SMP_HEAD || len > SMP_MAX) rc = 2;
    else {
        for (unsigned i = 0; i < CH_SAMPLES; ++i) ch_sample_bind(i, 0);   /* nothing reads the region meanwhile */
        app_say("SAMPLES", "LOADING"); ui.msg_until = plat_ms() + 60000u; ui.redraw = 1;
        plat_host_patience(5000, 0);
        for (unsigned s = 0; s < SMP_SECTORS + 1 && !rc; ++s) if (plat_data_erase(SMP_BASE + s * 4096u)) rc = 7;
        for (uint32_t off = 0; off < len && !rc; off += 512u) {
            uint32_t k = len - off > 512u ? 512u : len - off;
            if (plat_host_read(off, buf, k)) rc = 2;
            else if (plat_data_write(SMP_DATA + off, buf, k)) rc = 8;
            else if (!(off & 0xFFFu)) { char pc[8]; int_text((int32_t)(off * 100u / len), pc); text_cat(pc, "%"); app_say("SAMPLES", pc); ui.msg_until = plat_ms() + 60000u; header(); }
        }
        if (!rc) { const uint8_t *p = plat_data(SMP_DATA); if (!p || smp_crc32(0, p, len) != crc) rc = 22; }
        if (!rc) {
            smp_rec h; h.magic = SMP_MAGIC; h.len = len; h.crc = crc; h.hcrc = smp_crc32(0, (const uint8_t *)&h, 12);
            if (plat_data_write(SMP_BASE, &h, sizeof h)) rc = 8;
        }
        smp_bind_all();
    }
    plat_host_read(rc ? SMP_FAIL_ADDR | (uint32_t)rc : SMP_OK_ADDR, tmp, 8);
    { char v[12] = ""; int_text(smp_count, v); text_cat(v, " SAMPLES"); app_say(rc ? "SAMPLES" : "LOADED", rc ? "FAILED" : v); }
    ui.msg_until = plat_ms() + 2500u; ui.redraw = 1;
}
