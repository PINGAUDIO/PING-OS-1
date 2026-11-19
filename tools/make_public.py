"""The public copy of PING OS-1's source: what this repository tracks at HEAD, written over a separate folder with its
own, clean history (default ../ping-os-1-public), which is what goes to GitHub. The working history here stays here.
    python tools/make_public.py [FOLDER]
The folder's .git is kept, so each release is one more commit there; everything else in it is replaced. fm1-base stays
a submodule (base, at the commit this repository has it at). Nothing is committed or pushed: that is done by hand."""
import io, shutil, subprocess, sys, tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / 'ping-os-1-public'

def git(*a, cwd=ROOT, text=True):
    return subprocess.run(['git', *a], cwd=cwd, check=True, capture_output=True, text=text).stdout

if git('status', '--porcelain', '--ignore-submodules=dirty').strip():
    sys.exit('uncommitted changes here: commit them first, so the copy is a commit of this repository')
base = git('rev-parse', 'HEAD:base').strip()
if git('rev-parse', 'HEAD', cwd=ROOT / 'base').strip() != base:
    sys.exit('base is not at the commit this repository records: commit base here first')
out.mkdir(parents=True, exist_ok=True)
if not (out / '.git').exists():
    git('init', '-q', '-b', 'main', cwd=out)
for p in out.iterdir():                                   # everything but its history
    if p.name == '.git':
        continue
    shutil.rmtree(p) if p.is_dir() else p.unlink()
tar = subprocess.run(['git', 'archive', '--format=tar', 'HEAD'], cwd=ROOT, check=True, capture_output=True).stdout
with tarfile.open(fileobj=io.BytesIO(tar)) as t:
    t.extractall(out)
(out / 'base').mkdir(exist_ok=True)
git('add', '-A', cwd=out)
git('update-index', '--add', '--cacheinfo', f'160000,{base},base', cwd=out)   # the submodule, by its commit alone
n = len(git('ls-files', cwd=out).splitlines())
print(f'public copy: {out}  ({n} files, base at {base[:7]}; staged, not committed)')
