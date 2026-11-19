"""The PING OS-1 manual, from one source: README.md at the repo root (GitHub flavoured), docs/manual/index.html (the
standalone page, Jost embedded) and docs/manual/PING-OS-1-Manual.pdf (the page printed by Edge or Chrome headless).
Images from docs/img (the screens: sim captures by dev/out/shots/caps.txt; the panel: docs/draw_panel.py).
    python docs/build_manual.py            builds all three (the PDF when a browser is found)"""
import html, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMG = ROOT / 'docs' / 'img'
OUT = ROOT / 'docs' / 'manual'
VERSION = '1.0.0'

# ---------------------------------------------------------------- the document
# blocks: ('h1'|'h2'|'h3', text) ('p', text) ('shots', [(file, caption), ...]) ('panel', file) ('table', [cols], [rows])
# ('note', text) ('ul'|'ol', [items]).  Inline: `code` for a control's name, **bold**.
D = []
def h1(t): D.append(('h1', t))
def h2(t): D.append(('h2', t))
def h3(t): D.append(('h3', t))
def p(t): D.append(('p', t))
def shots(*items): D.append(('shots', list(items)))
def table(cols, rows): D.append(('table', cols, rows))
def note(t): D.append(('note', t))
def ul(items): D.append(('ul', items))
def ol(items): D.append(('ol', items))

h1('PING OS-1')
p('A four-track rhythm synthesis engine for the M-VAVE FM-1, by PING AUDIO (ping.audio). Version ' + VERSION + '.')
ul(['**69 sound models**: 49 drums and 20 synths, plus a supplemental sampler and granular engine',
    '**23 effects**, four on every track and four on the master, each drawn live on the screen',
    '**A step sequencer** with eight note lanes a track, parameter locks, ratchets, chance, ties and micro timing',
    '**A line generator and an autopilot**: one writes bass lines and melodies in key, the other keeps varying a playing pattern',
    '**Key lock**: one key and scale for every track',
    '**Four modulators** a track and four macros',
    '**16 patterns** a project, chained in song mode, and 16 sound presets',
    '**Performance mode**, a 3D visualizer and a custom theme colour',
    '**A VST3 plugin** with the same engine, and a browser page to try it, install it, load samples and record'])

h2('Quick start')
ol(['**Install.** Connect the FM-1 by USB, open the PING OS-1 page in Chrome or Edge, accept the terms and press Install.',
    '**Play.** Press `PLAY`. The factory project starts: a drum kit and three synths at 130 BPM, with three patterns to try (hold `HOME` and press white key 1, 3 or 4).',
    '**Pick a track.** Turn `PRESETS`, or press black keys 1 to 4. The top line shows T1 to T4.',
    '**Add steps.** The sixteen white keys are the steps of the chosen track. Tap one to set it, tap again to clear it.',
    '**Shape the sound.** Press `EDIT`. `K1` to `K4` are the four knobs on the screen and `SELECT` turns the pages.',
    '**Change the sound.** Press `SEL` for the BROWSER, turn `SELECT` through every engine and model while it plays, and press `SAVE` to keep one.',
    '**Add an effect.** Press `FX`, turn `ALGO` to choose one, and set it with `K1` to `K4`.',
    '**Write notes.** Press `SEQ` for the STEP view. Black keys 1 to 8 choose a note lane, white keys set steps on it.',
    '**Mix.** Press `GLO`. The first pages are level, pan and filter for the four tracks.',
    '**Save.** Hold `SAVE` until the screen says SAVED. A tap on `SAVE` opens the LIBRARY instead.',
    '**Lost?** Hold `GLO` and press `SAVE` for HELP: press any control and the screen says what it does.'])

h2('How it fits together')
p('A **track** is one sound engine, then four effect slots, then its level, pan and filter in the mix. Each track has its own pattern, four modulators and sends. '
  'The four tracks meet in the **master chain**: four more effects and a limiter.')
p('A **pattern** is the steps of all four tracks. A **project** holds sixteen patterns, a song that chains them, sixteen sound presets, the mix and the master chain.')
p('Each button opens one screen. Every screen uses the same four knobs, `K1` to `K4`, for the four tiles it shows, and `SELECT` for its pages.')
table(['Press', 'Screen', 'What it is for'], [
    ['`HOME`', 'TRACKS', 'All four patterns at once. The white keys are steps.'],
    ['`SEQ`', 'STEP', 'One track in detail: eight note lanes, per-step settings, locks.'],
    ['`EDIT`', 'EDIT', 'The engine of the track: its sound.'],
    ['`SEL`', 'BROWSER', 'Choose an engine, model, preset or effect by ear.'],
    ['`FX`', 'FX 1 to 4', 'The four effect slots of the track.'],
    ['`LFO`', 'MOD', 'The four modulators of the track.'],
    ['`ARP`', 'LINE', 'The line generator: writes a bass line or melody.'],
    ['`GLO`', 'Global pages', 'Key, tempo, mix, sends, song, autopilot, setup, display.'],
    ['`GLO` + `FX`', 'MASTER', 'The master effects.'],
    ['`HOME` + `FX`', 'PERFORM', 'The master effects on four knobs, for playing live.'],
    ['`GLO` + `HOME`', 'SPECTRUM', 'A 3D picture of the sound.'],
    ['`SAVE`', 'LIBRARY', 'Sounds, patterns and the project, saved and loaded. Hold `SAVE` to save the project at once.'],
    ['`OCT-` + `OCT+`', 'MACROS', 'Four knobs that each move several settings.'],
    ['`GLO` + `EDIT`', 'LOAD', 'How hard the processor is working.'],
    ['`GLO` + `SAVE`', 'HELP', 'What every control does.'],
])

