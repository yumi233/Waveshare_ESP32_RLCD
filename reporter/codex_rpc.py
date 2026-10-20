# Syna project-specific code and modifications: Copyright (c) 2026 黑沐.
# SPDX-License-Identifier: MIT; third-party notices remain applicable.
"""A reusable, bounded JSONL connection used only for Codex account queries."""
from __future__ import annotations

import json
import os
import queue
import signal
import subprocess
import threading
import time
from typing import Any


class CodexConnectionError(RuntimeError):
    pass


class CodexRPCError(RuntimeError):
    def __init__(self, method: str, error: dict[str, Any]) -> None:
        # Use the server text only to classify auth, never store it in logs.
        message = str(error.get("message", "")).casefold()
        self.login_required = error.get("code") in {401, 403} or any(
            token in message for token in ("not logged in", "login required", "unauthorized",
                                           "authentication", "token expired", "sign in"))
        code = error.get("code")
        self.unsupported = code in {-32601, -32602}
        super().__init__(f"{method}: RPC error {code if isinstance(code, int) else 'unknown'}")


class AppServerConnection:
    def __init__(self, executable: str) -> None:
        self.executable = executable
        self.started_at = time.monotonic()
        self.responses: queue.Queue[Any] = queue.Queue(maxsize=64)
        self._next_id = 0
        self._write_lock = threading.Lock()
        self._close_lock = threading.Lock()
        self._closed = False
        self._stderr_reason = "no_diagnostic"
        environment = os.environ.copy()
        if os.name != "nt":
            # Last-resort wrappers may still need Homebrew's Node interpreter.
            paths = environment.get("PATH", os.defpath).split(os.pathsep)
            environment["PATH"] = os.pathsep.join(dict.fromkeys(
                paths + ["/opt/homebrew/bin", "/usr/local/bin"]))
        self.process = subprocess.Popen(
            [executable, "app-server"],  # Default stdio works across CLI versions.
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8", errors="replace", env=environment,
            start_new_session=os.name != "nt",
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        self._readers = [
            threading.Thread(target=self._read_stdout, name="codex-quota-output", daemon=True),
            threading.Thread(target=self._read_stderr, name="codex-quota-errors", daemon=True),
        ]
        for reader in self._readers:
            reader.start()

    def _enqueue(self, value: Any) -> None:
        try:
            self.responses.put_nowait(value)
        except queue.Full:
            # Unexpected output floods must not grow memory or block shutdown.
            try:
                self.responses.get_nowait()
            except queue.Empty:
                pass
            try:
                self.responses.put_nowait(CodexConnectionError("response_queue_overflow"))
            except queue.Full:
                pass

    def _read_stdout(self) -> None:
        try:
            assert self.process.stdout is not None
            for line in self.process.stdout:
                if not line.strip():
                    continue
                try:
                    message = json.loads(line)
                except ValueError:
                    self._enqueue(CodexConnectionError("invalid_json_response"))
                    return
                if not isinstance(message, dict):
                    self._enqueue(CodexConnectionError("invalid_rpc_response"))
                    return
                if "id" not in message:
                    continue  # Notifications are not quota responses.
                if "method" in message:
                    # No server-initiated tools or credential callbacks are supported.
                    self.send({"id": message["id"], "error": {
                        "code": -32601, "message": "Method not supported by Reporter"}})
                else:
                    self._enqueue(message)
        except (OSError, ValueError, CodexConnectionError):
            pass  # Closing the connection interrupts the reader.
        finally:
            self._enqueue(None)

    def _read_stderr(self) -> None:
        try:
            assert self.process.stderr is not None
            for line in self.process.stderr:
                value = line.casefold()
                for pattern, reason in (
                    ("node: no such file", "node_missing_from_path"),
                    ("unexpected argument", "unsupported_cli_arguments"),
                    ("unrecognized subcommand", "unsupported_cli_command"),
                    ("error loading config", "config_error"),
                    ("dyld", "dynamic_library_error"),
                    ("permission denied", "permission_denied"),
                ):
                    if pattern in value:
                        self._stderr_reason = reason
                        break
                # Raw stderr can contain auth/config values; never retain or log it.
        except (OSError, ValueError):
            pass

    def send(self, message: dict[str, Any]) -> None:
        with self._write_lock:
            if self._closed or self.process.stdin is None:
                raise CodexConnectionError("connection_closed")
            self.process.stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
            self.process.stdin.flush()

    def request(self, method: str, params: Any = None, timeout: float = 10) -> dict[str, Any]:
        self._next_id += 1
        request_id = self._next_id
        self.send({"id": request_id, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise CodexConnectionError(f"{method}: response_timeout")
            try:
                message = self.responses.get(timeout=remaining)
            except queue.Empty:
                raise CodexConnectionError(f"{method}: response_timeout") from None
            if message is None:
                try:
                    self.process.wait(timeout=0.2)
                except subprocess.TimeoutExpired:
                    pass
                self._readers[1].join(timeout=0.2)
                raise CodexConnectionError(
                    f"process_exited code={self.process.poll()} reason={self._stderr_reason}")
            if isinstance(message, Exception):
                raise message
            if message.get("id") != request_id:
                continue
            if "error" in message:
                error = message["error"]
                raise CodexRPCError(method, error if isinstance(error, dict) else {})
            result = message.get("result")
            if not isinstance(result, dict):
                raise CodexConnectionError(f"{method}: invalid_result")
            return result

    def initialize(self) -> None:
        self.request("initialize", {
            "clientInfo": {"name": "ai-agent-panel-reporter",
                           "title": "AI Agent Panel Reporter", "version": "0.2.0"},
            "capabilities": {"experimentalApi": True},
        })
        self.send({"method": "initialized"})

    def close(self) -> None:
        with self._close_lock:
            if self._closed:
                return
            self._closed = True
            try:
                if os.name != "nt":
                    # npm wrappers spawn a native child inheriting all pipes.
                    os.killpg(self.process.pid, signal.SIGTERM)
                elif self.process.poll() is None:
                    self.process.terminate()
            except ProcessLookupError:
                pass
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                if os.name != "nt":
                    try:
                        os.killpg(self.process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                else:
                    self.process.kill()
                self.process.wait(timeout=2)
            if os.name != "nt":
                try:
                    os.killpg(self.process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            for reader in self._readers:
                reader.join(timeout=0.5)
            for pipe in (self.process.stdin, self.process.stdout, self.process.stderr):
                if pipe is not None:
                    try:
                        pipe.close()
                    except OSError:
                        pass
