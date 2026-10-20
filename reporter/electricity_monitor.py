"""Wuyi electricity balance and unpaid WeChat recharge QR orders.

No payment credentials are accepted and no charge/payment operation is called.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
from electricity_history import UsageHistory

import copy
import json
import math
import threading
import time
from urllib.parse import urlencode, urlparse
from urllib.request import Request, urlopen

BASE = "http://202.192.240.231"
API = BASE + "/scp-api/electricity-recharge/"


def valid_config(config):
    if not isinstance(config, dict) or any(type(config.get(k, 0)) is not int for k in ("building", "room")):
        raise ValueError("invalid_electricity_config")
    if "enabled" in config and type(config["enabled"]) is not bool:
        raise ValueError("invalid_electricity_config")
    building, room = int(config.get("building", 0)), int(config.get("room", 0))
    threshold = float(config.get("threshold_kwh", 10))
    if not (0 <= building <= 999 and 0 <= room <= 9999
            and math.isfinite(threshold) and 1 <= threshold <= 100):
        raise ValueError("invalid_electricity_config")
    return {"building": building, "room": room, "threshold_kwh": threshold,
            "enabled": bool(config.get("enabled", True))}


def fetch(endpoint, data, *, json_body=False):
    body = json.dumps(data).encode() if json_body else urlencode(data).encode()
    request = Request(API + endpoint, data=body, headers={
        "Content-Type": "application/json" if json_body else "application/x-www-form-urlencoded",
        "Referer": BASE + "/recharge.html", "Accept": "application/json"})
    with urlopen(request, timeout=10) as response:
        body = response.read(16385)
    if len(body) > 16384:
        raise ValueError("response_too_large")
    result = json.loads(body)
    if not isinstance(result, dict) or result.get("success") is not True:
        raise ValueError("school_api_rejected")
    data = result.get("data")
    if not isinstance(data, dict):
        raise ValueError("invalid_school_response")
    return data


def valid_order(data):
    qr, order = data.get("qrCode"), data.get("orderCode")
    duration = int(data.get("qrCodeExpireTime", 0))
    if not (isinstance(qr, str) and 0 < len(qr.encode()) <= 512
            and isinstance(order, str) and 0 < len(order.encode()) <= 128
            and 1 <= duration <= 1800):
        raise ValueError("invalid_payment_order")
    parsed = urlparse(qr)
    if not (parsed.scheme in {"weixin", "wxp"} or
            (parsed.scheme == "https" and parsed.hostname in {"wx.tenpay.com", "qr.95516.com"})):
        raise ValueError("invalid_payment_qr")
    return qr, order, duration


class ElectricityMonitor:
    def __init__(self, config, history_path=None):
        self._lock = threading.RLock()
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._thread = threading.Thread(target=self._run, name="electricity", daemon=True)
        self._generation = 0
        self._next_balance = 0.0
        self._next_check = 0.0
        self._order = ""
        self._pending = None
        self._auto_latched = False
        self._dismissed = False
        self._history_path = history_path
        self._config = valid_config(config)
        self._history = UsageHistory(self._config["building"], self._config["room"], history_path)
        self._state = self._empty()

    def _empty(self):
        c = self._config
        return {**c, "configured": bool(c["enabled"] and c["building"] and c["room"]),
                "available": False, "stale": False, "remaining_kwh": None,
                "used_kwh": None, "updated_at": 0, "low_balance": False,
                "daily_kwh": None, "estimated_days": None, "estimate_state": "learning",
                "payment_state": "idle", "payment_amount_yuan": 100,
                "qr_code": "", "expires_at": 0, "alert_id": 0, "error": ""}

    def start(self):
        self._thread.start()

    def stop(self):
        self._stop.set()
        self._wake.set()
        if self._thread.ident is not None:
            self._thread.join(timeout=12)

    def snapshot(self):
        with self._lock:
            result = copy.deepcopy(self._state)
            if result["payment_state"] == "ready" and result["expires_at"] <= time.time():
                result.update(payment_state="expired", qr_code="")
            result["expires_in_seconds"] = max(0, min(1800, int(result["expires_at"] - time.time())))
            # A paid order keeps its popup suppressed while meter credit catches up.
            result["show_alert"] = (result["low_balance"] and result["available"]
                                    and not result["stale"] and not self._dismissed)
            return result

    def update_config(self, config):
        config = valid_config(config)
        with self._lock:
            if config == self._config:
                return
            self._generation += 1
            self._config = config
            self._history = UsageHistory(config["building"], config["room"], self._history_path)
            self._state = self._empty()
            self._order = ""
            self._pending = None
            self._auto_latched = self._dismissed = False
            self._next_balance = 0
        self._wake.set()

    def request_payment(self, amount):
        if type(amount) is not int or amount not in (50, 100):
            return False
        with self._lock:
            s = self._state
            if not s["configured"] or not s["available"] or s["stale"]:
                return False
            if s["payment_state"] == "creating":
                return False
            if (s["payment_state"] == "ready" and s["payment_amount_yuan"] == amount
                    and s["expires_at"] > time.time()):
                self._dismissed = False
                self._auto_latched = True
                return True
            self._pending = (amount, self._generation)
            s.update(payment_state="creating", payment_amount_yuan=amount, qr_code="", error="")
            self._dismissed = False
            self._auto_latched = True
        self._wake.set()
        return True

    def dismiss(self):
        with self._lock:
            self._dismissed = True
            self._auto_latched = True

    def request_check(self):
        with self._lock:
            self._next_check = 0
            self._next_balance = 0 if not self._order else self._next_balance
        self._wake.set()

    def _balance(self):
        with self._lock:
            generation, config = self._generation, dict(self._config)
        try:
            data = fetch("getCurrentRemaining_v2", {
                "userTypeID": 1, "building": config["building"], "room": config["room"]})
            remaining, used = float(data["resamp"]), float(data["usedamp"])
            if not all(math.isfinite(v) and 0 <= v < 1000000 for v in (remaining, used)):
                raise ValueError("invalid_balance")
        except Exception:
            with self._lock:
                if generation == self._generation:
                    self._state.update(stale=True, error="balance_unavailable")
                    self._next_balance = time.monotonic() + 60
            return
        with self._lock:
            if generation != self._generation:
                return
            s = self._state
            low = remaining <= config["threshold_kwh"]
            if remaining > config["threshold_kwh"] + 2:
                self._auto_latched = self._dismissed = False
            s.update(available=True, stale=False, remaining_kwh=remaining, used_kwh=used,
                     updated_at=int(time.time()), low_balance=low, error="")
            timestamp = time.time()
            try:
                meter_time = datetime.fromisoformat(data.get("updatedt", ""))
                if meter_time.tzinfo is None:
                    # School meter dates use China Standard Time. Fixed UTC+8
                    # also works on Windows without an external IANA database.
                    meter_time = meter_time.replace(tzinfo=timezone(timedelta(hours=8)))
                timestamp = meter_time.timestamp()
            except (ValueError, TypeError): pass
            if time.time() - 8 * 86400 <= timestamp <= time.time() + 600:
                self._history.observe(timestamp, used)
                daily, days = self._history.estimate(remaining)
                s.update(daily_kwh=daily, estimated_days=days,
                         estimate_state="ready" if days is not None else "learning")
            else:
                s.update(daily_kwh=None, estimated_days=None, estimate_state="learning")
            self._next_balance = time.monotonic() + 300
            if low and not self._auto_latched:
                s["alert_id"] += 1
                self.request_payment(100)

    def _create(self, pending):
        amount, generation = pending
        with self._lock:
            if generation != self._generation:
                return
            config = dict(self._config)
        try:
            data = fetch("create-order-cb", {"mobile": "", "building": config["building"],
                         "room": config["room"], "payCent": amount * 100,
                         "userTypeId": 1, "codeType": "weixin"}, json_body=True)
            qr, order, duration = valid_order(data)
        except Exception:
            with self._lock:
                if generation == self._generation:
                    self._state.update(payment_state="error", qr_code="", error="qr_unavailable")
            return
        with self._lock:
            if generation != self._generation:
                return
            self._order = order
            self._state.update(payment_state="ready", qr_code=qr,
                               expires_at=int(time.time()) + duration, error="")
            self._state["alert_id"] += 1
            self._next_check = time.monotonic() + 10

    def _check(self):
        with self._lock:
            generation, order = self._generation, self._order
            self._next_check = time.monotonic() + 10
        try:
            data = fetch("check-order-cb", {"orderCode": order})
            result = data["record"]["payResult"]
        except Exception:
            return
        with self._lock:
            if generation != self._generation or order != self._order:
                return
            if type(result) is int and result in (1, 2):
                self._state.update(payment_state="paid" if result == 1 else "failed", qr_code="")
                self._order = ""
                self._auto_latched = True
                self._dismissed = result == 1
                self._next_balance = 0

    def _tick(self):
        with self._lock:
            if not self._state["configured"]:
                return
            pending, self._pending = self._pending, None
            balance_due = time.monotonic() >= self._next_balance
        if balance_due:
            self._balance()
        if pending is not None:
            self._create(pending)
        with self._lock:
            if self._state["payment_state"] == "ready" and self._state["expires_at"] <= time.time():
                self._state.update(payment_state="expired", qr_code="")
            if self._state["expires_at"] + 300 < time.time():
                self._order = ""
            check_due = bool(self._order and self._state["payment_state"] in {"ready", "expired"}
                             and time.monotonic() >= self._next_check)
        if check_due:
            self._check()

    def _run(self):
        while not self._stop.is_set():
            self._wake.clear()
            self._tick()
            self._wake.wait(1)