h2('The panel')
D.append(('panel', 'panel.png'))
table(['Control', 'What it does'], [
    ['`SELECT`', 'Turns the pages. In the sequencer, moves the step cursor.'],
    ['`ALGORITHM`', 'Chooses the engine on EDIT, the effect in an FX slot, the track in TRACKS. Written `ALGO` below.'],
    ['`PRESETS`', 'Chooses the track, T1 to T4.'],
    ['`KNOB 1` to `KNOB 4`', 'The four tiles on the screen, left to right. Written `K1` to `K4` below.'],
    ['`MASTER`', 'Volume.'],
    ['`SEQ`', 'Also the shift key: held with a knob it changes that knob\'s mode.'],
    ['`ENV`', 'Tap: the keys play notes instead of steps. Held: white keys 1 to 4 mute a track, black keys 1 to 4 solo it.'],
    ['`REC`', 'Live record. Held with steps: parameter locks. Held and let go alone: the metronome.'],
    ['`OCT-` `OCT+`', 'The keyboard an octave down or up.'],
    ['Keys', 'White keys are steps, or notes after a tap on `ENV`. C4 is the sound as tuned.'],
])
p('Every knob follows how fast it is turned: slowly, a click is 1 % for exact values; quickly, a half turn crosses the whole range.')

h2('The screen')
shots(('edit_synth.png', 'The EDIT page of a synth track.'))
p('Every page has the same frame.')
ul(['**Top line:** the track, the pattern, the tempo, play or stop, the output meter and the battery.',
    '**Slot row:** the engine and the four effect slots, the open one lit, and the page number on the right.',
    '**Tiles:** the four settings on `K1` to `K4`, each with its name and value. A teal bar over a tile shows a modulator moving it.',
    '**A word under a value** is a mode. Hold `SEQ` and turn that knob to change it.',
    '**Bottom:** the four tracks with their steps and the playhead.'])

h2('TRACKS')
shots(('tracks.png', 'TRACKS: the four patterns.'), ('tracks_play.png', 'Playing.'))
p('`HOME` shows all four tracks, a row each, with every lane merged into one row of steps.')
ul(['White keys set and clear steps on the chosen track.',
    'Black keys 1 to 4, `ALGO` or `PRESETS` choose the track.',
    'Hold a step and turn `K1` to `K4` for its VELOCITY, RATCHET, CHANCE and MICRO timing, as in the STEP view.',
    'Hold `HOME` and press a white key to change pattern.',
    'Hold `SEQ` and turn `ALGO` for the tempo.'])
p('How steps are drawn: the bar is as tall as the velocity and as wide as the gate. A ratchet cuts the bar into its hits. A step fades with its CHANCE. Tied steps are chequered. A step with a lock carries a dot.')

h2('The STEP view')
shots(('step.png', 'The STEP view of a KIT: a drum on each lane.'), ('step_tie.png', 'A tie: chequered after its first step.'))
p('`SEQ` opens one track: eight note lanes, sixteen steps a page, up to four pages (64 steps). On a synth the lanes are the notes of the track\'s scale. On a KIT each lane is a drum.')
table(['Do this', 'To get this'], [
    ['Black keys 1 to 8', 'Choose a lane and hear it.'],
    ['White keys', 'Set or clear a step on that lane.'],
    ['Hold a step, turn `K1` to `K4`', 'VELOCITY, RATCHET (1 to 4 hits, and four rolls), CHANCE, MICRO timing (up to half a step early or late).'],
    ['`EDIT` + `OCT+`', 'The second page of step knobs: OCTAVE, SEND (this hit alone into the master effects; on a KIT the whole kit on that step, so every drum on it is striped), CYCLE (plays on pass A of every B), GATE (note length). `EDIT` + `OCT-` goes back.'],
    ['Hold a step, press black keys', 'A chord: those lanes on the same step.'],
    ['Hold a step a moment, tap a later one', 'A tie: one long note.'],
    ['`SEL` held on a step + `SELECT` or `ALGO`', 'Move the step along its lane, or to another lane.'],
    ['`SEQ` + `K1` to `K4`', 'The track\'s LENGTH, RATE, DIRECTION (forward, reverse, pendulum, random) and SWING.'],
    ['`SEQ` + `SELECT`', 'The page of steps.'],
    ['`SEQ` + `ALGO`', 'The track\'s scale.'],
    ['`EDIT` + `ALGO`, `EDIT` + `SELECT`', 'The track\'s own key and scale, over the master key.'],
    ['`EDIT` + `PRESETS`', 'All lanes up or down a semitone, or a scale step.'],
    ['`OCT-` `OCT+`', 'All lanes an octave.'],
    ['`SEQ` + `EDIT`', 'LANE NOTES: set each lane\'s note by hand.'],
    ['`SEQ` + white 12 to 16', 'Generators: a melody, a beat, random locks, random lanes, a variation.'],
    ['`SEQ` + `REC`', 'Clear the track.'],
    ['`EDIT` + `REC`', 'Undo the last generator, clear or shift. Again: redo.'],
])
h3('Live recording')
p('Tap `ENV` so the keys play notes, press `REC`, then `PLAY`. A bar is counted in and what you play is written at the playhead. '
  'The SETUP page under `GLO` sets the count-in (off, one bar, two bars) and MICRO: on, each note keeps the exact timing you played inside its step.')
h3('Parameter locks')
shots(('step_lock.png', 'Two steps armed.'), ('edit_lock.png', 'A knob turned now is locked on those steps.'))
p('A lock is a knob setting that belongs to one step. Hold `REC` and press steps to arm them, then turn any knob on an EDIT or FX page: it changes on those steps only. '
  'The model can be locked too, so one track can be a kick, a snare and a hat. Tap `REC` to finish. Thirty-two locks a track a pattern.')

