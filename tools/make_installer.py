"""PING FM1's installer page, on this PC only: fm1-base's own web installer (base/web/installer.html with its
fm1ports.js, fm1pkg.js and fm1ota.js, as base/tools/make_site.py fills it), for a release package, without the rest of
a site (no emulator, front page or manual) and without the m-bootloader offer (it would rewrite the boot area).
    python tools/make_installer.py [PKG] [VERSION]     (default app/build/ping fm1-1.0.fwsc, 1.0)  ->  dev/out/installer
Open it through dev/installer.ps1 (a local server: Web MIDI needs http://localhost, not a file)."""
import json, re, shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'base'
sys.path.insert(0, str(BASE / 'tools'))
from make_site import strip_module, product_of          # noqa: E402  (fm1-base's own helpers)
from build import read_conf                              # noqa: E402

pkg = Path(sys.argv[1]) if len(sys.argv) > 1 else None   # default: the package build.py writes for the name in app/fm1.toml (set below)
version = sys.argv[2] if len(sys.argv) > 2 else '1.0.1'
out = ROOT / 'dev' / 'out' / 'installer'
conf = read_conf(ROOT / 'app' / 'fm1.toml')
if pkg is None:
    pkg = ROOT / 'app' / 'build' / f"{conf['name'].lower()}-{version}.fwsc"
raw = pkg.read_bytes()
product = product_of(raw)
if not re.fullmatch(re.escape(conf['id_prefix']) + r'\d{6}', product):
    sys.exit(f'{pkg}: identity {product!r}: needs a release build (dev/fwbuild.ps1 --release {version})')
if b'FELUCCA-LOADER-1' not in raw:
    sys.exit(f'{pkg}: no update loader in it')
stamp = int(pkg.stat().st_mtime)                      # each build its own file name: no browser hands back an old one
name = f"{conf['name'].lower().replace(' ', '-')}-{version}-{stamp}.fwsc"
if out.exists():
    shutil.rmtree(out)
(out / 'install').mkdir(parents=True); (out / 'firmware').mkdir()
shutil.copy(pkg, out / 'firmware' / name)
for lic in sorted((BASE / 'LICENSES').glob('*.txt')):
    shutil.copy(lic, out / 'firmware' / lic.name)
html = (BASE / 'web' / 'installer.html').read_text(encoding='utf-8')
lib = '\n'.join(strip_module((BASE / 'web' / f).read_text(encoding='utf-8')) for f in ('fm1ports.js', 'fm1pkg.js', 'fm1ota.js'))
meta = json.dumps({'version': version, 'product': product, 'pkg': '../firmware/' + name,
                   'backup': bool(conf.get('backup')), 'res': None, 'boot': None})
for key, val in (('RES', "{{NAME}}'s data"), ('RES_JA', '{{NAME}} のデータ'), ('NAME', conf.get('display', conf['name'])), ('REPO', ''),
                 ('REPO_SHORT', ''), ('LICENSE', conf.get('licence', 'MPL-2.0')),
                 ('CREDITS', ' ' + conf.get('credits', ''))):
    html = html.replace('{{' + key + '}}', val)
# One look for the three pages, and one way in. THEME goes into the install and transfer pages after their own styles:
# PING's colours and face over whatever they had. Opened on their own (an old bookmark) they go to the front page, which
# shows them in a frame behind the terms; there they drop their own titles, as the front page has the name.
THEME = """<style>
  @font-face { font-family: "Jost"; src: url("../manual/Jost.ttf"); font-weight: 100 900; font-display: swap; }
  :root { --bg: #0b0b0c !important; --fg: #f2f2f0 !important; --dim: #7d7d7a !important; --accent: #f2f2f0 !important;
          --acc: #f2f2f0 !important; --on-accent: #0b0b0c !important; --bad: #ff5c3d !important; --focus: #f2f2f0 !important; --line: #2a2a2c !important;
          --yel: #f2f2f0 !important; --teal: #f2f2f0 !important; --field: #111113; }
  :root[data-theme="light"] { --bg: #f2f2ee !important; --fg: #111112 !important; --dim: #7a7a76 !important; --accent: #111112 !important;
          --acc: #111112 !important; --on-accent: #f2f2ee !important; --bad: #cf3a1c !important; --focus: #111112 !important; --line: #d6d6d0 !important;
          --yel: #111112 !important; --teal: #111112 !important; --field: #ffffff; }
  input[type=text], select { background: var(--field) !important; }
  button.plain { background: none !important; }
  body { font: 15px/1.55 "Jost", "Segoe UI", system-ui, sans-serif !important; }
  main, footer { max-width: none !important; margin: 0 !important; padding-left: 17.5vw !important; padding-right: 17.5vw !important; }
  @media (max-width: 1000px) { main, footer { padding-left: 30px !important; padding-right: 30px !important; } }
  main { padding-top: 42px !important; }
  h2 { font-size: 15px !important; font-weight: 400 !important; letter-spacing: 0 !important; text-transform: uppercase; color: var(--dim) !important; margin: 44px 0 10px !important; }
  .small, .hint, .cap, .sub { font-size: 14px !important; }
  button { border-radius: 0 !important; font-weight: 400 !important; padding: 7px 18px !important; background: var(--fg) !important; color: var(--bg) !important; border: 1px solid var(--fg) !important; }
  button.plain, button.small, button.lang { background: none !important; color: var(--fg) !important; }
  button.lang { border: 0 !important; padding: 0 !important; color: var(--dim) !important; }
  button.rec { background: #ff5c3d !important; border-color: #ff5c3d !important; color: var(--bg) !important; }
  .backup, .drop, dialog, textarea, input[type=text], select, #log { border-radius: 0 !important; }
  .backup { border-width: 1px !important; }
  .backup strong { color: inherit !important; }
  a { color: inherit; }
  main > header, main > h1, main > p:last-child > a[href="../"] { display: none !important; }
  @media (max-width: 640px) {
    main, footer { padding-left: 18px !important; padding-right: 18px !important; }
    main { padding-top: 24px !important; }
    table { font-size: 14px; }
    input[type=text] { width: 110px !important; }
  }
</style>
<script>try { if (localStorage.getItem('ping-os1-theme') !== 'dark') document.documentElement.dataset.theme = 'light'; } catch (e) { document.documentElement.dataset.theme = 'light'; }</script>
<script>if (window.top === window.self) location.replace('../#' + location.pathname.replace(/\\/+$/, '').split('/').pop());</script>
</head>"""
def themed(page):
    assert page.count('</head>') == 1
    return page.replace('</head>', THEME)
