# Syna project-specific code and modifications: Copyright (c) 2026 黑沐.
# SPDX-License-Identifier: MIT; third-party notices remain applicable.
"""Host-specific paths and process helpers shared by Reporter launchers."""
from __future__ import annotations

import os
from pathlib import Path
import platform
import shutil
import sys


def application_data_dir() -> Path:
    override = os.environ.get("SYNA_REPORTER_DATA_DIR")
    if override:
        return Path(override).expanduser()
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "SynaReporter"
    return Path(os.environ.get("LOCALAPPDATA", Path.home())) / "AIAgentPanel"


def disk_root() -> str:
    if os.name == "nt":
        return os.environ.get("SystemDrive", "C:") + "\\"
    if sys.platform == "darwin" and Path("/System/Volumes/Data").is_dir():
        return "/System/Volumes/Data"
    return "/"


def codex_executables() -> list[str]:
    """Ordered fallbacks, with self-contained macOS binaries before Node wrappers."""
    candidates: list[Path] = []
    override = os.environ.get("SYNA_CODEX_EXECUTABLE")
    if override:
        candidates.append(Path(override).expanduser())
    executable = shutil.which("codex")
    if sys.platform == "darwin":
        architecture = "aarch64" if platform.machine() == "arm64" else "x86_64"
        pattern = f"openai.chatgpt-*-darwin-*/bin/macos-{architecture}/codex"
        for directory in (Path("/Applications"), Path.home() / "Applications"):
            for app in ("ChatGPT.app", "Codex.app"):
                resources = directory / app / "Contents/Resources"
                candidates.extend([
                    resources / "codex-cli/CodexCLI.app/Contents/MacOS/codex",
                    resources / "codex-cli/bin/codex",
                    resources / "codex",
                ])
        # npm's entry point uses /usr/bin/env node, which is often absent from
        # a Finder/login item's PATH. Its platform package contains a native CLI.
        wrappers = ([Path(executable)] if executable else []) + [
            Path("/opt/homebrew/bin/codex"), Path("/usr/local/bin/codex")]
        npm_arch = "arm64" if architecture == "aarch64" else "x64"
        for wrapper in wrappers:
            package = wrapper.resolve().parent.parent
            if package.name == "codex" and package.parent.name == "@openai":
                relative = f"vendor/{architecture}-apple-darwin/bin/codex"
                candidates.extend([
                    package / f"node_modules/@openai/codex-darwin-{npm_arch}" / relative,
                    package / relative,
                ])
    elif os.name == "nt":
        pattern = "openai.chatgpt-*-win32-x64/bin/windows-x86_64/codex.exe"
        wrappers = [Path(executable)] if executable else []
    else:
        pattern = "openai.chatgpt-*/bin/linux-*/codex"
        wrappers = [Path(executable)] if executable else []
    for name in (".vscode", ".vscode-insiders"):
        extensions = list((Path.home() / name / "extensions").glob(pattern))
        candidates.extend(sorted(extensions, key=lambda p: p.stat().st_mtime, reverse=True))
    candidates.extend(wrappers)
    result: list[str] = []
    seen: set[Path] = set()
    for path in candidates:
        try:
            resolved = path.resolve()
            if resolved not in seen and path.is_file() and os.access(path, os.X_OK):
                seen.add(resolved)
                result.append(str(path))
        except OSError:
            continue
    return result


def codex_executable() -> str | None:
    return next(iter(codex_executables()), None)


def acquire_posix_instance(role: str):
    """Keep the returned file open for the process lifetime. Never unlink it."""
    import fcntl

    root = application_data_dir()
    root.mkdir(parents=True, exist_ok=True)
    handle = (root / f"{role}.lock").open("a+")
    try:
        fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        handle.close()
        return None
    return handle