h2('EDIT: the sound')
shots(('edit_kit.png', 'A KIT: part 1, the kick.'), ('edit_synth_p2.png', 'ANALOG, the filter page.'))
p('`EDIT` opens the engine of the track. `ALGO` chooses the engine, `K1` is usually the model, `SELECT` turns the pages. Hold `EDIT` to switch the engine off and on.')
table(['Engine', 'What it is'], [
    ['KIT', 'Eight drums on one track, one on each lane. Each part has its own model, pitch, decay and level. The closed hat chokes the open one.'],
    ['DRUM', 'One of 49 drum models, played across the lanes as notes.'],
    ['SYNTH', 'One of 20 synth models, from a full subtractive synth to FM, strings, bells and an acid bass.'],
    ['SAMPLE', 'One of the eight samples you loaded, with start, length, loop, envelope and filter.'],
    ['GRAIN', 'A sample played as a cloud of grains while a note is held: position, size, density, spray.'],
])
p('Every model and effect is listed under Models and effects, near the end.')
p('Choosing a model sets every knob to a sound that works. Most synth models have three knobs of their own, then a shared filter (CUTOFF, RES, ENV>FILT), envelopes, PITCH, GLIDE, LEVEL and VELOCITY.')
ul(['**MODE** is how a note ends: GATE (held as long as the key or step, the default), PING (rings out) or DRONE (never stops).',
    '**VOICES** is how many notes sound at once: 4, 2, 1 or LEGATO. Fewer voices leave more processor time.',
    '**KEY TRACK:** hold `SEQ` and turn CUTOFF to make the filter follow the note: OFF, HALF or FULL.',
    '**PITCH** moves in semitones, in scale steps, or freely: hold `SEQ` and turn it to choose.'])
h3('ANALOG')
p('A full subtractive synth on eight pages. Two oscillators whose SHAPE runs from sine through triangle, saw and square to pulse; a sine SUB; NOISE; a ladder filter with four types (LP 24, LP 12, BAND, HIGH); '
  'two ADSR envelopes, one for level and one for the filter; SYNC; and COLOR, one knob from exact and clean to worn and driven.')
h3('ACID and WOBBLE')
p('ACID is a one-voice acid bass: notes over velocity 100 are accented and a note tied into the next one slides. '
  'WOBBLE is ANALOG with a filter sweep locked to the tempo: RATE, DEPTH and SHAPE set the sweep.')
h3('Layouts')
shots(('layout_mixer.png', 'MIXER.'), ('layout_panel.png', 'PANEL: every page at once.'))
p('Tap `EDIT` again to go round three layouts of the page: the knobs over the four tracks; MIXER, tall meters with the track levels on `K1` to `K4` and the master effects below; and PANEL, every page of the engine at once.')
h3('The envelope display')
shots(('envelope.png', 'SEQ + ENV: the amp envelope.'))
p('`SEQ` + `ENV` draws the envelope under the knobs, with its stages on `K1` to `K4`. On a synth, the same again shows the filter envelope; once more closes it.')

h2('The BROWSER')
shots(('browser.png', 'Engines on the left, models on the right.'))
p('`SEL` opens a full-screen list of what the track can hold. The line you are on plays at once, so you choose by ear.')
ul(['`SELECT` runs through every model of every engine. `ALGO` jumps between engines.',
    '`EDIT` switches to the PRESETS tab, your sixteen saved sounds.',
    '`SAVE` keeps the choice. `SEL` or `HOME` closes the page and puts the track back as it was.',
    'Opened from an FX slot, it lists the effects instead.'])

h2('Effects')
shots(('fx_empty.png', 'An empty slot.'), ('fx_delay.png', 'DELAY, with its taps drawn.'), ('fx_delay_p2.png', 'REVERB.'))
p('`FX` steps through the four slots of the track. `ALGO` chooses the effect, `SELECT` turns its pages, and holding `FX` bypasses the slot. '
  'They run in order, 1 to 4. Any effect can go in any slot, and each one draws what it is doing.')
p('All 23 are listed, with a line on each, under Models and effects below.')
p('Delays and reverbs share a pool of memory. If it runs out, a slot says NO MEMORY and passes the sound dry.')

h2('Modulation')
shots(('mod.png', 'MOD 1, an LFO.'), ('macros.png', 'The four macros.'))
p('`LFO` opens the modulators of the track. There are four; hold `LFO` and press white keys 1 to 4 to choose one. Each is an LFO or a TRIG RND, a new random value on every note.')
ul(['**To map one:** hold `LFO` and turn any knob on any page. How far you turn is the depth, up or down.',
    '**LFO settings:** RATE (free or synced), AMOUNT, SHAPE, OFFSET, then PHASE, SKEW, SMOOTH and SPREAD.',
    '**Polarity:** hold `SEQ` and turn AMOUNT for BIPOLAR (both ways) or UNIPOLAR (one way), on an LFO or a TRIG RND.',
    '**To clear:** `LFO` + `REC` clears every mapping of the chosen modulator.',
    'Track level, pan and mix filter can be mapped too, from their `GLO` pages.'])
h3('Macros')
p('Four macros are shared by all tracks. Press `OCT-` and `OCT+` together and `K1` to `K4` become the macros; press both again to get the knobs back. '
  'To assign one, hold both OCT buttons and press white key 1 to 4, let go, turn the knobs it should move, then press both again.')

h2('The line generator')
shots(('line.png', 'LINE: the generator and the line it drew.'))
p('`ARP` opens the generator. A second tap writes a line into the chosen track, in its key; every tap after that writes a new one. Hold `ARP` from any other screen to write one without leaving it.')
ul(['**TYPE** runs from all random notes to nearly all root notes. **DENSITY** is how many steps play.',
    '**ACCENT** and **GATE** set how hard the accents hit and how long the notes are.',
    'The knobs reshape the line that is there; `ARP` draws a fresh one.',
    'White keys 1 to 4 choose which layers are redrawn: notes, accents, gates, octaves.',
    '`EDIT` + `REC` undoes the last line.'])

