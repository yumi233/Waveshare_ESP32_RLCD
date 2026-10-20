import errno
import os
from pathlib import Path
import socket
import tempfile
import unittest
from unittest.mock import Mock, patch

import platform_support
import reporter


class ReporterRecoveryTests(unittest.TestCase):
    def test_invalid_override_falls_back_to_current_executable(self):
        with tempfile.TemporaryDirectory() as root:
            executable = Path(root) / 'codex'
            executable.touch()
            executable.chmod(0o755)
            with patch.dict(os.environ, {'SYNA_CODEX_EXECUTABLE': '/missing/codex'}), \
                    patch.object(platform_support.shutil, 'which', return_value=str(executable)), \
                    patch.object(Path, "is_file", lambda path: path == executable):
                self.assertEqual(platform_support.codex_executable(), str(executable))

    def test_unreachable_peer_does_not_stop_discovery(self):
        state = Mock()
        state.discovery.return_value = {}
        service = reporter.DiscoveryServer(state)
        sock = Mock()
        calls = 0
        def receive(_):
            nonlocal calls
            calls += 1
            if calls == 1:
                return reporter.DISCOVERY_REQUEST, ('192.0.2.1', 42100)
            service._stop.set()
            raise socket.timeout()
        sock.recvfrom.side_effect = receive
        sock.sendto.side_effect = OSError(errno.EHOSTUNREACH, 'unreachable')
        with patch.object(reporter.socket, 'socket', return_value=sock):
            service._run()
        self.assertEqual(calls, 2)
        sock.close.assert_called_once()
