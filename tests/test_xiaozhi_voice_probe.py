"""Offline protocol/audio lifecycle tests. All inputs and identities are synthetic."""

import json
import logging
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import xiaozhi_voice_probe as probe


def hello(version=1):
    return json.dumps({"type": "hello", "version": version, "transport": "websocket",
                       "session_id": "synthetic-session", "audio_params": {
                           "format": "opus", "sample_rate": 24000, "channels": 1,
                           "frame_duration": 60}})


def event(kind, **fields):
    return json.dumps({"type": kind, "session_id": "synthetic-session", **fields})


class PacketTests(unittest.TestCase):
    def test_every_legal_packet_size_roundtrips_all_versions(self):
        for version in (1, 2, 3):
            for size in range(1, probe.OPUS_MAX + 1):
                payload = bytes(x % 251 for x in range(size))
                self.assertEqual(payload, probe.unpack_audio(version, probe.pack_audio(version, 60, payload)))

    def test_headers_match_wire_vectors(self):
        self.assertEqual(probe.pack_audio(2, 0x12345678, b"abc"),
                         bytes.fromhex("00020000000000001234567800000003") + b"abc")
        self.assertEqual(probe.pack_audio(3, 0, b"abc"), b"\x00\x00\x00\x03abc")

    def test_truncation_reserved_type_version_and_lengths_rejected(self):
        for version in (2, 3):
            packet = probe.pack_audio(version, 0, b"abc")
            for size in range(len(packet)):
                with self.assertRaises(probe.ProbeError):
                    probe.unpack_audio(version, packet[:size])
            for pos in ((1, 3, 7, 15) if version == 2 else (0, 1, 3)):
                invalid = bytearray(packet)
                invalid[pos] ^= 1
                with self.assertRaises(probe.ProbeError):
                    probe.unpack_audio(version, bytes(invalid))
        for payload in (b"", b"x" * 1276):
            for version in (1, 2, 3):
                with self.assertRaises(probe.ProbeError):
                    probe.pack_audio(version, 0, payload)
                with self.assertRaises(probe.ProbeError):
                    probe.unpack_audio(version, payload)

    def test_header_injection_and_non_tls_urls_rejected_without_echo(self):
        for value in ("http://example.invalid", "wss://user:secret@example.invalid/",
                      "wss://example.invalid/#secret", "wss://example.invalid/\r\nsecret"):
            with self.assertRaises(probe.ProbeError) as caught:
                probe.ConnectionConfig(value, "secret", "d", "c").validate()
            self.assertNotIn("secret", str(caught.exception))
        with self.assertRaises(probe.ProbeError):
            probe.ConnectionConfig("wss://example.invalid/", "secret\r\ninjected", "d", "c").validate()
        self.assertNotIn("secret", repr(probe.ConnectionConfig("wss://example.invalid/", "secret", "d", "c")))