h2('Global pages')
shots(('key_tempo.png', 'KEY / TEMPO, with the list of pages below.'), ('mix.png', 'MIX LEVEL.'), ('glo_record.png', 'SETUP.'))
p('`GLO` opens ten pages; `SELECT` turns them. The list under the knobs shows every page, with the open one lit.')
table(['Page', 'K1 to K4'], [
    ['KEY / TEMPO', 'The master key and scale, TEMPO and SWING. Tap tempo: hold `GLO` and tap `PLAY`.'],
    ['MIX LEVEL', 'The level of each track. `ALGO` is the return level of everything sent to the master effects.'],
    ['MIX PAN', 'The pan of each track.'],
    ['MIX FILTER', 'A one-knob filter on each track.'],
    ['SENDS', 'The chosen track into the master effects (FX SEND), and into each of the other three tracks.'],
    ['SEND TO', 'Where in the master chain each track\'s send goes in.'],
    ['SONG', 'Chains patterns: an entry, its pattern, how many bars, and song mode on or off.'],
    ['AUTOPILOT', 'Varies the sounds and patterns by itself. See below.'],
    ['SETUP', 'MICRO (recorded notes keep their exact timing), COUNT IN, the master LIMITER on or off, and HOLD FX, which of the four master effects `HOME` + `K3` brings in. All four are kept over a power-off.'],
    ['DISPLAY', 'Colours, the PANEL row style and MIDI SYNC.'],
])
p('Mute and solo work from anywhere: hold `ENV` and press white keys 1 to 4 to mute, black keys 1 to 4 to solo.')

h2('Master, PERFORM and SPECTRUM')
shots(('master.png', 'The master chain.'), ('perform.png', 'PERFORM.'), ('scope.png', 'The spectrum.'))
ul(['**MASTER** (`GLO` + `FX`): four effect slots on the whole mix, then a limiter. It starts with DJ FILTER, REPEAT, TRANCE and REVERB.',
    '**PERFORM** (`HOME` + `FX`): the main amount of each master effect on `K1` to `K4`. Hold `HOME` while you turn and the knobs spring back when you let go.',
    '**Stutter:** while playing, hold `HOME` and turn `K1` to repeat the current step on every track, or `K2` for the chosen track only. More turns set the rate.',
    '**Hold FX:** hold `HOME` and turn `K3` to bring one master effect in, as far as you turn; let go of `HOME` and it drops back to where it was. HOLD FX on the SETUP page under `GLO` chooses which effect.',
    '**SPECTRUM** (`GLO` + `HOME`): the sound as a moving 3D landscape. `K1` chooses the master or one track, `K2` turns the view.'])

h2('Patterns, song and presets')
shots(('library.png', 'The LIBRARY.'), ('song.png', 'The SONG page.'))
ul(['**Patterns:** hold `HOME` and press a white key to go to one of sixteen. It changes at the next bar. With `SEQ` held too, the playing pattern is copied there.',
    '**Song:** the SONG page under `GLO` plays patterns in order, each for a number of bars.',
    '**Sound presets:** hold `SAVE` and press a white key to store the sound of the track; hold `EDIT` and press the key to load it.',
    '**Project:** hold `SAVE` for most of a second to write everything. It comes back at power-on.',
    '**LIBRARY** (a tap on `SAVE`, or `SAVE` + `EDIT`): sounds, patterns and the project on one page. `EDIT` loads, `SAVE` stores, and overwriting asks twice. `REC` starts over, the selected track on SOUNDS, every track and the master effects on PROJECT: a box asks first, `REC` again is yes, any other button cancels.',
    '**Start again:** `SEQ` + `SAVE`, then `SAVE` to reset the chosen track, or `REC` to reset everything: plain sounds, empty sequences, and no effects on the tracks or the master. Nothing saved is touched until you save.'])

h2('Autopilot')
p('AUTOPILOT, under `GLO`, keeps changing a playing pattern for you. EVERY sets how often, from 1 to 16 bars. VARIETY sets how much: low values nudge a few knobs, high values redraw lines and swap models. '
  'WHEN OFF chooses what happens when you stop it: REVERT puts everything back as you had it, OVERRIDE keeps what is playing. It never touches the mix, mutes, tempo or the master chain.')

h2('LOAD and voices')
shots(('load.png', 'LOAD.'), ('help.png', 'HELP.'))
p('About eight voices can sound at once. Past that, the least important one fades out; nothing is ever cut with a click. '
  'LOAD (`GLO` + `EDIT`) shows what each track and effect costs and whether any audio was late (XR). If the sound breaks up, lower VOICES on a synth track or remove an effect. '
  'On LOAD, `K1` sets one or two processor cores, `K2` the limiter (the same switch as on SETUP), and `K4` pages through the cost of every model and effect.')

h2('MIDI sync')
p('On the DISPLAY page, `K4` is MIDI SYNC.')
table(['Setting', 'What the FM-1 does'], [
    ['OFF', 'Plays on its own. Start and stop from outside still work.'],
    ['SEND', 'Leads: sends clock, start and stop over USB, through a computer.'],
    ['SEND DIRECT', 'Leads one other FM-1 plugged straight into its USB port, through a powered hub or adapter.'],
    ['FOLLOW', 'Follows the tempo and position of the clock coming in.'],
])

h2('Samples, MIDI and the plugin')
p('The Transfer tab of the PING OS-1 page works with the FM-1 on USB.')
ul(['**Samples:** drop up to eight WAV files, name them and send. The bank holds about 4.6 seconds at 32 kHz or 9.2 at 16 kHz. SAMPLE and GRAIN play them.',
    '**Record:** the FM-1 is a USB audio input, and the page records it to a WAV.',
    '**Pattern as MIDI:** saves the playing pattern as a MIDI file, a track on each of channels 1 to 4.',
    '**Project for the plugin:** saves the whole project for the PING OS-1 VST3 plugin, which plays it in a DAW exactly as the FM-1 does.'])

h2('Install and uninstall')
p('Connect the FM-1 by USB, open the PING OS-1 page in Chrome or Edge, accept the terms, open Install and press Install. Do not unplug while it writes. '
  'The same page puts the stock firmware back. Nothing is sent to any server.')
