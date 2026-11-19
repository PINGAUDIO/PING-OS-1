"""Worst-case stack depth of the PING FM1 firmware, from its disassembly (app/build/<name>.dis, written by fm1-base's
build). The JieLi compiler does not report frame sizes, so they are read off each function's code:
  [--sp] = {rets, r15-r4}     a push: 4 bytes a register
  sp += -900                  a frame: every negative adjustment in the function is counted (an upper bound)
then the call graph (call / tail goto to another function's label) is walked from each root for its deepest path.
Indirect calls (call rN) cannot be followed: each is listed with its function, and given INDIRECT bytes as a guess.
    python tools/stack_depth.py ["app/build/ping fm1.dis"]
Stacks (device/app.ld): the main loop runs on the user stack (_ustack, 24064 B), interrupts on the system stack
(_sstack, 7936 B); the audio render runs in the audio interrupt."""
import os, re, sys
from functools import lru_cache

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'app', 'build', 'ping fm1.dis')
INDIRECT = 256

label = re.compile(r'^([A-Za-z_$][\w$.]*):\s*$')
push = re.compile(r'\[--sp\] = \{?([^}]*)\}?')
spadj = re.compile(r'\bsp \+= (-?\d+)')
callre = re.compile(r'\bcall [^<]*<([A-Za-z_$][\w$.]*)(?:\+0x[0-9A-Fa-f]+)? :')
gotore = re.compile(r'\bgoto [^<]*<([A-Za-z_$][\w$.]*) :')       # a jump to a label's start: a tail call
indcall = re.compile(r'\bcall (r\d+|\[)')

funcs, cur = {}, None
for line in open(DIS, encoding='utf-8', errors='replace'):
    m = label.match(line)
    if m:
        cur = m.group(1)
        funcs[cur] = {'push': 0, 'frame': 0, 'calls': set(), 'ind': 0, 'first': True}
        continue
    if cur is None:
        continue
    f = funcs[cur]
    m = push.search(line)
    if m:                                      # every push counts (an ISR saves twice; an upper bound elsewhere)
        regs = 0
        for part in m.group(1).split(','):
            part = part.strip()
            r = re.match(r'r(\d+)-r(\d+)', part)
            regs += abs(int(r.group(1)) - int(r.group(2))) + 1 if r else (1 if part else 0)
        f['push'] += 4 * regs
    m = spadj.search(line)
    if m and int(m.group(1)) < 0:
        f['frame'] += -int(m.group(1))
    m = callre.search(line)
    if m:
        f['calls'].add(m.group(1))
    m = gotore.search(line)
    if m and m.group(1) != cur and not m.group(1).startswith('.'):
        f['calls'].add(m.group(1))
    if indcall.search(line):
        f['ind'] += 1

# the function pointers in the image that matter: Felucca's engines behind ch_fe.c
TARGETS = {'ch_fe_render': ['wheel_render', 'phase_render', 'trio_render', 'lofi_render', 'fm6_render'],
           'ch_fe_note_on': ['wheel_note_on', 'phase_note_on', 'trio_note_on', 'lofi_note_on', 'fm6_note_on']}
for n, ts in TARGETS.items():
    if n in funcs:
        funcs[n]['calls'].update(t for t in ts if t in funcs); funcs[n]['ind'] = 0

def own(n):
    f = funcs[n]
    return f['push'] + f['frame']

@lru_cache(None)
def deep(n, guard=()):
    """(bytes, path) of the deepest chain from n"""
    if n not in funcs or n in guard:
        return 0, ()
    best, path = 0, ()
    for c in funcs[n]['calls']:
        d, p = deep(c, guard + (n,))
        if d > best: best, path = d, p
    if funcs[n]['ind'] and INDIRECT > best:
        best, path = INDIRECT, ('(indirect call: %d B guessed)' % INDIRECT,)
    return own(n) + best, (n,) + path

def report(root, stack_name, size):
    if root not in funcs:
        print('%-22s not in the image' % root); return 0
    d, p = deep(root)
    print('%-22s %6d B of %s %d B (%3.0f%%)' % (root, d, stack_name, size, 100.0 * d / size))
    for n in p:
        print('      %6d  %s' % (own(n) if n in funcs else INDIRECT, n))
    return d

print('functions read: %d\n' % len(funcs))
print('deepest single frames:')
for n in sorted(funcs, key=own, reverse=True)[:12]:
    print('      %6d  %s' % (own(n), n))
print()
for root in ('app_render', 'app_frame', 'app_init', 'app_ui_init'):
    report(root, 'stack', 0) if False else None
print('--- the system stack (interrupts) ---')
isrs = sorted(n for n in funcs if n.startswith('isr_'))
a = report('isr_alnk0', 'system stack', 7936)
others = [(deep(n)[0], n) for n in isrs if n != 'isr_alnk0']
for d, n in sorted(others, reverse=True): print('%-22s %6d B' % (n, d))
worst = a + sum(d for d, n in others)
print('worst case, every other interrupt nested on top of the audio: %d B of 7936 B (%.0f%%), %d B spare'
      % (worst, 100.0 * worst / 7936, 7936 - worst))
print('--- the second core (its jobs: fm1_cpu1.S, 2816 B less a 256 B guard) ---')
report('job_tracks', 'CPU1 stack', 2816 - 256)
print()
print('--- the user stack (main loop) ---')
m = report('app_frame', 'user stack', 24064)
report('app_init', 'user stack', 24064)
print('\nindirect calls (not followed):')
for n, f in funcs.items():
    if f['ind'] and (n.startswith(('ch_', 'app_', 'fm6', 'logo', 'lg_')) or 'render' in n):
        print('      %s: %d' % (n, f['ind']))
