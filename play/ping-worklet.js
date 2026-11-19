// PING OS-1 in the page's audio thread: the firmware as WebAssembly (sim/web/ping_web.c), which makes the sound and
// runs the panel; the page (index.html) sends what is pressed and turned and gets the screen and the lights back.
class PingOS1 extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions;
    const mod = new WebAssembly.Module(o.wasm), imports = {};
    for (const i of WebAssembly.Module.imports(mod)) {
      if (i.kind !== 'function') continue;
      (imports[i.module] = imports[i.module] || {})[i.name] = () => 0;
    }
    // a saved object: a copy to the page, which keeps it in the browser for the next visit
    imports.env.js_store = (obj, ptr, len) => {
      const d = new Uint8Array(this.x.memory.buffer, ptr, len).slice();
      this.port.postMessage({ t: 'store', obj: obj, data: d }, [d.buffer]);
    };
    const x = this.x = new WebAssembly.Instance(mod, imports).exports;
    if (x._initialize) x._initialize();
    for (const k of Object.keys(o.store || {})) {            // what was saved on an earlier visit
      const d = o.store[k], p = x.web_alloc(d.length);
      new Uint8Array(x.memory.buffer, p, d.length).set(d);
      x.web_store_put(k | 0, p, d.length); x.web_free(p);
    }
    x.web_init();
    this.step = 44100 / sampleRate;                          // frames of ours for one of the page's
    this.pos = 0; this.last = [0, 0]; this.buf = [new Float32Array(0), new Float32Array(0)]; this.have = 0; this.at = 0;
    this.since = 0;
    this.port.onmessage = e => {
      const m = e.data;
      if (m.t === 'in') x.web_input(m.b >>> 0, m.k >>> 0);
      else if (m.t === 'turn') x.web_turn(m.r, m.d);
      else if (m.t === 'vol') x.web_master(m.v);
      else if (m.t === 'midi') x.web_midi(m.s, m.a, m.b);
      else if (m.t === 'splash') x.web_splash();
    };
  }
  pull(n) {                                                  // n more frames at 44.1 kHz
    const x = this.x; x.web_process(n);
    this.buf = [new Float32Array(x.memory.buffer, x.web_out(0), n), new Float32Array(x.memory.buffer, x.web_out(1), n)];
    this.have = n; this.at = 0;
  }
  process(inputs, outputs) {
    const out = outputs[0], L = out[0], R = out[1] || out[0], n = L.length, x = this.x;
    if (this.step === 1) {
      this.pull(n); L.set(this.buf[0]); if (R !== L) R.set(this.buf[1]);
    } else {                                                 // another rate: a line between our frames
      for (let i = 0; i < n; ++i) {
        while (this.pos >= 1) {
          if (this.at >= this.have) this.pull(128);
          this.last[0] = this.cur0 || 0; this.last[1] = this.cur1 || 0;
          this.cur0 = this.buf[0][this.at]; this.cur1 = this.buf[1][this.at]; ++this.at; this.pos -= 1;
        }
        L[i] = this.last[0] + ((this.cur0 || 0) - this.last[0]) * this.pos;
        if (R !== L) R[i] = this.last[1] + ((this.cur1 || 0) - this.last[1]) * this.pos;
        this.pos += this.step;
      }
    }
    this.since += n;
    if (this.since >= sampleRate / 40) {                     // the screen and the lights, 40 times a second at most
      this.since = 0;
      const lights = new Uint32Array(x.memory.buffer, x.web_lights(), 6).slice();
      if (x.web_fb_take()) {
        const fb = new Uint8ClampedArray(x.memory.buffer, x.web_fb(), 240 * 240 * 4).slice();
        this.port.postMessage({ t: 'frame', fb: fb, lights: lights }, [fb.buffer]);
      } else this.port.postMessage({ t: 'frame', lights: lights });
    }
    return true;
  }
}
registerProcessor('ping-os-1', PingOS1);