p('**Back up first.** PING OS-1 keeps its projects, patterns, presets and samples in the memory of the FM-1, in its own format. It does not read what another firmware saved there, and switching firmware in either direction can overwrite what the other one stored. '
  'Before you switch, back up with the firmware you are on. From PING OS-1, the Transfer tab saves the project as a file; that file opens in the plugin and cannot yet be sent back to the FM-1.')

h2('Models and effects')
p('Everything a track can hold. On a drum, the two knobs after DECAY change name with the model; they are given in brackets.')
h3('Engines (5)')
table(['Engine', 'What it is'], [
    ['KIT', 'Eight drums on one track, one on each lane, each with its own model, pitch, decay and level.'],
    ['DRUM', 'One drum model, played across the lanes as notes.'],
    ['SYNTH', 'One synth model, up to four notes at once.'],
    ['SAMPLE', 'One of your eight loaded samples, with start, length, loop, envelope and filter.'],
    ['GRAIN', 'A loaded sample played as a cloud of grains while a note is held.'],
])
h3('Synth models (20)')
table(['Model', 'What it is'], [
    ['ANALOG', 'A two-oscillator subtractive synth: shapes from sine to pulse, sub, noise, a four-mode ladder filter, two ADSR envelopes.'],
    ['FOLD', 'A wavefolder: a simple wave folded back on itself for bright, buzzy tones (SHAPE, FOLD, ASYM).'],
    ['FM2', 'Two-operator FM with feedback: bells, basses, electric pianos (RATIO, INDEX, FEEDBACK).'],
    ['VOICE', 'A vowel synth that sings A E I O U (VOWEL, SHIFT, BREATH).'],
    ['ADDITIVE', 'Eight harmonics mixed by hand: where they centre, how wide, odd against even.'],
    ['WAVES', 'A morphing waveform with bit crush and smoothing (CRUSH, WAVE, SMOOTH).'],
    ['CHORD', 'A whole chord from one note: eleven chords, inversions, organ or string-machine tone.'],
    ['SWARM', 'A stack of detuned saws, the supersaw (DETUNE, BRIGHT, JITTER).'],
    ['STRING', 'A plucked or bowed string, from sitar buzz to piano stiffness (DAMP, BRIGHT, STRUCTURE).'],
    ['MODAL', 'A struck or bowed resonator, from wood to bell (MATERIAL, BRIGHT, DAMP).'],
    ['BUBBLE', 'Bubbles in water: one drop a note, or a stream from a drip to a fizz (SPREAD, DENSITY, RING).'],
    ['ACID', 'A one-voice acid bass with accent and slide (WAVE, ACCENT, DRIVE).'],
    ['NOISE', 'Tuned, filtered noise for wind, hiss and sweeps (RESO, COLOR, filter type).'],
    ['RIPPLE', 'A wave-warping synth: one sine bent, folded and re-triggered from the inside (SYNC, FOLD, BEND), with AM and jitter.'],
    ['WOBBLE', 'The ANALOG voice with a filter sweep locked to the tempo (RATE, DEPTH, SHAPE).'],
    ['ORGAN', 'A tonewheel organ.'],
    ['PHASE', 'A phase distortion synth: glassy digital basses and leads.'],
    ['TRIO', 'Three oscillators with ring modulation and sync into a multimode filter.'],
    ['LOFI', 'A chip voice: pulse, triangle, saw, noise and tiny wavetables.'],
    ['FM6', 'Six-operator FM with eight patches: electric piano, bell, bass, brass, pad, marimba, organ, pluck.'],
])
h3('Drum models (49)')
table(['Kicks (10)', 'What it is'], [
    ['KICK 808', 'The long round analogue kick (SWEEP, HARMONIC).'],
    ['KICK 909', 'Punchy and clicky, with a driven body (ATTACK, WAVE).'],
    ['KICK FM', 'An FM kick, from soft to metallic (RATIO, INDEX).'],
    ['KICK HARD', 'Folded and gritty, for hard styles (FOLD, GRIT).'],
    ['KICK 606', 'Short and light (CLICK, TONE).'],
    ['KICK DEEP', 'Low and long with a soft punch (PUNCH, WARMTH).'],
    ['KICK SKIN', 'An acoustic-style kick: a beater on a head (BEATER, DAMP).'],
    ['KICK DUB', 'Dark with a rumbling tail (RUMBLE, DARK).'],
    ['KICK CLICK', 'A clean kick with a separate tuned click (CLICK TONE, CLICK LEN).'],
    ['808 BOOM', 'The long sub bass drum, playable as a bassline (PUNCH, SATURATE).'],
])
table(['Snares (7)', 'What it is'], [
    ['SNARE 808', 'Two tones and noise (MIX, SNAPPY).'],
    ['SNARE 909', 'Brighter and harder, with shaped noise (SPREAD, NOISE TONE).'],
    ['SNARE FM', 'An FM body under noise (RATIO, NOISE).'],
    ['SNARE MODAL', 'A drum shell modelled from its resonances (MODES, SNARES).'],
    ['SNARE SKIN', 'Two heads with wires that rattle (STRIKE, WIRES).'],
    ['SNARE LOFI', 'A sample-and-hold snare, crunchy (RATE, BITS).'],
    ['SNARE BAND', 'A body with noise through a band you place and widen (BAND, WIDTH).'],
])
table(['Claps and wood (5)', 'What it is'], [
    ['CLAP', 'Several hands and a tail (SPREAD, TAIL).'],
    ['CLAP BAND', 'A clap whose noise sits in a band you choose (BAND, WIDTH).'],
    ['RIM', 'A rim shot (BODY, CLICK).'],
    ['CLAVE', 'A wooden click (WOOD, HARD).'],
    ['COWBELL', 'Two detuned squares (RATIO, DECAY).'],
])
table(['Hats (7)', 'What it is'], [
    ['HAT 808', 'Six squares through a band: the classic metallic hat (METAL, BAND).'],
    ['HAT NOISE', 'Filtered noise (RESO, BAND).'],
    ['HAT 606', 'Thinner and spitting (SPREAD, BAND).'],
    ['HAT RING', 'Ring-modulated squares (RATIO, BAND).'],
    ['HAT FM', 'An FM hat (RATIO, INDEX).'],
    ['HAT CLOCK', 'Noise clocked at an audible rate (RATE, BAND).'],
    ['HAT WIRE', 'A short buzzing string fed with noise (BRIGHT, BAND).'],
])
table(['Cymbals and metal (6)', 'What it is'], [
    ['CYMBAL', 'Metal and noise (METAL, NOISE).'],
    ['CRASH', 'A long wash (METAL, WASH).'],
    ['RIDE', 'A ping with a sizzle and a bell (BELL, SIZZLE).'],
    ['RIDE 808', 'The 808 cymbal voiced long (METAL, BAND).'],
    ['RIDE 909', 'A gritty, low-rate ride (BELL, GRIT).'],
    ['METAL', 'FM for gongs, bells and trashy cymbals (RATIO, SPREAD).'],
])
table(['Skins and shakers (4)', 'What it is'], [
    ['TOM', 'A swept tom (SWEEP, SKIN).'],
    ['CONGA', 'Open, slapped or muted (SLAP, MUTE).'],
    ['MEMBRANE', 'A struck drum head (STRIKE, DAMP).'],
    ['SHAKER', 'Grains in a shell (DENSITY, RESO).'],
])
table(['Tuned (6)', 'What it is'], [
    ['BELL', 'A struck bell or bar (MATERIAL, POSITION).'],
    ['PLUCK', 'A plucked string that can turn into a drum (BRIGHT, DRUM).'],
    ['PING', 'A sine blip with a sub (SWEEP, SUB).'],
    ['IMPULSE', 'A tuned tick: a strike through a comb (HARD, DAMP).'],
    ['FM PERC', 'A pitched FM hit (RATIO, INDEX).'],
    ['FM THUMP', 'A low FM thump (RATIO, FM AMT).'],
])
table(['Noise and digital (4)', 'What it is'], [
    ['NOISE PERC', 'A burst of noise through a swept band (SWEEP, RESO).'],
    ['ZAP', 'A laser sweep (SWEEP, FEEDBACK).'],
    ['BIT', 'A chip hit, tone against noise (NOISE, TONE MIX).'],
    ['GLITCH', 'Digital chaos (CHAOS, RATE).'],
])
h3('Effects (23)')
table(['Effect', 'What it does'], [
    ['FILTER', 'Low, high, band pass or notch, 12 or 24 dB.'],
    ['DJ FILTER', 'One knob: left closes a low pass, right opens a high pass.'],
    ['DISPERSE', 'Smears a hit in time into a chirp or a zap.'],
    ['COMB', 'Rings at a pitch and its harmonics. The pitch can follow the notes of a track.'],
    ['PHASER', '2, 4, 6 or 8 poles, with a choice of sweep shape.'],
    ['FLANGER', 'A swept short delay with feedback.'],
    ['TREMOLO', 'Level sweep; with STEREO, an auto-pan.'],
    ['CHORUS', 'A swept voice on each side.'],
    ['UNISON', 'Two, four or six detuned copies of any sound, spread left and right.'],
    ['DELAY', 'Stereo, synced or free, with ping pong.'],
    ['DUB DELAY', 'A resonant filter in the feedback that can sweep and run away.'],
    ['ECHO', 'Tape echo: dark, saturating repeats.'],
    ['CRYSTAL', 'Pitch-shifted grains fed back, so each repeat climbs.'],
    ['REVERB', 'HALL, ROOM, PLATE or GATED, with DUCK to dip it under the dry sound.'],
    ['SPRING', 'Two springs, left and right.'],
    ['ERODE', 'Grit and air: a tiny delay shaken by noise or a sine.'],
    ['VINYL', 'Crackle, pops and dust.'],
    ['TAPE', 'Wow, saturation and hiss.'],
    ['RESONATOR', 'Four tuned bands that follow the master key.'],
    ['DRIVE', 'Five distortion curves and a bit crusher, with tone, clip and gate.'],
    ['TRANCE', 'A sixteen-step gate in time with the sequencer. While its page is open the white keys are its steps.'],
    ['REPEAT', 'Beat repeat: grabs a slice every bar or beat and repeats it.'],
    ['SIDECHAIN', 'Ducks the track under another track, or under one drum of a kit.'],
])

