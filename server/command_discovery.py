import json
import os
import time
from pathlib import Path
from typing import Any, Optional


DISCOVERY_FILES = (
    "package.json",
    "pyproject.toml",
    "pytest.ini",
    "setup.cfg",
    "tox.ini",
    "ruff.toml",
    "requirements.txt",
    "requirements-dev.txt",
    "build_neural_engine.bat",
)


def _now_utc() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def _normalize_relpath(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _build_signature(workspace_root: Path) -> dict[str, Any]:
    files: list[dict[str, Any]] = []

    for name in DISCOVERY_FILES:
        path = workspace_root / name
        if path.exists():
            stat = path.stat()
            files.append({
                "path": name,
                "mtime": int(stat.st_mtime),
                "size": int(stat.st_size),
            })

    for package_path in workspace_root.glob("*/package.json"):
        if "node_modules" in package_path.parts or ".claude" in package_path.parts:
            continue
        stat = package_path.stat()
        files.append({
            "path": _normalize_relpath(package_path, workspace_root),
            "mtime": int(stat.st_mtime),
            "size": int(stat.st_size),
        })

    tests_dir = workspace_root / "tests"
    if tests_dir.exists():
        python_tests = sorted(tests_dir.glob("test*.py"))
        files.append({
            "path": "tests",
            "mtime": max((int(path.stat().st_mtime) for path in python_tests), default=int(tests_dir.stat().st_mtime)),
            "size": len(python_tests),
        })

    return {"files": files}


def _load_json(path: Path) -> Optional[dict[str, Any]]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def _npm_cmd() -> str:
    return "npm.cmd" if os.name == "nt" else "npm"


def _build_package_command(
    package_path: Path,
    workspace_root: Path,
    script_name: str,
    script_body: str,
) -> dict[str, Any]:
    package_dir = package_path.parent
    relative_dir = _normalize_relpath(package_dir, workspace_root)
    if relative_dir == ".":
        relative_dir = ""
    path_prefixes = [f"{relative_dir}/"] if relative_dir else []
    stack = "desktop" if "desktop_app" in relative_dir.split("/") else "node"
    copy_paths = [relative_dir] if relative_dir else []
    link_paths = []
    ignore_patterns: dict[str, list[str]] = {}

    if relative_dir and (package_dir / "node_modules").exists():
        link_paths.append(f"{relative_dir}/node_modules")
        ignore_patterns[relative_dir] = ["node_modules", "dist", "dist-electron", ".vite", "coverage"]

    category = script_name
    timeout = 120
    if script_name == "build":
        timeout = 180 if stack == "desktop" else 120
    elif script_name == "test":
        timeout = 180
    elif script_name == "lint":
        timeout = 120

    display_dir = relative_dir or "."
    command_name = f"{relative_dir.replace('/', '_') or 'root'}_{script_name}"
    if relative_dir == "desktop_app":
        command_name = f"desktop_{script_name}"
    return {
        "name": command_name,
        "category": category,
        "stack": stack,
        "cwd": relative_dir or ".",
        "command": [_npm_cmd(), "run", script_name],
        "timeout": timeout,
        "confidence": 0.98,
        "reason": f"Found `{script_name}` script in {display_dir}/package.json: {script_body}",
        "path_prefixes": path_prefixes,
        "copy_paths": copy_paths,
        "link_paths": link_paths,
        "ensure_dirs": [],
        "ignore_patterns": ignore_patterns,
    }


def _discover_package_commands(workspace_root: Path) -> list[dict[str, Any]]:
    commands: list[dict[str, Any]] = []
    package_paths = [workspace_root / "package.json", *sorted(workspace_root.glob("*/package.json"))]

    for package_path in package_paths:
        if not package_path.exists():
            continue
        if "node_modules" in package_path.parts or ".claude" in package_path.parts:
            continue

        package_data = _load_json(package_path)
        if not package_data:
            continue

        scripts = package_data.get("scripts") or {}
        if not isinstance(scripts, dict):
            continue

        for script_name in ("build", "test", "lint"):
            script_body = scripts.get(script_name)
            if isinstance(script_body, str) and script_body.strip():
                commands.append(_build_package_command(package_path, workspace_root, script_name, script_body.strip()))

    return commands


def _discover_python_commands(workspace_root: Path) -> list[dict[str, Any]]:
    commands: list[dict[str, Any]] = []

    if (workspace_root / "server").exists():
        commands.append({
            "name": "python_compileall",
            "category": "validate",
            "stack": "python",
            "cwd": ".",
            "command": ["py", "-3", "-m", "compileall", "server"],
            "timeout": 60,
            "confidence": 0.9,
            "reason": "Detected Python connector sources under server/; using compileall as a safe local validation step.",
            "path_prefixes": ["server/"],
            "copy_paths": ["server"],
            "link_paths": [],
            "ensure_dirs": [],
            "ignore_patterns": {},
        })

    tests_dir = workspace_root / "tests"
    if tests_dir.exists() and any(tests_dir.glob("test*.py")):
        commands.append({
            "name": "python_unittest",
            "category": "test",
            "stack": "python",
            "cwd": ".",
            "command": ["py", "-3", "-m", "unittest", "discover", "-s", "tests", "-p", "test*.py"],
            "timeout": 180,
            "confidence": 0.88,
            "reason": "Detected Python test files under tests/; using unittest discovery as the local test command.",
            "path_prefixes": ["server/", "tests/"],
            "copy_paths": ["server", "tests"],
            "link_paths": [],
            "ensure_dirs": [],
            "ignore_patterns": {},
        })

    return commands


def _discover_cpp_commands(workspace_root: Path) -> list[dict[str, Any]]:
    build_script = workspace_root / "build_neural_engine.bat"
    if not build_script.exists():
        return []

    return [{
        "name": "cpp_build",
        "category": "build",
        "stack": "cpp",
        "cwd": ".",
        "command": ["cmd", "/c", "build_neural_engine.bat"],
        "timeout": 240,
        "confidence": 0.92,
        "reason": "Detected build_neural_engine.bat; using the existing local C++ build script.",
        "path_prefixes": ["src/", "include/"],
        "copy_paths": ["src", "include", "build_neural_engine.bat"],
        "link_paths": [],
        "ensure_dirs": ["bin"],
        "ignore_patterns": {},
    }]


def _dedupe_commands(commands: list[dict[str, Any]]) -> list[dict[str, Any]]:
    unique: dict[str, dict[str, Any]] = {}
    for command in commands:
        unique[command["name"]] = command
    return list(unique.values())


def discover_workspace_commands(
    workspace_root: str,
    storage_dir: str | None = None,
    force_refresh: bool = False,
) -> dict[str, Any]:
    root = Path(workspace_root).resolve()
    cache_path = Path(storage_dir) / "command_discovery.json" if storage_dir else None
    signature = _build_signature(root)

    if cache_path and cache_path.exists() and not force_refresh:
        cached = _load_json(cache_path)
        if cached and cached.get("signature") == signature:
            return cached

    commands = _dedupe_commands([
        *_discover_package_commands(root),
        *_discover_python_commands(root),
        *_discover_cpp_commands(root),
    ])

    payload = {
        "workspaceRoot": str(root),
        "discoveredAt": _now_utc(),
        "signature": signature,
        "commands": commands,
    }

    if cache_path:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        cache_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    return payload


def match_commands_for_file(
    discovery: dict[str, Any],
    relative_file_path: str,
    categories: tuple[str, ...] = ("validate", "build"),
) -> list[dict[str, Any]]:
    normalized_path = relative_file_path.replace("\\", "/").lstrip("./")
    matches: list[dict[str, Any]] = []

    for command in discovery.get("commands", []):
        if command.get("category") not in categories:
            continue
        prefixes = command.get("path_prefixes") or []
        if not prefixes:
            continue
        if any(normalized_path.startswith(prefix) for prefix in prefixes):
            matches.append(command)

    matches.sort(
        key=lambda item: (
            0 if item.get("category") == "validate" else 1,
            -float(item.get("confidence") or 0),
            item.get("name") or "",
        )
    )
    return matches
