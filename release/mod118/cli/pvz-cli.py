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

PROTOCOL = 2
MAX_LINE = 1024 * 1024
COMMANDS = {
    "state": {}, "step": {"frames": (0, 10000, 1), "delay_ms": (0, 10, 0)},
    "advance": {"frames": (0, 10000, 300), "events": (0, 15, 15), "delay_ms": (0, 10, 0)},
    "plant_buffs": {"seed": (0, 47, None)},
    "shovel": {"row": (0, 5, None), "col": (0, 8, None)},
    "pause": {"enabled": (0, 1, None)},
    "cob": {"plant_id": (1, 4294967295, None), "row": (0, 5, None), "col": (0, 8, None)},
    "gift_sun": {"seat": (1, 6, None)},
    "swap": {"seat": (1, 6, None)},
    "swap_answer": {"accept": (0, 1, None)},
    "room_role": {"role": (1, 2, None)},
    "ready": {"enabled": (0, 1, None)},
    "start_answer": {"accept": (0, 1, None)},
    "team_vote": {"index": (-1, 2, None)},
    "reroll": {"locked_index": (-1, 2, -1)},
    "new_run": {"mode": (0, 3, 0), "difficulty": (0, 2, 1), "scene": (0, 4, 0),
                "scale": (0, 3, 1), "tempo": (0, 2, 1), "plant_zombies": (0, 1, 0),
                "boss_flags": (0, 6, 2), "buff_multiplier": (1, 3, 1), "first_wave": (0, 2, 0)},
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


class SessionTools:
    def __init__(self, send):
        self.send = send
        self.recipes = {}

    def call(self, request):
        if not isinstance(request, dict):
            raise ValueError("request must be an object")
        cmd = request.get("cmd")
        if not isinstance(cmd, str):
            raise ValueError("cmd must be a string")
        if cmd == "observe":
            response = self.send({"cmd": "state"})
            state = response.get("state")
            if not state:
                return response
            board, choice, chooser = (state.get(key) for key in ("board", "choice", "chooser"))
            if choice:
                phase = choice["kind"] + "_choice"
                actions = ["choice", "reroll"]
            elif chooser and chooser["interactive"]:
                phase = "seed_selection"
                actions = ["seed", "start"]
            elif state["scene"] == 3 and board:
                phase = "battle"
                actions = ["plant", "collect", "shovel", "cob", "pause", "save", "menu"]
            elif state["scene"] == 2:
                phase, actions = "intro", []
            else:
                phase, actions = "menu", ["new_run", "continue", "room_host", "room_join"]
            state["phase"] = phase
            if state["dialogs"] and not choice:
                actions = ["control"]
            network = state.get("network") or {}
            if network.get("connected"):
                if any(c.get("kind")=="team" and c["id"]==2 and c["enabled"] for c in state["controls"]):
                    actions.append("team_vote")
                if board and board.get("seeds_held"):
                    actions.append("ready")
                actions.append("swap_answer" if network.get("incoming_swap") else "swap")
                if phase=="battle" and not state["dialogs"]:
                    actions.append("gift_sun")
            state["action_types"] = actions + ["control"] if "control" not in actions else actions
            if request.get("summary"):
                state.pop("controls", None)
                if board:
                    for key in ("plants", "zombies", "coins"):
                        state["board"][key + "_count"] = len(board.pop(key, []))
            return response
        if cmd == "inspect" and request.get("kind") == "plant_buffs":
            return self.send({"cmd": "plant_buffs", "seed": request.get("seed")})
        if cmd == "act":
            if request.get("action") == "control":
                return self.activate_control(request)
            if not isinstance(request.get("action"), str) or request["action"] not in COMMANDS or request["action"] in {"quit", "step", "advance"}:
                raise ValueError("unknown game action")
            fields = {key: value for key, value in request.items() if key not in {"action", "board_id", "choice_id"}}
            if "board_id" in request or "choice_id" in request:
                state = self.send({"cmd": "state"})["state"]
                for kind, key in (("board", "board_id"), ("choice", "choice_id")):
                    if key in request and (state.get(kind) or {}).get(key) != request[key]:
                        return {"ok": False, "error": "expired " + key, "state": state}
            fields["cmd"] = request["action"]
            return self.send(fields)
        if cmd == "tools.list":
            return {"ok": True, "tools": sorted([*COMMANDS, "observe", "inspect", "act", "sequence",
                    "tools.list", "tools.register", "tools.remove", "tools.call"]),
                    "command_fields": COMMANDS, "registered": sorted(self.recipes)}
        if cmd in {"tools.register", "tools.remove", "tools.call"}:
            name = request.get("name")
            if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9_]{0,39}", name):
                raise ValueError("invalid tool name")
            if cmd == "tools.remove":
                return {"ok": True, "removed": self.recipes.pop(name, None) is not None}
            if cmd == "tools.call":
                if name not in self.recipes:
                    raise ValueError("unknown tool")
                args = request.get("args", {})
                if not isinstance(args, dict):
                    raise ValueError("tool arguments must be an object")
                def resolve(value):
                    if not isinstance(value, str) or not value.startswith("$"):
                        return value
                    if value[1:] not in args:
                        raise ValueError("missing tool argument " + value[1:])
                    return args[value[1:]]
                return self.sequence([{key: resolve(value) for key, value in step.items()}
                                      for step in self.recipes[name]])
            steps = self.validate_steps(request.get("steps"))
            if name not in self.recipes and len(self.recipes) >= 32:
                raise ValueError("session tool limit reached")
            self.recipes[name] = steps
            return {"ok": True, "name": name}
        if cmd == "sequence":
            return self.sequence(request.get("steps"))
        return self.send(request)

    def activate_control(self, request):
        response = self.send({"cmd": "state"})
        if not response.get("ok"):
            return response
        ident, label = request.get("control_id"), request.get("label")
        if ident is None and label is None:
            raise ValueError("provide an exact control_id or label")
        controls = [button for button in response["state"]["controls"] if button["enabled"]
                    and (ident is None or button["id"] == ident)
                    and (label is None or button["label"] == label)]
        if len(controls) != 1:
            return dict(response, ok=False, error="control is unavailable or ambiguous")
        x, y, width, height = controls[0]["rect"]
        return self.send({"cmd": "click", "x": x + width // 2, "y": y + height // 2, "button": 1})

    @staticmethod
    def validate_steps(steps):
        allowed = {*COMMANDS, "observe", "inspect", "act"} - {"quit", "host", "join", "room_host", "room_join"}
        if not isinstance(steps, list) or not 1 <= len(steps) <= 24:
            raise ValueError("sequence requires 1 to 24 steps")
        if any(not isinstance(step, dict) or not isinstance(step.get("cmd"), str) or
               step["cmd"] not in allowed for step in steps):
            raise ValueError("invalid or unsupported sequence step")
        return steps

    def sequence(self, steps):
        steps = self.validate_steps(steps)
        completed = []
        last = self.send({"cmd": "state"})
        initial = last.get("state", {})
        original = (initial.get("board") or {}).get("board_id")
        dialogs = {d["id"] for d in initial.get("dialogs", [])}
        for index, step in enumerate(steps):
            try:
                last = self.call(step)
            except ValueError as exc:
                last = {"ok": False, "error": str(exc)}
            completed.append({key: last[key] for key in ("ok", "error", "stopped", "details") if key in last})
            state = last.get("state", {})
            board = (state.get("board") or {}).get("board_id")
            new_dialog = any(d["id"] not in dialogs for d in state.get("dialogs", []))
            if (not last.get("ok") or last.get("pending") or
                    last.get("stopped") not in {None, "timeout"} or board != original or new_dialog):
                return dict(last, completed=completed, remaining=steps[index+1:])
        return dict(last, completed=completed, remaining=[])


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
    parser = argparse.ArgumentParser(description="PvZ Online native CLI; JSON Lines on stdin/stdout.")
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
            online = False
            def send(request):
                nonlocal online
                if online and request.get("cmd") in {"step", "advance"}:
                    request = dict(request, delay_ms=10)
                process.stdin.write(encode_request(request))
                process.stdin.flush()
                response = receive()
                if "state" in response:
                    online = bool((response["state"].get("network") or {}).get("active"))
                return response
            session = SessionTools(send)
            requests = queue.Queue(maxsize=64)
            def read_requests():
                with os.fdopen(os.dup(sys.stdin.fileno()), "rb") as stream:
                    while True:
                        line = stream.readline(MAX_LINE + 1)
                        if not line:
                            break
                        if len(line) > MAX_LINE:
                            while line and not line.endswith(b"\n"):
                                line = stream.readline(MAX_LINE + 1)
                            requests.put(b" " * (MAX_LINE + 1))
                        else:
                            requests.put(line)
                requests.put(None)
            threading.Thread(target=read_requests, daemon=True).start()
            while True:
                try:
                    line = requests.get(timeout=0.02 if online else None)
                except queue.Empty:
                    process.stdin.write("pulse 2\n")
                    process.stdin.flush()
                    online = receive()["active"]
                    continue
                if line is None:
                    break
                request = None
                try:
                    if len(line) > MAX_LINE:
                        raise ValueError("request exceeds 1 MiB")
                    request = json.loads(line, parse_constant=lambda _: (_ for _ in ()).throw(ValueError("non-finite JSON number")))
                    reply = session.call(request)
                except (ValueError, RecursionError, UnicodeError) as exc:
                    emit({"id": request.get("id") if isinstance(request, dict) else None, "ok": False, "error": str(exc)})
                    continue
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