h2('Shortcuts')
table(['Combination', 'Result'], [
    ['`SEQ` + a knob', 'That knob\'s mode'],
    ['`SEQ` + `ENV`', 'Envelope display'],
    ['`SEQ` + `K1` to `K4` (STEP)', 'Length, rate, direction, swing'],
    ['`SEQ` + `ALGO`', 'Scale. In TRACKS: tempo'],
    ['`SEQ` + `REC`', 'Clear the track (STEP), the pattern (TRACKS), the song (SONG)'],
    ['`SEQ` + `SAVE`', 'INIT: then `SAVE` for the track, `REC` for everything'],
    ['`REC` in the LIBRARY', 'INIT, after a yes: the track on SOUNDS, everything on PROJECT'],
    ['`SEQ` + white 12 to 16', 'Generators'],
    ['`EDIT` + `OCT+` / `OCT-`', 'Step knobs, second page / first page'],
    ['`EDIT` + `REC`', 'Undo, redo'],
    ['`EDIT` + `ALGO`, `SELECT`', 'Track key, track scale'],
    ['`EDIT` + white 1 to 16', 'Load a sound preset'],
    ['`SAVE` + white 1 to 16', 'Store a sound preset'],
    ['`SAVE` tap, hold', 'LIBRARY, save the project'],
    ['`HOME` + white 1 to 16', 'Pattern'],
    ['`HOME` + `SEQ` + white', 'Copy the pattern there'],
    ['`HOME` + `K1`, `K2`', 'Stutter: all tracks, this track'],
    ['`HOME` + `K3`', 'Hold FX: a master effect in while `HOME` is held'],
    ['`HOME` + `FX`', 'PERFORM'],
    ['`HOME` + `ARP`, `SEQ` + `ARP`', 'A new line without leaving TRACKS or STEP'],
    ['`GLO` + `FX`', 'MASTER'],
    ['`GLO` + `HOME`', 'SPECTRUM'],
    ['`GLO` + `EDIT`', 'LOAD'],
    ['`GLO` + `SAVE`', 'HELP'],
    ['`GLO` + `PLAY` taps', 'Tap tempo'],
    ['`LFO` + a knob', 'Map the chosen modulator'],
    ['`LFO` + white 1 to 4', 'Choose the modulator'],
    ['`LFO` + `REC`', 'Clear its mappings'],
    ['`ENV` + white 1 to 4, black 1 to 4', 'Mute, solo'],
    ['`SEL` + `ALGO`', 'Track level'],
    ['`OCT-` + `OCT+`', 'Macros on the knobs'],
    ['`REC` + steps', 'Arm steps for locks'],
    ['`REC` held and let go', 'Metronome'],
    ['Step held + later step', 'Tie'],
    ['Step held + black keys', 'Chord'],
])