class ReceiveTests(unittest.TestCase):
    def state(self, version=1, keep_audio=False):
        result = probe.ProbeResult(version, 960)
        state = probe.ReceiveState(result, lambda packet: struct.pack("<hhh", 0, 1200, -1600), keep_audio)
        state.consume(hello(version))
        state.response_enabled = True
        return state, result

    def test_representative_event_order_with_server24k_and_host16k(self):
        for version in (1, 2, 3):
            state, result = self.state(version, keep_audio=True)
            result.tx_frames = 1
            for message in (event("stt", text="synthetic request"),
                            event("tts", state="start"),
                            event("tts", state="sentence_start", text="synthetic answer"),
                            probe.pack_audio(version, 0, b"abc"),
                            event("tts", state="sentence_end"),
                            event("tts", state="stop")):
                state.consume(message)
            summary = result.safe_summary()
            self.assertTrue(summary["passed"])
            self.assertEqual(summary["server_sample_rate"], 24000)
            self.assertEqual(summary["output_sample_rate"], 16000)
            self.assertEqual(result.rx_frames, 1)
            self.assertEqual(result.output_peak, 1600)
            self.assertEqual(len(state.audio), 6)
            self.assertNotIn("synthetic", json.dumps(summary))
            self.assertNotIn("synthetic", repr(result))

    def test_handshake_or_tts_without_audio_is_not_a_pass(self):
        state, result = self.state()
        result.tx_frames = 1
        self.assertFalse(result.safe_summary()["passed"])
        state.consume(event("tts", state="start"))
        state.consume(event("tts", state="stop"))
        self.assertFalse(result.safe_summary()["passed"])

    def test_no_tool_execution_and_late_audio_rejected(self):
        state, result = self.state()
        state.consume(event("mcp", payload={"method": "tools/call"}))
        self.assertEqual(result.ignored_messages, 1)
        self.assertIsNone(state.audio)
        with self.assertRaisesRegex(probe.ProbeError, "audio_outside_tts"):
            state.consume(b"abc")
        state.consume(event("tts", state="start"))
        state.consume(event("tts", state="stop"))
        with self.assertRaisesRegex(probe.ProbeError, "audio_outside_tts"):
            state.consume(b"abc")

    def test_wrong_session_oversized_and_prehello_events_fail(self):
        state, _ = self.state()
        for message in (json.dumps({"type": "stt", "session_id": "wrong"}),
                        "x" * (probe.MESSAGE_MAX + 1), "[]", "not-json"):
            with self.assertRaises(probe.ProbeError):
                state.consume(message)
        unstarted = probe.ReceiveState(probe.ProbeResult(1, 960), lambda packet: b"\0\0")
        with self.assertRaisesRegex(probe.ProbeError, "message_before_hello"):
            unstarted.consume(event("stt", text="private"))

    def test_audio_and_transcript_accumulation_are_bounded(self):
        state, result = self.state()
        state.consume(event("tts", state="start"))
        result.output_samples = probe.RATE * probe.RESPONSE_SECONDS_MAX
        with self.assertRaisesRegex(probe.ProbeError, "response_audio_limit"):
            state.consume(b"abc")
        result.stt_texts = ["x" * (probe.MESSAGE_MAX * 2)]
        with self.assertRaisesRegex(probe.ProbeError, "transcript_limit"):
            state.consume(event("stt", text="x"))


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now


class FakeCodec:
    def __init__(self):
        self.closed = False
        self.inputs = []

    def encode(self, pcm):
        self.inputs.append(pcm)
        return b"encoded"

    def decode(self, opus):
        return struct.pack("<hhh", 0, 1200, -1600)

    def close(self):
        self.closed = True


class NetworkBudgetTests(unittest.TestCase):
    def network_type(self):
        clock = FakeClock()
        class DeadlineExpired(Exception):
            pass
        class Socket:
            timeout = 1.0
            def settimeout(self, value):
                self.timeout = value
        class WebSocket:
            def __init__(self):
                self.sock = Socket()
                self.frame_buffer = SimpleNamespace(header=None, length=None)
                self.cont_frame = SimpleNamespace(cont_data=None)
                self.read_calls = 0
            def _recv(self, size):
                self.read_calls += 1
                delay = min(0.04, self.sock.timeout)
                clock.now += delay
                if delay < 0.04:
                    raise DeadlineExpired()
                return b"x"
        module = SimpleNamespace(WebSocket=WebSocket, WebSocketTimeoutException=DeadlineExpired)
        return clock, DeadlineExpired, probe._deadline_websocket_type(module, clock)()

    def test_slow_multiple_reads_cannot_extend_a60ms_receive_slot(self):
        clock, expired, ws = self.network_type()
        ws.receive_deadline = 0.06
        self.assertEqual(ws._recv(1), b"x")
        with self.assertRaises(expired):
            ws._recv(1)
        with self.assertRaises(expired):
            ws._recv(1)
        self.assertEqual(ws.read_calls, 2)
        self.assertAlmostEqual(clock(), 0.06)

    def test_oversized_frame_and_continuation_rejected_before_payload_read(self):
        _, _, ws = self.network_type()
        ws.frame_buffer.header = (1, 0, 0, 0, 2, 0, 127)
        ws.frame_buffer.length = 1 << 40
        with self.assertRaisesRegex(probe.ProbeError, "websocket_message_limit"):
            ws._recv(16384)
        self.assertEqual(ws.read_calls, 0)
        ws.frame_buffer.header = (1, 0, 0, 0, 0, 0, 8)
        ws.frame_buffer.length = 8
        ws.cont_frame.cont_data = [2, b"x" * (probe.MESSAGE_MAX - 4)]
        with self.assertRaisesRegex(probe.ProbeError, "websocket_message_limit"):
            ws._recv(8)
        self.assertEqual(ws.read_calls, 0)
        ws.frame_buffer.header = (1, 0, 0, 0, 9, 0, 8)
        self.assertEqual(ws._recv(8), b"x")  # Interleaved ping is a separate message.


