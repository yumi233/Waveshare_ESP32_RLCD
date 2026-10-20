import json
from datetime import datetime, timedelta, timezone
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen

import electricity_monitor as e
import reporter

CONFIG = {'building': 1, 'room': 101, 'threshold_kwh': 10, 'enabled': True}
ORDER = {'qrCode': 'https://qr.95516.com/test-unpaid', 'orderCode': 'test-order', 'qrCodeExpireTime': 300}


class ElectricityTests(unittest.TestCase):
    def setUp(self):
        self.monitor = e.ElectricityMonitor(CONFIG)
        self.balance = 8.5
        self.paid = 0
        self.calls = []
        def fetch(endpoint, data, **kwargs):
            self.calls.append((endpoint, data, kwargs))
            if endpoint == 'getCurrentRemaining_v2':
                return {'resamp': self.balance, 'usedamp': 200}
            if endpoint == 'create-order-cb': return dict(ORDER)
            if endpoint == 'check-order-cb': return {'record': {'payResult': self.paid}}
            self.fail('unexpected school API')
        self.fetch = patch.object(e, 'fetch', side_effect=fetch)
        self.fetch.start()
        self.addCleanup(self.fetch.stop)

    def test_low_balance_generates_100_once_then_can_select_50(self):
        self.monitor._balance()
        self.assertTrue(self.monitor.snapshot()['show_alert'])
        self.monitor._tick()
        s = self.monitor.snapshot()
        self.assertEqual(s['payment_state'], 'ready')
        self.assertEqual(self.calls[-1][1]['payCent'], 10000)
        self.assertEqual(self.calls[-1][2], {'json_body': True})
        for _ in range(3): self.monitor._balance()
        self.assertEqual(sum(c[0] == 'create-order-cb' for c in self.calls), 1)
        self.assertTrue(self.monitor.request_payment(50))
        self.monitor._tick()
        self.assertEqual(self.calls[-1][1]['payCent'], 5000)
        self.assertEqual(self.monitor.snapshot()['payment_amount_yuan'], 50)

    def test_normal_balance_no_auto_order_and_amounts_strict(self):
        self.balance = 100
        self.monitor._tick()
        self.assertEqual([c[0] for c in self.calls], ['getCurrentRemaining_v2'])
        for invalid in (0, 1, 500, '100', True, 50.0):
            self.assertFalse(self.monitor.request_payment(invalid))
        self.assertTrue(self.monitor.request_payment(100))

    def test_school_naive_meter_time_uses_utc_plus_eight_without_tzdata(self):
        expected = 1800000000
        school = datetime.fromtimestamp(expected, timezone(timedelta(hours=8))).replace(tzinfo=None)
        data = {'resamp': 100, 'usedamp': 200, 'updatedt': school.isoformat()}
        with patch.object(e, 'fetch', return_value=data), patch.object(e.time, 'time', return_value=expected):
            self.monitor._balance()
        self.assertEqual(self.monitor._history.samples[-1][0], expected)

    def test_explicit_meter_timezone_is_preserved(self):
        expected = 1800000000
        school = datetime.fromtimestamp(expected, timezone.utc)
        data = {'resamp': 100, 'usedamp': 200, 'updatedt': school.isoformat()}
        with patch.object(e, 'fetch', return_value=data), patch.object(e.time, 'time', return_value=expected):
            self.monitor._balance()
        self.assertEqual(self.monitor._history.samples[-1][0], expected)

    def test_stale_or_unavailable_cannot_generate(self):
        self.assertFalse(self.monitor.request_payment(100))
        self.balance = 100
        self.monitor._balance()
        with patch.object(e, 'fetch', side_effect=OSError): self.monitor._balance()
        self.assertTrue(self.monitor.snapshot()['stale'])
        self.assertFalse(self.monitor.request_payment(50))
        self.assertFalse(self.monitor.snapshot()['show_alert'])

    def test_dismiss_then_rearm_after_balance_recovers(self):
        self.monitor._balance(); self.monitor._tick(); self.monitor.dismiss()
        self.monitor._balance()
        self.assertFalse(self.monitor.snapshot()['show_alert'])
        self.assertIsNone(self.monitor._pending)
        self.balance = 13; self.monitor._balance()
        self.balance = 9; self.monitor._balance()
        self.assertTrue(self.monitor.snapshot()['show_alert'])
        self.assertEqual(self.monitor.snapshot()['payment_state'], 'ready')

    def test_expiration_clears_qr_without_automatically_new_order(self):
        self.monitor._balance(); self.monitor._tick()
        with patch.object(e.time, 'time', return_value=self.monitor._state['expires_at'] + 1):
            self.assertEqual(self.monitor.snapshot()['qr_code'], '')
            self.monitor._tick()
            self.assertEqual(self.monitor.snapshot()['payment_state'], 'expired')
        self.assertEqual(sum(c[0] == 'create-order-cb' for c in self.calls), 1)

    def test_paid_clears_qr_refreshes_balance_and_suppresses_duplicate(self):
        self.monitor._balance(); self.monitor._tick()
        self.paid = 1; self.monitor._check(); self.monitor._balance()
        s = self.monitor.snapshot()
        self.assertEqual(s['payment_state'], 'paid')
        self.assertEqual(s['qr_code'], '')
        self.assertFalse(s['show_alert'])
        self.assertIsNone(self.monitor._pending)

    def test_old_room_pending_and_result_are_discarded(self):
        self.monitor._balance()
        pending = self.monitor._pending
        self.monitor.update_config({**CONFIG, 'room': 102})
        count = len(self.calls); self.monitor._create(pending)
        self.assertEqual(len(self.calls), count)
        self.monitor._balance()
        def changed(*args, **kwargs):
            self.monitor.update_config({**CONFIG, 'room': 103})
            return dict(ORDER)
        with patch.object(e, 'fetch', side_effect=changed):
            self.monitor._create(self.monitor._pending)
        self.assertEqual(self.monitor.snapshot()['qr_code'], '')

    def test_order_validation_and_failed_generation(self):
        for qr in ('https://evil.example/pay', 'https://qr.95516.com.evil/pay', 'javascript:pay', 'x'*513):
            with self.assertRaises(ValueError): e.valid_order({**ORDER, 'qrCode': qr})
        self.monitor._balance()
        with patch.object(e, 'fetch', side_effect=OSError):
            self.monitor._create(self.monitor._pending)
        self.assertEqual(self.monitor.snapshot()['payment_state'], 'error')
        self.assertEqual(self.monitor.snapshot()['qr_code'], '')

    def test_config_rejects_invalid_inputs(self):
        for item in ({'building':True}, {'room':1.5}, {'threshold_kwh':float('nan')},
                     {'threshold_kwh':0}, {'enabled':'false'}):
            with self.assertRaises((ValueError, TypeError)): e.valid_config(item)