h2('Credits and licence')
p('PING OS-1 is open source under the Mozilla Public License 2.0. It is built on fm1-base and felucca-bsp. '
  'FOLD, VOICE, CHORD, STRING and MODAL are ports of Mutable Instruments Plaits; ORGAN, PHASE, TRIO, LOFI and FM6 come from Felucca by Leo Kuroshita, FM6 after msfa. '
  'Full notices are in docs/THIRD_PARTY.md. Jost by indestructible type, SIL Open Font License.')
p('PING OS-1 is not made or endorsed by M-VAVE. It is provided as is, with no warranty; you install it at your own risk.')

BUTTONS = {'FX', 'SEL', 'ENV', 'LFO', 'EDIT', 'GLO', 'HOME', 'SAVE', 'ARP', 'SEQ', 'PLAY', 'REC', 'OCT-', 'OCT+'}
KNOBS = {'SELECT', 'ALGO', 'ALGORITHM', 'PRESETS', 'MASTER', 'KNOB 1', 'KNOB 2', 'KNOB 3', 'KNOB 4', 'K1', 'K2', 'K3', 'K4'}
def control_kind(name):
    return 'btn' if name in BUTTONS else 'knob' if name in KNOBS else None

# ---------------------------------------------------------------- markdown
import re
def bold_html(s): return re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', s)
def md_inline(t):
    out, parts = [], t.split('`')
    for i, s in enumerate(parts):
        if i % 2 and control_kind(s): out.append(f'<kbd>{s}</kbd>')
        elif i % 2: out.append(f'`{s}`')
        else: out.append(s)
    return ''.join(out)
def to_md():
    L = []
    for b in D:
        k = b[0]
        if k == 'h1': L += [f'# {b[1]}', '']
        elif k == 'h2': L += [f'## {b[1]}', '']
        elif k == 'h3': L += [f'### {b[1]}', '']
        elif k == 'p': L += [md_inline(b[1]), '']
        elif k == 'note': L += [f'> {b[1]}', '']
        elif k == 'ul': L += [f'- {md_inline(x)}' for x in b[1]] + ['']
        elif k == 'ol': L += [f'{n}. {md_inline(x)}' for n, x in enumerate(b[1], 1)] + ['']
        elif k == 'panel': L += [f'<img src="docs/img/{b[1]}" width="820" alt="The FM-1 panel">', '']
        elif k == 'shots':
            cells = ''.join(f'<td align="center"><img src="docs/img/{f}" width="240" alt="{html.escape(c)}"><br><sub>{html.escape(c)}</sub></td>' for f, c in b[1])
            L += ['<table><tr>' + cells + '</tr></table>', '']
        elif k == 'table':
            cols, rows = b[1], b[2]
            L += ['| ' + ' | '.join(cols) + ' |', '|' + '---|' * len(cols)]
            L += ['| ' + ' | '.join(md_inline(c) for c in r) + ' |' for r in rows]
            L += ['']
    return '\n'.join(L).rstrip('\n') + '\n'

