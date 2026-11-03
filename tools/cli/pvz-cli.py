#!/usr/bin/env python3
"""MOD110 JSON Lines front end. Only the bundled native engine simulates gameplay."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import queue
import re
import time

PROTOCOL = 1
MAX_LINE = 1024 * 1024
COMMANDS = {
    "state": {}, "step": {"frames": (0, 10000, 1), "delay_ms": (0, 10, 0)},
    "new_run": {"mode": (0, 3, 0), "difficulty": (0, 2, 1), "scene": (0, 4, 0)},
    "continue": {"mode": (0, 3, 0), "scene": (0, 4, 0), "slot": (0, 18446744073709551615, 0)},
    "choice": {"index": (-1, 2, None)}, "seed": {"seed": (0, 47, None)},
    "start": {}, "plant": {"slot": (0, 9, None), "row": (0, 5, None), "col": (0, 8, None)},
    "collect": {"coin_id": (1, 4294967295, None)},
    "click": {"x": (0, 799, None), "y": (0, 599, None), "button": (-1, 1, 1)},
    "key": {"key": (1, 254, None)}, "save": {}, "menu": {},
    "profile": {"profile_id": (1, 2147483647, None)}, "screenshot": {}, "quit": {},
    "host": {"port": (1, 65535, 27888)},
    "join": {"port": (1, 65535, 27888)},
    "room_host": {"port": (1, 65535, 97)},
    "room_join": {"port": (1, 65535, 97)}, "disconnect": {},
    "saves": {},
}


def encode_request(request):
    if not isinstance(request, dict):
        raise ValueError("request must be an object")
    command = request.get("cmd")
    if not isinstance(command, str) or command not in COMMANDS:
        raise ValueError("unknown cmd; see --help or README")
    fields = COMMANDS[command]
    extra = {"server"} if command in {"join", "room_host", "room_join"} else set()
    if command == "room_join":
        extra.add("room")
    if set(request) - set(fields) - {"cmd", "id"} - extra:
        raise ValueError("unknown fields for " + command)
    values = []
    for name, (low, high, default) in fields.items():
        value = request.get(name, default)
        if name == "slot" and isinstance(value, str) and re.fullmatch(r"[0-9]{1,20}", value):
            value = int(value)
        if type(value) is not int or not low <= value <= high:
            raise ValueError(f"{name} must be an integer in [{low}, {high}]")
        values.append(value)
    if command == "click" and not values[-1]:
        raise ValueError("button must be 1 (left) or -1 (right)")
    if extra:
        server = request.get("server", "127.0.0.1")
        if not isinstance(server, str) or not re.fullmatch(r"[A-Za-z0-9.-]{1,253}", server):
            raise ValueError("server must be an IPv4 address or DNS hostname")
        values.insert(0, server)
        if "room" in extra:
            room = request.get("room")
            if not isinstance(room, str) or not re.fullmatch(r"[A-Za-z0-9-]{1,32}", room):
                raise ValueError("room must be a room code")
            values.append(room)
    return " ".join([command] + [str(v) for v in values]) + "\n"


def emit(value):
    print(json.dumps(value, ensure_ascii=False, allow_nan=False), flush=True)


def backup_data(root):
    userdata = root / "userdata"
    if not userdata.exists():
        return None
    target = root / "cli-backups" / (time.strftime("%Y%m%d-%H%M%S") + "-" + str(os.getpid()))
    shutil.copytree(userdata, target / "userdata")
    return str(target)


def main():
    home = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="MOD110 real-engine CLI; JSON Lines on stdin/stdout.")
    parser.add_argument("--resources", type=Path, default=home / "resources", help="complete MOD110 PC resource directory")
    parser.add_argument("--engine", type=Path, default=home / "pvz-cli-engine.exe")
    parser.add_argument("--data-root", type=Path, default=home / "cli-data", help="directory containing userdata (same native format as graphical MOD110)")
    parser.add_argument("--shared-data", action="store_true", help="use %%ProgramData%%/PlantsVsZombies; close graphical game first")
    parser.add_argument("--timeout", type=int, default=180, help="engine startup/command timeout in seconds")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("the MOD110 native engine currently requires Windows")
    if args.timeout < 1 or args.timeout > 3600:
        parser.error("--timeout must be 1..3600")
    resources, engine = args.resources.resolve(), args.engine.resolve()
    if not (resources / "properties/partner.xml").is_file() or not (resources / "main.pak").is_file() or not engine.is_file():
        parser.error("missing engine/resources; use the complete CLI package or specify --resources and --engine")
    root = (Path(os.environ["ProgramData"]) / "PlantsVsZombies" if args.shared_data else args.data_root).resolve()
    if root.name.lower() == "userdata":
        parser.error("--data-root must be userdata's PARENT directory")
    root.mkdir(parents=True, exist_ok=True)
    # Hold the exact legacy game mutex for the lifetime of this process. A
    # graphical MOD110 started later also sees it and refuses a second copy.
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateMutexW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_wchar_p]
    kernel.CreateMutexW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel.ReleaseMutex.argtypes = [ctypes.c_void_p]
    shared = root == (Path(os.environ["ProgramData"]) / "PlantsVsZombies").resolve()
    mutex = None
    if shared:
        mutex = kernel.CreateMutexW(None, True, "PlantsVsZombiesMutex")
        if not mutex or ctypes.get_last_error() == 183:
            if mutex:
                kernel.CloseHandle(mutex)
            emit({"ok": False, "error": "graphical game or shared-data CLI is running; exit it before sharing saves"})
            return 2
    # Byte-range lock blocks another CLI using even a custom data directory.
    import msvcrt
    lock = (root / ".pvz-cli.lock").open("a+b")
    try:
        lock.seek(0)
        if not lock.read(1):
            lock.write(b"0")
            lock.flush()
        lock.seek(0)
        msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
    except OSError:
        emit({"ok": False, "error": "another CLI owns this data directory"})
        lock.close()
        if mutex:
            kernel.ReleaseMutex(mutex)
            kernel.CloseHandle(mutex)
        return 2
    process = None
    try:
        backup = backup_data(root)
        with (root / "cli-engine.log").open("a", encoding="utf-8") as log:
            # Native worker creates its private desktop before constructing LawnApp.
            # PyInstaller changes DLL search to its x64 Python runtime. The real
            # game is x86: do not inherit that directory into the native worker.
            kernel.SetDllDirectoryW.argtypes = [ctypes.c_wchar_p]
            kernel.SetDllDirectoryW(None)
            previous_error_mode = kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
            try:
                process = subprocess.Popen([str(engine), str(root)], cwd=resources,
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log,
                    text=True, encoding="utf-8", bufsize=1, creationflags=subprocess.CREATE_NO_WINDOW)
            finally:
                kernel.SetErrorMode(previous_error_mode)
                if getattr(sys, "frozen", False):
                    kernel.SetDllDirectoryW(sys._MEIPASS)
            replies = queue.Queue()
            def reader():
                try:
                    for line in process.stdout:
                        replies.put(json.loads(line))
                except Exception as exc:
                    replies.put({"ok": False, "error": "engine output error: " + str(exc)})
                finally:
                    replies.put(None)
            threading.Thread(target=reader, daemon=True).start()
            def receive():
                try:
                    value = replies.get(timeout=args.timeout)
                except queue.Empty:
                    raise RuntimeError("engine timeout; see cli-engine.log") from None
                if value is None:
                    raise RuntimeError("engine exited; see cli-engine.log")
                return value
            ready = receive()
            ready.update(event="ready", backup=backup)
            emit(ready)
            while True:
                line = sys.stdin.readline(MAX_LINE + 1)
                if not line:
                    break
                request = None
                try:
                    if len(line) > MAX_LINE:
                        while line and not line.endswith("\n"):
                            line = sys.stdin.readline(MAX_LINE + 1)
                        raise ValueError("request exceeds 1 MiB")
                    request = json.loads(line, parse_constant=lambda _: (_ for _ in ()).throw(ValueError("non-finite JSON number")))
                    encoded = encode_request(request)
                except (ValueError, RecursionError) as exc:
                    emit({"id": request.get("id") if isinstance(request, dict) else None, "ok": False, "error": str(exc)})
                    continue
                process.stdin.write(encoded)
                process.stdin.flush()
                reply = receive()
                reply["id"] = request.get("id")
                emit(reply)
                if request["cmd"] == "quit":
                    break
            process.stdin.close()
            process.wait(timeout=15)
            return process.returncode
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as exc:
        emit({"ok": False, "error": str(exc)})
        return 3
    except KeyboardInterrupt:
        return 130
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
        lock.seek(0)
        msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
        lock.close()
        if mutex:
            kernel.ReleaseMutex(mutex)
            kernel.CloseHandle(mutex)


if __name__ == "__main__":
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