class ElectricityHTTPTests(unittest.TestCase):
    def setUp(self):
        monitor = e.ElectricityMonitor(CONFIG)
        state = reporter.ReporterState({'reporter_id':'test', 'computer_name':'test', 'pairing_token':'private'},
            Mock(), Mock(), Mock(), Mock(), 0, monitor)
        self.state, self.monitor = state, monitor
        self.server = reporter.ThreadingHTTPServer(('127.0.0.1', 0), reporter.make_handler(state))
        state.http_port = self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True);self.thread.start()
        self.addCleanup(self.server.server_close);self.addCleanup(self.server.shutdown)

    def post(self, operation, body, headers=None):
        request = Request(f'http://127.0.0.1:{self.server.server_port}/api/v1/electricity/{operation}',
            data=json.dumps(body).encode(), headers=headers or {'Content-Type':'application/json'})
        try:
            with urlopen(request, timeout=2) as response: return response.status, json.load(response)
        except HTTPError as ex:
            with ex: return ex.code, json.load(ex)

    def test_mutation_requires_json_and_rejects_foreign_origin(self):
        self.assertEqual(self.post('dismiss', {}, {'Content-Type':'text/plain'})[0], 415)
        self.assertEqual(self.post('dismiss', {}, {'Content-Type':'application/json','Origin':'https://evil.example'})[0],403)
        self.assertEqual(self.post('recharge', {'amount_yuan':100})[0],409)
        self.assertEqual(self.post('dismiss', {})[0],200)

    def test_device_token_bound_to_seen_peer_and_not_public(self):
        s=self.state
        self.assertFalse(s.authorize_command('192.0.2.2', 'fake'))
        s.record_device('192.0.2.2', 'test-board')
        token=s.command_token('192.0.2.2')
        self.assertTrue(s.authorize_command('192.0.2.2', token))
        self.assertFalse(s.authorize_command('192.0.2.3', token))
        self.assertNotIn('command_token', s.recent_devices()[0])
        with patch.object(reporter.time, 'monotonic', return_value=1e20):
            self.assertFalse(s.authorize_command('192.0.2.2', token))

    def test_config_preserves_pairing_identity(self):
        with tempfile.TemporaryDirectory() as root:
            p=Path(root)/'config.json';p.write_text(json.dumps({'reporter_id':'test','pairing_token':'private'}))
            with patch.object(reporter, 'config_path', return_value=p):
                self.assertEqual(self.post('config', CONFIG)[0],200)
            saved=json.loads(p.read_text())
            self.assertEqual(saved['pairing_token'],'private')
            self.assertEqual(saved['electricity']['threshold_kwh'],10)
            self.assertEqual(p.stat().st_mode & 0o777,0o600)


class UsageHistoryTests(unittest.TestCase):
    def test_requires_full_day_and_calculates_from_used_counter(self):
        from electricity_history import UsageHistory
        h=UsageHistory(1,101)
        h.observe(100000,100);h.observe(100000+86399,110)
        self.assertEqual(h.estimate(50),(None,None))
        h.observe(100000+86400,110)
        self.assertEqual(h.estimate(50),(10,5))

    def test_recharge_does_not_change_usage_rate_and_duplicate_time_ignored(self):
        from electricity_history import UsageHistory
        h=UsageHistory(1,101)
        h.observe(100000,100);h.observe(186400,105);h.observe(186400,1000)
        self.assertEqual(h.estimate(50),(5,10))
        self.assertEqual(h.estimate(100),(5,20))

    def test_counter_reset_and_zero_usage_have_no_invented_estimate(self):
        from electricity_history import UsageHistory
        h=UsageHistory(1,101);h.observe(100000,100);h.observe(186400,100)
        self.assertEqual(h.estimate(50),(None,None))
        h.observe(200000,1)
        self.assertEqual(h.estimate(50),(None,None))

    def test_private_persistence_and_room_isolation(self):
        from electricity_history import UsageHistory
        with tempfile.TemporaryDirectory() as root:
            p=Path(root)/'history.json';h=UsageHistory(1,101,p)
            h.observe(100000,100);h.observe(186400,110)
            self.assertEqual(UsageHistory(1,101,p).estimate(50),(10,5))
            self.assertEqual(UsageHistory(1,102,p).estimate(50),(None,None))
            self.assertEqual(p.stat().st_mode & 0o777,0o600)


if __name__ == '__main__':
    unittest.main()