# ---------------------------------------------------------------- html
CSS = '''
@font-face { font-family: Jost; src: url(Jost.ttf); font-weight: 100 900; }
:root { --ink: #16171a; --mute: #6b6e76; --line: #e3e4e8; --acc: #ff8424; --bg: #fff; }
* { box-sizing: border-box; }
html { font-size: 15px; }
body { margin: 0; background: var(--bg); color: var(--ink); font-family: Jost, "Segoe UI", system-ui, sans-serif; font-weight: 400; line-height: 1.75; }
main { max-width: 860px; margin: 0 auto; padding: 56px 32px 80px; }
h1 { font-size: 46px; font-weight: 700; letter-spacing: .02em; margin: 0 0 4px; }
h1 + p { color: var(--mute); margin-top: 0; font-weight: 500; }
h2 { font-size: 24px; font-weight: 600; margin: 48px 0 12px; padding-top: 14px; border-top: 1px solid var(--line); letter-spacing: .01em; }
h3 { font-size: 17px; font-weight: 600; margin: 24px 0 8px; }
p { margin: 0 0 12px; max-width: 70ch; }
ul, ol { margin: 0 0 14px; padding-left: 22px; max-width: 70ch; }
li { margin: 0 0 5px; }
strong { font-weight: 600; }
code { font-family: Jost, sans-serif; font-weight: 600; font-size: .92em; padding: 1px 6px; border: 1px solid var(--line); border-radius: 5px; background: #f6f6f8; white-space: nowrap; }
kbd { font-family: Jost, sans-serif; font-weight: 600; font-size: .72em; letter-spacing: .06em; white-space: nowrap; display: inline-block; line-height: 1.45; vertical-align: 1px; margin: 0 1px; -webkit-print-color-adjust: exact; print-color-adjust: exact; }
kbd.btn { padding: 0 7px; border-radius: 5px; background: #2b2d32; color: #e9e9ec; border: 1px solid #141517; }
kbd.knob { padding: 0 8px 0 6px; border-radius: 999px; background: #fff; color: #2b2d32; border: 1px solid #2b2d32; }
kbd.knob::before { content: ''; display: inline-block; width: 6px; height: 6px; border-radius: 50%; background: #2b2d32; margin-right: 5px; vertical-align: 1px; }
td kbd { font-size: .72em; }
td { line-height: 1.6; }
table { border-collapse: collapse; width: 100%; margin: 8px 0 16px; font-size: 14px; }
th { text-align: left; font-weight: 600; color: var(--mute); font-size: 12px; letter-spacing: .08em; text-transform: uppercase; padding: 6px 10px 6px 0; border-bottom: 1px solid var(--line); }
td { padding: 7px 10px 7px 0; border-bottom: 1px solid var(--line); vertical-align: top; }
td:first-child { width: 30%; padding-right: 16px; }
figure { margin: 10px 0 18px; display: flex; gap: 22px; flex-wrap: wrap; }
figure > div { flex: 0 0 auto; width: 240px; }
figure img { width: 240px; height: 240px; display: block; border-radius: 10px; image-rendering: pixelated; }
figure figcaption { color: var(--mute); font-size: 13px; margin-top: 6px; }
.panel img { width: 100%; max-width: 820px; display: block; border-radius: 16px; margin: 8px 0 16px; }
.foot { color: var(--mute); font-size: 13px; margin-top: 40px; }
@page { size: A4; margin: 18mm 16mm; }
@media print {
  html { font-size: 11.5px; }
  main { max-width: none; padding: 0; }
  h2, h3 { break-after: avoid; }
  h2 { margin-top: 28px; }
  figure, tr { break-inside: avoid; }
  thead { display: table-header-group; }
  p { orphans: 3; widows: 3; }
  figure img { width: 180px; height: 180px; }
  figure > div { width: 180px; }
  a { color: inherit; text-decoration: none; }
}
'''
def inline_html(t):
    out, parts = [], t.split('`')
    for i, s in enumerate(parts):
        kind = control_kind(s) if i % 2 else None
        s = html.escape(s)
        out.append(f'<kbd class="{kind}">{s}</kbd>' if kind else f'<code>{s}</code>' if i % 2 else bold_html(s))
    return ''.join(out)
def to_html():
    L = ['<!doctype html>', '<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
         f'<title>PING OS-1 Manual</title><style>{CSS}</style></head><body><main>']
    first_h2 = True
    for b in D:
        k = b[0]
        if k == 'h1': L.append(f'<h1>{html.escape(b[1])}</h1>')
        elif k == 'h2': L.append(f'<h2>{html.escape(b[1])}</h2>')
        elif k == 'h3': L.append(f'<h3>{html.escape(b[1])}</h3>')
        elif k == 'p': L.append(f'<p>{inline_html(b[1])}</p>')
        elif k == 'note': L.append(f'<p class="foot">{inline_html(b[1])}</p>')
        elif k in ('ul', 'ol'): L.append(f'<{k}>' + ''.join(f'<li>{inline_html(x)}</li>' for x in b[1]) + f'</{k}>')
        elif k == 'panel': L.append(f'<div class="panel"><img src="img/{b[1]}" alt="The FM-1 panel"></div>')
        elif k == 'shots':
            L.append('<figure>' + ''.join(f'<div><img src="img/{f}" alt="{html.escape(c)}"><figcaption>{html.escape(c)}</figcaption></div>' for f, c in b[1]) + '</figure>')
        elif k == 'table':
            cols, rows = b[1], b[2]
            L.append('<table><thead><tr>' + ''.join(f'<th>{html.escape(c)}</th>' for c in cols) + '</tr></thead><tbody>')
            for r in rows:
                L.append('<tr>' + ''.join(f'<td>{inline_html(c)}</td>' for c in r) + '</tr>')
            L.append('</tbody></table>')
    L.append('</main></body></html>')
    return '\n'.join(L)

def main():
    (ROOT / 'README.md').write_text(to_md(), encoding='utf-8')
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'img').mkdir(exist_ok=True)
    used = {b[1] for b in D if b[0] == 'panel'} | {f for b in D if b[0] == 'shots' for f, _ in b[1]}
    for f in used:
        shutil.copy(IMG / f, OUT / 'img' / f)
    shutil.copy(ROOT / 'app' / 'assets' / 'fonts' / 'Jost[wght].ttf', OUT / 'Jost.ttf')
    (OUT / 'index.html').write_text(to_html(), encoding='utf-8')
    print(f'README.md and {OUT / "index.html"}')
    for exe in (r'C:\Program Files\Google\Chrome\Application\chrome.exe', r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'):
        if Path(exe).exists():
            pdf = OUT / 'PING-OS-1-Manual.pdf'
            if pdf.exists(): pdf.unlink()
            import tempfile; prof = Path(tempfile.gettempdir()) / 'ping-os-1-manual-print'
            subprocess.run([exe, '--headless=new', '--disable-gpu', f'--user-data-dir={prof}', '--no-pdf-header-footer', f'--print-to-pdf={pdf}',
                            '--allow-file-access-from-files', (OUT / 'index.html').as_uri()], check=False, timeout=120,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if pdf.exists():
                print(f'{pdf}  {pdf.stat().st_size} B'); break
    else:
        print('no browser for the PDF', file=sys.stderr)

if __name__ == '__main__':
    main()
