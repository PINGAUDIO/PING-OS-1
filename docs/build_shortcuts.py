"""PING OS-1's shortcuts on one page: docs/manual/shortcuts.html and docs/manual/PING-OS-1-Shortcuts.pdf (Letter, landscape,
printed by Chrome or Edge headless). Each entry: the controls drawn as on the panel, then what they do.

A control in a combo is written as a token:
  NAME        a panel button            k:NAME   a knob (K1, ALGO, SELECT, PRESET)
  w:1-4       white keys                b:1-8    black keys
  h:NAME      a button held             t:text   a plain word between them ("or", "then")
"""
import subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'docs' / 'manual'

SECTIONS = [
 ('Views', [
  ('EDIT', 'Engine page. Again: layout. Hold: engine off / on'),
  ('FX', 'Next effect slot. Hold: bypass the slot'),
  ('SEQ', 'STEP view. Again: back'),
  ('HOME', 'TRACKS view. Again: back'),
  ('GLO', 'Mix and global pages'),
  ('LFO', 'Modulators'),
  ('ARP', 'Line generator. Again there, or held: a new line'),
  ('h:EDIT GLO', 'Track rows as a MIXER with meters'),
  ('h:GLO FX', 'Master effects chain'),
  ('h:GLO PLAY', 'Tap tempo'),
  ('h:HOME FX', 'PERFORM: master effect amounts'),
  ('h:GLO HOME', 'Spectrum display'),
  ('h:SAVE EDIT', 'LIBRARY: sounds, patterns, project'),
  ('h:SEQ ENV', 'Envelope display and its knobs'),
 ]),
 ('Sound', [
  ('k:ALGO', 'Pick the engine, or the effect in a slot'),
  ('h:SEQ k:K1-4', 'That parameter: its mode'),
  ('h:SEQ w:1-4', 'The engine: its actions'),
  ('h:LFO k:K1-4', 'Map the modulator (also MIX level, pan, filter)'),
  ('h:LFO h:SEL k:K1-4', 'Map TRIG RND to it'),
  ('h:LFO w:1-4', 'Choose the modulator'),
  ('h:LFO REC', 'Clear the modulator maps'),
  ('OCT- OCT+', 'Together: K1 to K4 play the macros'),
  ('h:OCT- h:OCT+ w:1-4', 'Assign that macro: turn knobs to map'),
  ('h:SAVE w:1-16', 'Store the sound as a preset'),
  ('h:EDIT w:1-16', 'Load a preset into the track'),
  ('OCT- t:or OCT+', 'Keyboard an octave down / up'),
 ]),
 ('Project', [
  ('PLAY', 'Start and stop'),
  ('SAVE', 'Save the project'),
  ('h:HOME w:1-16', 'Switch pattern (at the bar while playing)'),
  ('h:HOME h:SEQ w:1-16', 'Copy the pattern there'),
  ('h:SEQ SAVE', 'INIT. Then SAVE: the track, REC: everything'),
 ]),
 ('Sequencer', [
  ('w:1-16', 'Steps on / off. Hold one and turn K1 to K4'),
  ('b:1-8', 'Choose the lane (and hear it)'),
  ('ENV', 'Keys play the track. Again: steps'),
  ('SEL', 'The BROWSER: engines, models, presets, effects'),
  ('h:SEQ k:K1-4', 'LENGTH, RATE, DIRECTION, SWING'),
  ('h:SEQ k:ALGO', 'Scale. In TRACKS: tempo'),
  ('h:SEQ k:SELECT', 'Sequencer page'),
  ('h:SEQ EDIT', 'Lane notes: SELECT the lane, K1 the note, SEQ ends'),
  ('h:SEQ w:8-11', 'Jump to page 1 to 4'),
  ('h:SEQ w:12-16', 'Generators: melody, beat, locks, lanes, vary'),
  ('h:EDIT OCT- t:or OCT+', 'Step knobs: OCT+ OCTAVE, SEND, CYCLE, GATE; OCT- back'),
  ('h:EDIT k:ALGO', 'Track key'),
  ('h:EDIT k:SELECT', 'Track scale'),
  ('h:EDIT k:PRESET', 'Lanes a semitone or scale step'),
  ('OCT- t:or OCT+', 'In STEP: all lanes an octave'),
  ('h:SEL k:SELECT', 'On a held step: move it along or across lanes'),
 ]),
 ('Tracks and mix', [
  ('k:PRESET', 'Choose the track, T1 to T4'),
  ('h:ENV w:1-4', 'Mute or unmute that track'),
  ('h:ENV b:1-4', 'Solo that track'),
  ('h:SEL k:ALGO', 'Track level, from any view'),
  ('h:HOME k:K1', 'Stutter all tracks, 1/8 to 1/64'),
  ('h:HOME k:K2', 'Stutter this track'),
  ('k:K1-4', 'EDIT mixer: levels of T1-T4'),
  ('h:FX k:K1-4', 'EDIT mixer: master effect amounts'),
  ('k:ALGO', 'Mixer shown: level of the chosen strip'),
  ('k:PRESET', 'Mixer shown: T1-T4, then RETURN'),
  ('k:SELECT', 'Turn the pages'),
 ]),
 ('Record and generate', [
  ('REC', 'Live record at the playhead. Hold and release: metronome'),
  ('h:REC w:1-16', 'Arm steps for parameter locks'),
  ('h:SEQ REC', 'Clear the track (TRACKS: the pattern)'),
  ('h:EDIT REC', 'Undo the last generator or clear. Again: redo'),
  ('h:HOME ARP', 'In TRACKS: a new line, staying there'),
  ('h:SEQ ARP', 'In STEP: a new line, staying there'),
  ('h:SEQ FX', 'On a TRANCE effect: keys play notes, or set its steps again'),
 ]),
]