(out / 'install' / 'index.html').write_text(themed(html.replace('/*LIB*/', lib).replace('/*META*/', meta).replace('/*APP*/', 'null')),
                                             encoding='utf-8')
# the sample loader and recorder page (app/web/loader.html), with the transfer code inlined
(out / 'transfer').mkdir()
loader = (ROOT / 'app' / 'web' / 'loader.html').read_text(encoding='utf-8')
llib = '\n'.join(strip_module((BASE / 'web' / f).read_text(encoding='utf-8')) for f in ('fm1ports.js', 'fm1ota.js'))
(out / 'transfer' / 'index.html').write_text(themed(loader.replace('/*LIB*/', llib)), encoding='utf-8')
# the manual (docs/build_manual.py's page and PDF)
manual = ROOT / 'docs' / 'manual'
if (manual / 'index.html').exists():
    shutil.copytree(manual, out / 'manual')
    mi = out / 'manual' / 'index.html'
    DARK = """<style>
  html.embed { --ink: #f2f2f0; --mute: #7d7d7a; --line: #2a2a2c; --bg: #0b0b0c; }
  html.embed[data-theme="light"] { --ink: #111112; --mute: #7a7a76; --line: #d6d6d0; --bg: #f2f2ee; }
  html.embed main { max-width: none; margin: 0; padding: 42px 17.5vw 80px; }
  @media (max-width: 1000px) { html.embed main { padding: 42px 30px 80px; } }
  html.embed:not([data-theme="light"]) code { background: #1a1a1c; border-color: #2a2a2c; }
  html.embed:not([data-theme="light"]) kbd.btn { background: #f2f2f0; color: #0b0b0c; border-color: #f2f2f0; }
  html.embed kbd.knob { background: none; }
  html.embed:not([data-theme="light"]) kbd.knob { color: #f2f2f0; border-color: #f2f2f0; }
  html.embed:not([data-theme="light"]) kbd.knob::before { background: #f2f2f0; }
  html.embed h1 { font-size: 30px; font-weight: 500; }
  html.embed h2 { font-size: 20px; font-weight: 500; }
  @media (max-width: 640px) {
    html.embed main { padding: 24px 18px 56px; }
    html.embed table, html.embed thead, html.embed tbody, html.embed tr, html.embed td, html.embed th { display: block; width: auto !important; }
    html.embed thead { display: none; }
    html.embed td { border: 0; padding: 0 0 2px; white-space: normal !important; }
    html.embed td:first-child { padding-top: 9px; color: var(--ink); }
    html.embed td:last-child { padding-bottom: 9px; border-bottom: 1px solid var(--line); }
    html.embed figure { gap: 12px; }
    html.embed figure > div, html.embed figure img { width: 100%; max-width: 240px; height: auto; }
  }
</style>
<script>if (window.top !== window.self) { document.documentElement.classList.add('embed'); try { if (localStorage.getItem('ping-os1-theme') !== 'dark') document.documentElement.dataset.theme = 'light'; } catch (e) {} }</script>
</head>"""
    mh = mi.read_text(encoding='utf-8')
    assert mh.count('</head>') == 1
    mi.write_text(mh.replace('</head>', DARK), encoding='utf-8')
# PING OS-1 in the browser (app/web/play: the page, its audio worklet, and the firmware as WebAssembly from sim/web/build.ps1)
play = ROOT / 'app' / 'web' / 'play'
if (play / 'ping-os-1.wasm').exists():
    shutil.copytree(play, out / 'play')
# the front page (app/web/home.html): the mark, the tabs, the terms to accept before the tools open
brand = ROOT / 'app' / 'web' / 'brand'
shutil.copytree(brand, out / 'brand')
mark = re.sub(r'<\?xml[^>]*\?>\s*|<!--.*?-->\s*', '', (brand / 'ping-mark.svg').read_text(encoding='utf-8'), flags=re.S)
mark = mark.replace('<svg ', '<svg role="img" aria-label="PING" ', 1).replace('#231f20', 'currentColor')   # the black line mark, in the page's ink
home = (ROOT / 'app' / 'web' / 'home.html').read_text(encoding='utf-8')
(out / 'index.html').write_text(home.replace('{{MARK}}', mark).replace('{{VERSION3}}', version if version.count('.') > 1 else version + '.0').replace('{{VERSION}}', version).replace('{{IDENT}}', product), encoding='utf-8')
print(f'installer: {out}\\install\\index.html  ({product}, firmware/{name}, {len(raw)} B)')
print(f'front:     {out}\\index.html  (http://localhost:8765/: play, install, transfer, manual, terms)')
