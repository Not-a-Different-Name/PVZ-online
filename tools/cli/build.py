"""Build the MOD110 adapter against verified R1 objects plus the retry-sun fix."""
from pathlib import Path
import hashlib
import json
import subprocess
import shutil
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SNAPSHOT = ROOT / "work/releases/six-player-ui-r1-mod110-20261102-124111"
BUILD = SNAPSHOT / "build/pc-x86"
PC = SNAPSHOT / "re-plants-vs-zombies"
OUT = ROOT / "work/cli-mod110-20261103/build"
OUT.mkdir(parents=True, exist_ok=True)
hash_file = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

# Never silently combine different game revisions with cached production objects.
changed = []
for p in PC.rglob("*"):
    if p.is_file() and p.suffix.lower() in {".cpp", ".h", ".inl"}:
        active = ROOT / "re-plants-vs-zombies" / p.relative_to(PC)
        if not active.is_file() or hash_file(active) != hash_file(p):
            changed.append(p.relative_to(PC).as_posix())
assert sorted(changed) == ["Lawn/TeamPlay.cpp", "LawnApp.cpp"], changed
fix = json.loads((ROOT / "work/retry-sun-20261102/SOURCE.json").read_text(encoding="utf-8"))
print("Verified source differences:", changed)
commands = json.loads((BUILD / "compile_commands.json").read_text())
objects = [BUILD / e["output"] for e in commands if "LawnProject.dir" in e.get("output", "") and Path(e["file"]).name not in {"main.cpp", "TeamPlay.cpp", "LawnApp.cpp"}]
assert objects and all(p.is_file() for p in objects)
libraries = list((BUILD / "vendor-build").rglob("*.lib"))
system = [name + ".lib" for name in ("winmm ws2_32 kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 psapi").split()]
common = ["/nologo", "/std:c++latest", "/EHsc", "/utf-8", "/MD", "/DNDEBUG", "/DRELEASEFINAL", "/I" + str(PC), "/I" + str(PC / "SexyAppFramework")]
team = common + ["/c", str(ROOT / "re-plants-vs-zombies/Lawn/TeamPlay.cpp"), "/Fo" + str(OUT / "team.obj")]
lawn = common + ["/c", "/O2", "/Zi", str(ROOT / "re-plants-vs-zombies/LawnApp.cpp"), "/Fo" + str(OUT / "lawn.obj")]
entry = common + [str(HERE / "engine.cpp"), "/Fo" + str(OUT / "entry.obj"), "/Fe" + str(OUT / "pvz-cli-engine.exe")]
entry += [str(p) for p in objects + [OUT / "team.obj", OUT / "lawn.obj"] + libraries] + system
for name, args in [("team", team), ("lawn", lawn), ("entry", entry)]:
    (OUT / (name + ".rsp")).write_text("\n".join('"' + arg + '"' for arg in args), encoding="utf-8")
(OUT / "build.cmd").write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvarsall.bat" x86 >nul\ncl @"' + str(OUT / "team.rsp") + '"\nif errorlevel 1 exit /b 1\ncl @"'+str(OUT/'lawn.rsp')+'"\nif errorlevel 1 exit /b 1\ncl @"' + str(OUT / "entry.rsp") + '" /link /SUBSYSTEM:CONSOLE /DEBUG /MAP:"' + str(OUT / 'pvz-cli-engine.map') + '"\n', encoding="utf-8")
with (OUT / "build.log").open("w") as log:
    result = subprocess.run(["cmd", "/c", str(OUT / "build.cmd")], stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
if result.returncode:
    print((OUT / "build.log").read_text(errors="replace")[-6000:])
    raise SystemExit(result.returncode)
(OUT / "SOURCE.json").write_text(json.dumps({"build": 110, "base": str(SNAPSHOT), "baseAggregateSha256": "06f04c5a97d141be917e661a9a2f452c0d6f7dff658b2bfe61b9555f7ac9719c", "activeSourceDifferences": changed, "teamSha256": hash_file(ROOT / "re-plants-vs-zombies/Lawn/TeamPlay.cpp"), "lawnAppSha256": hash_file(ROOT / "re-plants-vs-zombies/LawnApp.cpp"), "cliSourceSha256": hash_file(HERE / "engine.cpp"), "exeSha256": hash_file(OUT / "pvz-cli-engine.exe"), "buildType": "native CLI entry + current TeamPlay/LawnApp recompiled, remaining R1 production objects relinked"}, indent=2), encoding="utf-8")
print(OUT / "pvz-cli-engine.exe")