def tok(t):
    if t.startswith('t:'): return f'<span class="w">{t[2:]}</span>'
    if t.startswith('k:'): return f'<span class="knob"><i></i><b>{t[2:]}</b></span>'
    if t.startswith('w:'): return f'<span class="key wk"><b>{t[2:]}</b></span>'
    if t.startswith('b:'): return f'<span class="key bk"><b>{t[2:]}</b></span>'
    if t.startswith('h:'): return f'<span class="btn held">{t[2:]}<u>HOLD</u></span>'
    return f'<span class="btn">{t}</span>'

def combo(c):
    out = []
    for i, t in enumerate(c.split()):
        prev = c.split()[i - 1] if i else ''
        if i and not t.startswith('t:') and not prev.startswith('t:'): out.append('<span class="plus">+</span>')
        out.append(tok(t))
    return ''.join(out)

CSS = '''
@page { size: 11in 8.5in; margin: 0; }
@font-face { font-family: Jost; src: url(Jost.ttf); font-weight: 100 900; }
* { box-sizing: border-box; -webkit-print-color-adjust: exact; print-color-adjust: exact; }
html, body { margin: 0; padding: 0; }
body { width: 11in; height: 8.5in; padding: .3in .36in .22in; font-family: Jost, "Segoe UI", sans-serif; color: #17181c; overflow: hidden; }
header { display: flex; align-items: baseline; gap: .16in; border-bottom: 2.5px solid #ff8424; padding-bottom: .05in; margin-bottom: .1in; }
header h1 { margin: 0; font-size: 21pt; font-weight: 800; letter-spacing: .02em; }
header .sub { font-size: 10.5pt; font-weight: 500; color: #555a66; }
header .leg { margin-left: auto; display: flex; align-items: center; gap: .09in; font-size: 7.4pt; color: #555a66; }
header .leg > span { display: inline-flex; align-items: center; gap: .04in; }
.cols { column-count: 4; column-gap: .2in; column-fill: auto; height: 7.3in; }
section { break-inside: avoid; margin-bottom: .14in; }
section.brk { break-before: column; }
h2 { margin: 0 0 .04in; font-size: 9.4pt; font-weight: 800; letter-spacing: .13em; text-transform: uppercase; color: #fff; background: #17181c; padding: 2px 7px; border-radius: 4px; }
.row { display: flex; align-items: center; gap: .07in; padding: 2.2px 0; border-bottom: .6px solid #e4e5ea; min-height: .335in; }
.row:last-child { border-bottom: 0; }
.c { flex: 0 0 auto; display: flex; align-items: center; gap: 2px; }
.d { flex: 1 1 auto; font-size: 8.1pt; line-height: 1.17; font-weight: 450; }
.btn { position: relative; display: inline-flex; align-items: center; justify-content: center; height: .21in; min-width: .36in; padding: 0 4px; font-size: 6.9pt; font-weight: 700; letter-spacing: .04em; color: #fff; background: #2b2d36; border-radius: 4px; border-bottom: 2px solid #0c0d10; }
.btn.held { background: #ff8424; color: #17181c; border-bottom-color: #b85a10; padding-bottom: 4.5px; height: .235in; }
.btn u { position: absolute; left: 0; right: 0; bottom: .5px; text-align: center; text-decoration: none; font-size: 3.7pt; font-weight: 800; letter-spacing: .1em; }
.plus { font-size: 7.5pt; font-weight: 700; color: #8a8e9a; padding: 0 .5px; }
.w { font-size: 6.6pt; color: #555a66; padding: 0 2px; font-style: italic; }
.knob { display: inline-flex; flex-direction: column; align-items: center; line-height: 1; }
.knob i { display: block; width: .15in; height: .15in; border-radius: 50%; background: #d9dbe2; border: 1.4px solid #2b2d36; position: relative; }
.knob i::after { content: ""; position: absolute; left: 50%; top: 1px; width: 1.4px; height: 40%; margin-left: -.7px; background: #2b2d36; }
.knob b { font-size: 4.9pt; font-weight: 700; letter-spacing: .03em; margin-top: 1px; }
.key { display: inline-flex; align-items: flex-end; justify-content: center; width: .27in; height: .215in; border-radius: 2px 2px 4px 4px; padding-bottom: 1.5px; }
.key b { font-size: 5.6pt; font-weight: 700; }
.wk { background: #fff; border: 1.3px solid #2b2d36; border-bottom-width: 2.4px; color: #17181c; }
.bk { background: #17181c; border: 1.3px solid #17181c; color: #fff; width: .25in; height: .17in; }
footer { position: absolute; left: .36in; right: .36in; bottom: .16in; font-size: 6.6pt; color: #7a7e8a; display: flex; justify-content: space-between; }
'''

