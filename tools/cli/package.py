"""Assemble a standalone console front end and complete PC resources, no saves."""
from pathlib import Path
import subprocess,sys,shutil,json,hashlib,zipfile
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
WORK=ROOT/'work/cli-mod110-20261103'
PACKAGE=ROOT/'dist/PvZ-Online-MOD110-CLI-20261103'
assert not PACKAGE.exists(), 'refuse to overwrite an existing delivery'
subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--onedir','--console','--name','pvz-cli',
    '--distpath',str(WORK/'pyinstaller-dist'),'--workpath','D:/CodexBuilds/pvz-online/cli-mod110-20261103/pyinstaller',
    '--specpath',str(WORK),str(HERE/'pvz-cli.py')],check=True)
shutil.copytree(WORK/'pyinstaller-dist/pvz-cli',PACKAGE)
shutil.copy2(WORK/'build/pvz-cli-engine.exe',PACKAGE/'pvz-cli-engine.exe')
for name in ['pvz-cli.py','example_agent.py','README.md']:
    shutil.copy2(HERE/name,PACKAGE/name)
# Public resource/runtime allowlist, never runtime or any player-data tree.
PC=ROOT/'dist/PvZ-Online-MOD110-R1-Full-20261102/PC';resources=PACKAGE/'resources';resources.mkdir()
for name in ['main.pak','pvz_zh.pak','LICENSE.txt','pvz-lan-host.exe']:
    shutil.copy2(PC/name,resources/name)
for p in PC.glob('*.dll'):shutil.copy2(p,resources/p.name)
shutil.copytree(PC/'properties',resources/'properties')
shutil.copy2(WORK/'build/SOURCE.json',PACKAGE/'BUILD-SOURCE.json')
files={p.relative_to(PACKAGE).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in PACKAGE.rglob('*') if p.is_file()}
(PACKAGE/'SHA256SUMS.txt').write_text(''.join(h+'  '+name+'\n' for name,h in sorted(files.items())),encoding='utf-8')
(WORK/'PACKAGE.json').write_text(json.dumps({'path':str(PACKAGE),'files':files,'standalonePythonRuntime':True,'nativeGameBuild':110,'source':str(HERE)},indent=2),encoding='utf-8')
print(PACKAGE)
