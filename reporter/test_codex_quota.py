import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

import codex_rpc
import platform_support
import reporter


SERVER = r'''
import json, sys
quota_reads = 0
for line in sys.stdin:
    request = json.loads(line)
    if "id" not in request or "method" not in request:
        continue
    method = request["method"]
    if method == "initialize":
        print("")
        print(json.dumps({"method": "notice", "params": {}}))
        print(json.dumps({"id": "server-request", "method": "unsupported/callback"}))
        result = {}
    elif method == "account/read":
        result = {"account": {"type": "chatgpt"}}
    elif method == "account/rateLimits/read":
        quota_reads += 1
        result = {"rateLimitsByLimitId": {"codex": {"primary": {
            "usedPercent": 20 + quota_reads, "windowDurationMins": 10080,
            "resetsAt": 2000000000}}}}
    print(json.dumps({"id": request["id"], "result": result}), flush=True)
'''


class QuotaTransportTests(unittest.TestCase):
    def setUp(self):
        self.children = []
        self.popen = subprocess.Popen

    def tearDown(self):
        for process in self.children:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=3)

    def spawn(self, args, **kwargs):
        code = SERVER
        if args[0] == "broken-codex":
            code = "import sys; print('env: node: No such file or directory', file=sys.stderr); sys.exit(127)"
        process = self.popen([sys.executable, "-u", "-c", code], **kwargs)
        self.children.append(process)
        return process

    def test_fallback_reuses_connection_and_reconnects_after_exit(self):
        collector = reporter.CodexQuotaCollector()
        with patch.object(collector, "_codex_executables", return_value=["broken-codex", "working-codex"]), \
                patch.object(codex_rpc.subprocess, "Popen", side_effect=self.spawn), \
                patch.object(reporter, "codex_auth_available", return_value=None):
            try:
                first = collector._read_snapshot()
                self.assertEqual(first.week_remaining_percent, 79)
                self.assertIsNone(first.short_remaining_percent)
                second = collector._read_snapshot()
                self.assertEqual(second.week_remaining_percent, 78)
                self.assertEqual(len(self.children), 2)
                self.children[-1].kill()
                self.children[-1].wait(timeout=3)
                third = collector._read_snapshot()
                self.assertEqual(third.week_remaining_percent, 79)
                self.assertEqual(len(self.children), 4)
            finally:
                collector._close_connection()
        self.assertTrue(all(p.poll() is not None for p in self.children))

    def test_connection_recycles_between_polls(self):
        collector = reporter.CodexQuotaCollector()
        with patch.object(collector, "_codex_executables", return_value=["working-codex"]), \
                patch.object(codex_rpc.subprocess, "Popen", side_effect=self.spawn), \
                patch.object(reporter, "codex_auth_available", return_value=None):
            try:
                collector._read_snapshot()
                collector._connection.started_at -= collector._CONNECTION_SECONDS
                collector._read_snapshot()
                self.assertEqual(len(self.children), 2)
                self.assertIsNotNone(self.children[0].poll())
            finally:
                collector._close_connection()

    def test_eof_reports_exit_code_and_safe_reason(self):
        with patch.object(codex_rpc.subprocess, "Popen", side_effect=self.spawn):
            connection = codex_rpc.AppServerConnection("broken-codex")
            try:
                with self.assertRaisesRegex(codex_rpc.CodexConnectionError,
                                            "code=127 reason=node_missing_from_path"):
                    connection.initialize()
            finally:
                connection.close()

    def test_auth_error_does_not_try_another_account_source(self):
        collector = reporter.CodexQuotaCollector()
        collector._accept_snapshot(reporter.QuotaSnapshot(week_remaining_percent=79, source="codex_app_server"))
        with patch.object(collector, "_codex_executables", return_value=["first", "second"]), \
                patch.object(reporter, "codex_auth_available", return_value=None), \
                patch.object(reporter, "AppServerConnection") as connection_type:
            connection_type.return_value.request.side_effect = codex_rpc.CodexRPCError(
                "account/rateLimits/read", {"code": 401, "message": "Unauthorized secret-token"})
            snapshot = collector._read_snapshot()
            collector._accept_snapshot(snapshot)
            self.assertEqual(snapshot.source, "login_required")
            self.assertIsNone(collector.snapshot().week_remaining_percent)
            self.assertEqual(connection_type.call_count, 1)

    def test_malformed_output_and_timeout_fail_without_leaking_output(self):
        for code, expected in [("print('PRIVATE malformed output', flush=True)", "invalid_json_response"),
                               ("import time; time.sleep(60)", "response_timeout")]:
            def spawn(args, **kwargs):
                process = self.popen([sys.executable, "-u", "-c", code], **kwargs)
                self.children.append(process)
                return process
            with patch.object(codex_rpc.subprocess, "Popen", side_effect=spawn):
                connection = codex_rpc.AppServerConnection("test-codex")
                try:
                    with self.assertRaisesRegex(codex_rpc.CodexConnectionError, expected):
                        connection.request("initialize", timeout=0.3)
                finally:
                    connection.close()
                self.assertTrue(all(not reader.is_alive() for reader in connection._readers))

    def test_service_error_keeps_cache_without_trying_every_binary(self):
        collector = reporter.CodexQuotaCollector()
        collector._accept_snapshot(reporter.QuotaSnapshot(week_remaining_percent=79, source="codex_app_server"))
        with patch.object(collector, "_codex_executables", return_value=["first", "second"]), \
                patch.object(reporter, "codex_auth_available", return_value=None), \
                patch.object(reporter, "AppServerConnection") as connection_type:
            connection_type.return_value.request.side_effect = codex_rpc.CodexRPCError(
                "account/rateLimits/read", {"code": -32000, "message": "service temporarily unavailable"})
            collector._accept_snapshot(collector._read_snapshot())
            self.assertTrue(collector.snapshot().stale)
            self.assertEqual(collector.snapshot().week_remaining_percent, 79)
            self.assertEqual(connection_type.call_count, 1)


class ExecutableSelectionTests(unittest.TestCase):
    def test_native_app_and_npm_binary_precede_node_wrapper_in_gui_environment(self):
        with tempfile.TemporaryDirectory() as root:
            home = Path(root).resolve()
            app = home / "Applications/ChatGPT.app/Contents/Resources/codex-cli/CodexCLI.app/Contents/MacOS/codex"
            package = home / "lib/node_modules/@openai/codex"
            wrapper = package / "bin/codex.js"
            native = package / "node_modules/@openai/codex-darwin-arm64/vendor/aarch64-apple-darwin/bin/codex"
            for path in (app, wrapper, native):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
                path.chmod(0o755)
            allowed = {app, wrapper, native}
            with patch.object(platform_support.sys, "platform", "darwin"), \
                    patch.object(platform_support.platform, "machine", return_value="arm64"), \
                    patch.object(Path, "home", return_value=home), \
                    patch.object(Path, "is_file", lambda p: p in allowed), \
                    patch.object(platform_support.shutil, "which", return_value=str(wrapper)), \
                    patch.dict(os.environ, {"PATH": "/usr/bin:/bin", "SYNA_CODEX_EXECUTABLE": ""}):
                self.assertEqual(platform_support.codex_executables(), [str(app), str(native), str(wrapper)])
                with patch.dict(os.environ, {"SYNA_CODEX_EXECUTABLE": str(native)}):
                    self.assertEqual(platform_support.codex_executable(), str(native))


if __name__ == "__main__":
    unittest.main()