def html():
    cols = []
    for title, rows in SECTIONS:
        r = ''.join(f'<div class="row"><div class="c">{combo(c)}</div><div class="d">{d}</div></div>' for c, d in rows)
        brk = ' class="brk"' if title in ('Sound', 'Sequencer', 'Tracks and mix') else ''
        cols.append(f'<section{brk}><h2>{title}</h2>{r}</section>')
    legend = (f'<span>{tok("NAME")} press</span><span>{tok("h:NAME")} hold</span><span>{tok("k:KNOB")} turn</span>'
              f'<span>{tok("w:1-16")} white keys</span><span>{tok("b:1-8")} black keys</span>')
    return (f'<!doctype html><html><head><meta charset="utf-8"><title>PING OS-1 Shortcuts</title><style>{CSS}</style></head><body>'
            f'<header><h1>PING OS-1</h1><span class="sub">Shortcuts and key combos</span><div class="leg">{legend}</div></header>'
            f'<div class="cols">{"".join(cols)}</div>'
            f'<footer><span>Hold the orange button, then press or turn the next control.</span><span>PING OS-1 1.0</span></footer></body></html>')

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    src = ROOT / 'app' / 'assets' / 'fonts' / 'Jost[wght].ttf'
    if src.exists() and not (OUT / 'Jost.ttf').exists():
        import shutil; shutil.copy(src, OUT / 'Jost.ttf')
    page = OUT / 'shortcuts.html'
    page.write_text(html(), encoding='utf-8')
    for exe in (r'C:\Program Files\Google\Chrome\Application\chrome.exe', r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'):
        if Path(exe).exists():
            pdf = OUT / 'PING-OS-1-Shortcuts.pdf'
            if pdf.exists(): pdf.unlink()
            prof = Path(tempfile.gettempdir()) / 'ping-os-1-shortcuts-print'
            subprocess.run([exe, '--headless=new', '--disable-gpu', f'--user-data-dir={prof}', '--no-pdf-header-footer', f'--print-to-pdf={pdf}',
                            '--allow-file-access-from-files', page.as_uri()], check=False, timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if pdf.exists():
                print(f'{pdf}  {pdf.stat().st_size} B'); break
    else:
        print('no browser for the PDF', file=sys.stderr)

if __name__ == '__main__':
    main()
