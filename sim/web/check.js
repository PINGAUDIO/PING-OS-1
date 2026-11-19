// A headless check of the browser build: node sim/web/check.js  -> dev/out/web-check.ppm, and the sound's peak.
const fs = require('fs'), path = require('path');
const root = path.resolve(__dirname, '..', '..');
const mod = new WebAssembly.Module(fs.readFileSync(path.join(root, 'app/web/play/ping-os-1.wasm')));
console.log('imports:', WebAssembly.Module.imports(mod).map(i => i.module + '.' + i.name).join(' '));
const imports = {};
let saved = 0;
for (const i of WebAssembly.Module.imports(mod)) {
  if (i.kind !== 'function') continue;
  (imports[i.module] = imports[i.module] || {})[i.name] = i.name === 'js_store' ? () => { ++saved; } : () => 0;
}
const x = new WebAssembly.Instance(mod, imports).exports;
if (x._initialize) x._initialize();
x.web_init();
let peak = 0;
const run = ms => { for (let n = Math.ceil(ms * 44.1 / 128); n > 0; --n) { x.web_process(128); const o = new Float32Array(x.memory.buffer, x.web_out(0), 128); for (const v of o) peak = Math.max(peak, Math.abs(v)); } };
const tap = b => { x.web_input(1 << b, 0); run(60); x.web_input(0, 0); run(60); };
run(8000); tap(4); run(300);          // the start screen and its sound, then EDIT past the first-start card
console.log('start sound peak', peak.toFixed(3)); peak = 0;
tap(10); run(3000);                   // PLAY
console.log('playing peak', peak.toFixed(3), 'objects saved', saved);
const fb = new Uint8Array(x.memory.buffer, x.web_fb(), 240 * 240 * 4), rgb = Buffer.alloc(240 * 240 * 3);
for (let i = 0; i < 240 * 240; ++i) { rgb[i * 3] = fb[i * 4]; rgb[i * 3 + 1] = fb[i * 4 + 1]; rgb[i * 3 + 2] = fb[i * 4 + 2]; }
fs.writeFileSync(path.join(root, 'dev/out/web-check.ppm'), Buffer.concat([Buffer.from('P6\n240 240\n255\n'), rgb]));
console.log('lights', Array.from(new Uint32Array(x.memory.buffer, x.web_lights(), 6)).map(v => v.toString(16)).join(' '));
