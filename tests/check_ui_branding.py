# Copyright (c) 2026 黑沐, 玉米. MIT License.
"""Build and exercise the real UI with the simulator's pinned LVGL configuration."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = root / 'simulator/build-mac'
subprocess.run(['cmake', '-S', str(root / 'simulator'), '-B', str(build),
                '-DAI_PANEL_BUILD_HOST_TESTS=ON'], check=True)
subprocess.run(['cmake', '--build', str(build), '--target', 'ui_branding_test', '-j', '6'], check=True)
output = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else build / 'about.ppm'
output.parent.mkdir(parents=True, exist_ok=True)
exe = build / 'bin' / ('ui_branding_test.exe' if os.name == 'nt' else 'ui_branding_test')
env = dict(os.environ, SDL_VIDEODRIVER='dummy')
subprocess.run([str(exe), str(output)], check=True, env=env)
