"""Private rolling meter samples used for dorm electricity estimates."""
from __future__ import annotations
import json
import math
import os
from pathlib import Path


class UsageHistory:
    def __init__(self, building, room, path=None):
        self.room = [building, room]
        self.path = Path(path) if path else None
        self.samples = []
        if self.path:
            try:
                if self.path.stat().st_size > 200000: return
                saved = json.loads(self.path.read_text())
                if saved.get('room') == self.room:
                    for sample in saved.get('samples', [])[-2048:]:
                        if (isinstance(sample, list) and len(sample) == 2 and
                            all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in sample) and
                            (not self.samples or (sample[0] > self.samples[-1][0] and sample[1] >= self.samples[-1][1]))):
                            self.samples.append(sample)
            except (OSError, ValueError, TypeError, AttributeError): pass

    def observe(self, timestamp, used):
        if self.samples and used < self.samples[-1][1]: self.samples = []
        if self.samples and timestamp <= self.samples[-1][0]: return
        self.samples.append([timestamp, used])
        cutoff = timestamp - 7 * 86400
        self.samples = [s for s in self.samples[-2048:] if s[0] >= cutoff]
        if self.path:
            try:
                temp = self.path.with_suffix('.tmp')
                with open(temp, 'w', opener=lambda p, flags: os.open(p, flags, 0o600)) as f:
                    os.chmod(temp, 0o600)
                    json.dump({'room': self.room, 'samples': self.samples}, f)
                temp.replace(self.path)
            except OSError: pass

    def estimate(self, remaining):
        if len(self.samples) < 2: return None, None
        first, last = self.samples[0], self.samples[-1]
        span = last[0] - first[0]
        if span < 86400: return None, None
        daily = (last[1] - first[1]) * 86400 / span
        if not .05 <= daily <= 200: return None, None
        return daily, remaining / daily