class FakeTransport:
    def __init__(self, clock, version=1, no_reply=False):
        self.clock, self.version, self.no_reply = clock, version, no_reply
        self.sent, self.incoming = [], []
        self.closed = False

    def send(self, message):
        self.sent.append((self.clock(), message))
        if isinstance(message, bytes):
            return
        obj = json.loads(message)
        if obj["type"] == "hello":
            self.incoming.append(hello(self.version))
        elif obj.get("state") == "stop" and not self.no_reply:
            self.incoming.extend((event("stt", text="synthetic test"), event("tts", state="start"),
                                  event("tts", state="sentence_start", text="synthetic reply"),
                                  probe.pack_audio(self.version, 0, b"response"), event("tts", state="stop")))

    def receive(self, timeout):
        if self.incoming:
            return self.incoming.pop(0)
        self.clock.now += timeout
        return None

    def close(self):
        self.closed = True


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.input = Path(self.tmp.name) / "input.wav"
        with wave.open(str(self.input), "wb") as wav:
            wav.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
            wav.writeframes(b"\x01\x00" * (960 * 2 + 1))
        self.config = probe.ConnectionConfig("wss://example.invalid/", "private-token", "private-d", "private-c")

    def tearDown(self):
        self.tmp.cleanup()

    def test_complete_turn_paces_upload_pads_last_frame_closes_and_saves_wav(self):
        clock, codec = FakeClock(), FakeCodec()
        transport = FakeTransport(clock)
        output = Path(self.tmp.name) / "answer.wav"
        result = probe.run_probe(self.config, self.input, output_wav=output,
                                 codec_factory=lambda: codec, transport_factory=lambda _: transport, clock=clock)
        self.assertTrue(result.safe_summary()["passed"])
        binary = [(t, msg) for t, msg in transport.sent if isinstance(msg, bytes)]
        self.assertEqual([round(t, 3) for t, _ in binary], [0, 0.06, 0.12])
        self.assertEqual(len(codec.inputs), 3)
        self.assertEqual(codec.inputs[-1], b"\x01\x00" + b"\0" * (probe.PCM_FRAME_BYTES - 2))
        sent_json = [json.loads(msg) for _, msg in transport.sent if isinstance(msg, str)]
        self.assertEqual([x.get("state") for x in sent_json], [None, "start", "stop"])
        self.assertFalse(sent_json[0]["features"]["mcp"])
        self.assertTrue(codec.closed and transport.closed)
        with wave.open(str(output), "rb") as wav:
            self.assertEqual((wav.getframerate(), wav.getnchannels(), wav.getsampwidth(), wav.getnframes()),
                             (16000, 1, 2, 3))

    def test_response_timeout_keeps_partial_safe_evidence_and_closes(self):
        clock, codec = FakeClock(), FakeCodec()
        transport = FakeTransport(clock, no_reply=True)
        with self.assertRaisesRegex(probe.ProbeError, "response_timeout") as caught:
            probe.run_probe(self.config, self.input, codec_factory=lambda: codec,
                            transport_factory=lambda _: transport, clock=clock)
        self.assertEqual(caught.exception.summary["tx_frames"], 3)
        self.assertFalse(caught.exception.summary["passed"])
        self.assertLessEqual(clock(), 90.181)
        self.assertTrue(codec.closed and transport.closed)
        self.assertNotIn("private", str(caught.exception.summary))
        self.assertEqual(list(Path(self.tmp.name).iterdir()), [self.input])

    def test_cancel_during_upload_closes_without_listen_stop(self):
        clock, codec = FakeClock(), FakeCodec()
        transport = FakeTransport(clock)
        with self.assertRaisesRegex(probe.ProbeError, "cancelled"):
            probe.run_probe(self.config, self.input, codec_factory=lambda: codec,
                            transport_factory=lambda _: transport, clock=clock,
                            cancelled=lambda: clock() >= 0.06)
        self.assertTrue(codec.closed and transport.closed)
        self.assertEqual(len(codec.inputs), 1)
        self.assertFalse(any('"stop"' in x for _, x in transport.sent if isinstance(x, str)))

    def test_network_error_does_not_echo_exception_tokens(self):
        codec = FakeCodec()
        def fail(_):
            raise RuntimeError("private-token leaked by dependency")
        with self.assertRaisesRegex(probe.ProbeError, "host_probe_failed") as caught:
            probe.run_probe(self.config, self.input, codec_factory=lambda: codec, transport_factory=fail)
        self.assertTrue(codec.closed)
        self.assertNotIn("private-token", str(caught.exception))

    def test_wrong_sample_rate_rejected_before_network(self):
        with wave.open(str(self.input), "wb") as wav:
            wav.setparams((1, 2, 24000, 0, "NONE", "not compressed"))
            wav.writeframes(b"\0\0" * 100)
        with self.assertRaisesRegex(probe.ProbeError, "input_requires_16k_mono_pcm16"):
            probe.run_probe(self.config, self.input, transport_factory=lambda _: self.fail("network called"))

    def test_dependency_logging_muted_and_restored_on_handshake_failure(self):
        logger = logging.getLogger("websocket")
        previous = logger.disabled
        attempted = []
        def fail_connect(*args, **kwargs):
            attempted.append(True)
            self.assertTrue(logger.disabled)
            logger.error("private-token raw header")
            raise RuntimeError("private-token raw header")
        fake = SimpleNamespace(enableTrace=lambda value: None, create_connection=fail_connect, WebSocket=object)
        with patch.dict(sys.modules, {"websocket": fake}):
            with self.assertRaisesRegex(probe.ProbeError, "websocket_connect_failed"):
                probe._WebSocketTransport(self.config)
        self.assertEqual(logger.disabled, previous)
        self.assertEqual(attempted, [True])

    def test_slow_encoding_does_not_trigger_catchup_audio_burst(self):
        clock = FakeClock()
        class SlowFirstCodec(FakeCodec):
            def encode(self, pcm):
                if not self.inputs:
                    clock.now += 0.2
                return super().encode(pcm)
        codec = SlowFirstCodec()
        transport = FakeTransport(clock)
        probe.run_probe(self.config, self.input, codec_factory=lambda: codec,
                        transport_factory=lambda _: transport, clock=clock)
        binary_times = [t for t, msg in transport.sent if isinstance(msg, bytes)]
        self.assertEqual([round(t, 3) for t in binary_times], [0.2, 0.26, 0.32])

    def test_redirects_never_forward_auth_and_non101_socket_is_closed(self):
        closed = []
        def connect(*args, **kwargs):
            self.assertEqual(kwargs["redirect_limit"], 0)
            return SimpleNamespace(getstatus=lambda: 302, shutdown=lambda: closed.append(True))
        fake = SimpleNamespace(enableTrace=lambda value: None, create_connection=connect, WebSocket=object)
        with patch.dict(sys.modules, {"websocket": fake}):
            with self.assertRaisesRegex(probe.ProbeError, "websocket_upgrade_rejected"):
                probe._WebSocketTransport(self.config)
        self.assertEqual(closed, [True])


if __name__ == "__main__":
    unittest.main()
