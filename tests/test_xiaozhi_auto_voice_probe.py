"""Offline server traces for automatic turn boundaries, not cloud acceptance."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import xiaozhi_auto_voice_probe as auto
import xiaozhi_voice_probe as wire


def event(kind, **values):
    return json.dumps({"type": kind, "session_id": "synthetic-session", **values})


def hello(version):
    return json.dumps({"type": "hello", "version": version, "transport": "websocket",
                       "session_id": "synthetic-session", "audio_params": {
                           "format": "opus", "sample_rate": 24000, "channels": 1,
                           "frame_duration": 60}})


class Clock:
    now = 0.0

    def __call__(self):
        return self.now


class Codec:
    def __init__(self):
        self.inputs = []
        self.decoded = []
        self.closed = False

    def encode(self, pcm):
        assert len(pcm) == wire.PCM_FRAME_BYTES
        assert not self.closed
        self.inputs.append(pcm)
        return b"encoded-silence" if not any(pcm) else b"encoded-voice"

    def decode(self, packet):
        assert not self.closed
        self.decoded.append(packet)
        return struct.pack("<hhh", 0, 1200, -1600)

    def close(self):
        self.closed = True


class ScriptTransport:
    """Timed server trace triggered by audio, never by a client stop command.

    Scenarios deliberately deliver partial STT, reordered STT/TTS, missing
    boundaries, disconnects, and late audio to exercise actual state rejection.
    """
    def __init__(self, clock, scenarios, version=1):
        self.clock, self.scenarios, self.version = clock, scenarios, version
        self.sent, self.incoming, self.received_events = [], [], []
        self.round = 0
        self.frames = {}
        self.closed = False

    def queue(self, delay, message):
        self.incoming.append((self.clock() + delay, message))
        self.incoming.sort(key=lambda item: item[0])

    def answer(self, spec):
        self.queue(0.005, event("tts", state="start"))
        if spec.get("stt_after_tts"):
            self.queue(0.006, event("stt", text="synthetic private request"))
        if spec.get("reply_text", True):
            self.queue(0.007, event("tts", state="sentence_start", text="synthetic private answer"))
        if spec.get("audio", True):
            self.queue(0.010, wire.pack_audio(self.version, 0, b"reply"))
        if not spec.get("no_stop"):
            self.queue(spec.get("stop_delay", 0.2), event("tts", state="stop"))

    def send(self, message):
        self.sent.append((self.clock(), self.round, message))
        if isinstance(message, str):
            obj = json.loads(message)
            if obj["type"] == "hello":
                self.queue(0, hello(self.version))
            elif obj["type"] == "listen" and obj["state"] == "start":
                self.round += 1
                self.frames[self.round] = 0
                spec = self.scenarios[self.round - 1]
                if spec.get("tts_on_listen"):
                    self.answer(spec)
                if spec.get("mcp"):
                    self.queue(0, event("mcp", payload={"method": "tools/call", "secret": "TOKEN-PRIVATE"}))
            # A client stop command has no simulated effect. A probe that needs
            # it cannot manufacture endpoint or response success in these tests.
            return
        self.frames[self.round] += 1
        count = self.frames[self.round]
        spec = self.scenarios[self.round - 1]
        if count == spec.get("stt_at", 1):
            self.queue(0, event("stt", text=spec.get("stt_text", "synthetic private request")))
        if count == spec.get("tts_at", 2):
            self.answer(spec)
        if count == spec.get("disconnect_at"):
            self.queue(0, wire.ProbeError("websocket_closed"))
        if count == spec.get("error_at"):
            self.queue(0, event("error", message="TOKEN-PRIVATE hidden credential"))
        if count == spec.get("bad_session_at"):
            self.queue(0, json.dumps({"type": "stt", "session_id": "wrong", "text": "private"}))
        if count == spec.get("audio_before_tts_at"):
            self.queue(0, wire.pack_audio(self.version, 0, b"reply"))

    def receive(self, timeout):
        until = self.clock() + timeout
        if not self.incoming or self.incoming[0][0] > until:
            self.clock.now = until
            return None
        at, message = self.incoming.pop(0)
        self.clock.now = max(at, self.clock())
        if isinstance(message, Exception):
            raise message
        if isinstance(message, str):
            obj = json.loads(message)
            self.received_events.append((self.clock(), self.round, obj.get("type"), obj.get("state")))
        return message

    def close(self):
        self.closed = True


class AutomaticProbeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.input = self.root / "synthetic.wav"
        with wave.open(str(self.input), "wb") as wav:
            wav.setparams((1, 2, wire.RATE, 0, "NONE", "not compressed"))
            wav.writeframes(struct.pack("<h", 1700) * wire.FRAME_SAMPLES * 5)
        self.clock = Clock()
        self.codecs = []

    def config(self, version=1):
        return wire.ConnectionConfig("wss://example.invalid/voice?secret=TOKEN-PRIVATE",
                                     "TOKEN-PRIVATE", "synthetic-device", "synthetic-client", version)

    def codec(self):
        # No two codec contexts should remain live at a round boundary.
        self.assertTrue(all(item.closed for item in self.codecs))
        codec = Codec()
        self.codecs.append(codec)
        return codec

    def run_scenarios(self, scenarios, version=1, **kwargs):
        self.transport = ScriptTransport(self.clock, scenarios, version)
        return auto.run_auto_probe(self.config(version), [self.input] * len(scenarios),
                                   transport_factory=lambda config: self.transport,
                                   codec_factory=self.codec, clock=self.clock, **kwargs)

    def sent_json(self):
        return [(at, json.loads(message)) for at, _, message in self.transport.sent if isinstance(message, str)]

    def test_early_stt_does_not_stop_upload_and_tts_start_does(self):
        result = self.run_scenarios([{"stt_at": 1, "tts_at": 3}])
        turn = result.rounds[0]
        self.assertEqual(turn.tx_frames, 3)
        self.assertEqual(turn.input_frames, 3)
        self.assertEqual(turn.upload_stop_reason, "tts_start")
        self.assertEqual(turn.silence_frames, 0)
        self.assertTrue(result.safe_summary()["passed"])
        names = [item["event"] for item in result.events]
        self.assertLess(names.index("stt"), names.index("upload_stopped_by_tts_start"))
        self.assertFalse(any(obj.get("state") == "stop" for _, obj in self.sent_json()))

    def test_tts_can_arrive_before_stt_and_immediately_stops_remaining_input(self):
        result = self.run_scenarios([{"stt_at": None, "tts_at": 1, "stt_after_tts": True}])
        self.assertEqual(result.rounds[0].tx_frames, 1)
        self.assertTrue(result.safe_summary()["passed"])
        names = [item["event"] for item in result.events]
        self.assertLess(names.index("tts_start"), names.index("stt"))

    def test_two_rounds_reuse_one_session_and_wait_for_stop_before_relisten(self):
        result = self.run_scenarios([{"tts_at": 3, "stop_delay": 0.8}, {"tts_at": 1}])
        self.assertTrue(result.safe_summary()["passed"])
        self.assertEqual([turn.tx_frames for turn in result.rounds], [3, 1])
        self.assertEqual([turn.rx_frames for turn in result.rounds], [1, 1])
        self.assertEqual(len(self.codecs), 2)
        self.assertTrue(all(codec.closed for codec in self.codecs))
        self.assertTrue(self.transport.closed)
        sent = self.sent_json()
        self.assertEqual(sum(obj["type"] == "hello" for _, obj in sent), 1)
        starts = [(at, obj) for at, obj in sent if obj["type"] == "listen"]
        self.assertEqual(len(starts), 2)
        self.assertTrue(all(obj["mode"] == "auto" and obj["state"] == "start" for _, obj in starts))
        self.assertEqual({obj["session_id"] for _, obj in starts}, {"synthetic-session"})
        stop_at = next(at for at, turn, kind, state in self.transport.received_events
                       if turn == 1 and kind == "tts" and state == "stop")
        self.assertGreaterEqual(starts[1][0], stop_at)

    def test_all_protocol_versions_and_monotonic_second_round_timestamps(self):
        for version in (1, 2, 3):
            self.clock = Clock()
            self.codecs = []
            result = self.run_scenarios([{"tts_at": 2}, {"tts_at": 1}], version=version)
            self.assertTrue(result.safe_summary()["passed"])
            packets = [m for _, _, m in self.transport.sent if isinstance(m, bytes)]
            self.assertEqual([wire.unpack_audio(version, packet) for packet in packets], [b"encoded-voice"] * 3)
            if version == 2:
                self.assertEqual([struct.unpack_from("!I", packet, 8)[0] for packet in packets], [0, 60, 120])

    def test_input_is_followed_by_real_silence_frames_until_tts_start(self):
        result = self.run_scenarios([{"stt_at": 6, "tts_at": 8}])
        turn = result.rounds[0]
        self.assertEqual((turn.input_frames, turn.silence_frames), (5, 3))
        self.assertEqual(self.codecs[0].inputs[5:], [bytes(wire.PCM_FRAME_BYTES)] * 3)
        self.assertTrue(result.safe_summary()["passed"])

    def test_stt_without_tts_times_out_without_synthetic_listen_stop(self):
        with self.assertRaisesRegex(wire.ProbeError, "auto_endpoint_timeout") as error:
            self.run_scenarios([{"stt_at": 1, "tts_at": None}])
        turn = error.exception.summary["rounds"][0]
        self.assertTrue(turn["stt_present"])
        self.assertEqual(turn["upload_stop_reason"], "")
        self.assertGreater(turn["silence_frames"], 100)
        self.assertLessEqual(turn["silence_seconds"], 8)
        self.assertLess(self.clock(), 9)
        self.assertFalse(error.exception.summary["passed"])
        self.assertTrue(self.codecs[0].closed and self.transport.closed)
        self.assertFalse(any(obj.get("state") == "stop" for _, obj in self.sent_json()))

    def test_no_speech_times_out_and_does_not_start_second_round(self):
        with self.assertRaisesRegex(wire.ProbeError, "auto_endpoint_timeout") as error:
            self.run_scenarios([{"stt_at": None, "tts_at": None}, {}])
        self.assertEqual(error.exception.summary["completed_rounds"], 0)
        self.assertEqual(self.transport.round, 1)
        self.assertFalse(error.exception.summary["rounds"][0]["stt_present"])

    def test_response_without_tts_stop_times_out_before_next_listen(self):
        with self.assertRaisesRegex(wire.ProbeError, "response_timeout") as error:
            self.run_scenarios([{"no_stop": True}, {}])
        self.assertTrue(error.exception.summary["rounds"][0]["tts_started"])
        self.assertFalse(error.exception.summary["rounds"][0]["tts_stopped"])
        self.assertEqual(self.transport.round, 1)
        self.assertLess(self.clock(), 91)

    def test_tts_stop_without_audio_or_empty_stt_cannot_pass(self):
        for scenario in ({"audio": False}, {"stt_text": "  "}):
            self.clock = Clock()
            self.codecs = []
            result = self.run_scenarios([scenario])
            self.assertFalse(result.safe_summary()["passed"])
            self.assertTrue(result.rounds[0].tts_stopped)

    def test_disconnect_closes_resources_and_keeps_partial_statistics(self):
        with self.assertRaisesRegex(wire.ProbeError, "websocket_closed") as error:
            self.run_scenarios([{"disconnect_at": 1, "tts_at": None}])
        self.assertEqual(error.exception.summary["tx_frames"], 1)
        self.assertFalse(error.exception.summary["passed"])
        self.assertTrue(self.transport.closed and self.codecs[0].closed)

    def test_cancel_during_response_closes_and_prevents_relisten(self):
        with self.assertRaisesRegex(wire.ProbeError, "cancelled"):
            self.run_scenarios([{"stop_delay": 2}, {}], cancelled=lambda: self.clock() >= 0.2)
        self.assertLess(self.clock(), 0.6)
        self.assertEqual(self.transport.round, 1)
        self.assertTrue(self.transport.closed and self.codecs[0].closed)

    def test_token_and_transcripts_stay_out_of_summary_repr_errors_and_mcp(self):
        result = self.run_scenarios([{"mcp": True}])
        self.assertIn("synthetic private request", result.rounds[0].stt_texts)
        self.assertIn("synthetic private answer", result.rounds[0].tts_texts)
        public = json.dumps(result.safe_summary()) + repr(result) + repr(result.rounds[0]) + repr(self.config())
        for secret in ("TOKEN-PRIVATE", "synthetic-session", "synthetic-device", "synthetic private"):
            self.assertNotIn(secret, public)
        self.assertEqual(result.rounds[0].ignored_messages, 1)
        self.assertEqual([obj["type"] for _, obj in self.sent_json()], ["hello", "listen"])
        self.assertFalse(any(p.suffix == ".json" for p in self.root.iterdir()))
        self.clock = Clock()
        self.codecs = []
        with self.assertRaisesRegex(wire.ProbeError, "remote_error") as error:
            self.run_scenarios([{"error_at": 1}])
        self.assertNotIn("TOKEN-PRIVATE", str(error.exception) + json.dumps(error.exception.summary))

    def test_unexpected_dependency_exception_is_sanitized(self):
        def fail(config):
            raise RuntimeError("TOKEN-PRIVATE in URL/header")
        with self.assertRaisesRegex(wire.ProbeError, "host_auto_probe_failed") as error:
            auto.run_auto_probe(self.config(), [self.input], transport_factory=fail, clock=self.clock)
        self.assertNotIn("TOKEN-PRIVATE", str(error.exception) + json.dumps(error.exception.summary))

    def test_wrong_session_and_audio_before_tts_are_rejected(self):
        for scenario, code in (({"bad_session_at": 1}, "session_mismatch"),
                               ({"audio_before_tts_at": 1}, "audio_outside_tts")):
            self.clock = Clock()
            self.codecs = []
            with self.assertRaisesRegex(wire.ProbeError, code):
                self.run_scenarios([scenario])
            self.assertTrue(self.transport.closed and self.codecs[0].closed)

    def test_three_turn_bound_and_output_path_validation_before_connect(self):
        for inputs in ([], [self.input] * 4):
            with self.assertRaisesRegex(wire.ProbeError, "auto_round_count_out_of_bounds"):
                auto.run_auto_probe(self.config(), inputs)
        with self.assertRaisesRegex(wire.ProbeError, "auto_output_count_mismatch"):
            auto.run_auto_probe(self.config(), [self.input], output_wavs=[])
        with self.assertRaisesRegex(wire.ProbeError, "auto_output_path_conflict"):
            auto.run_auto_probe(self.config(), [self.input], output_wavs=[self.input])

    def test_output_audio_is_only_written_on_explicit_opt_in(self):
        output = self.root / "reply.wav"
        result = self.run_scenarios([{}], output_wavs=[output])
        with wave.open(str(output), "rb") as wav:
            self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()), (1, 2, 16000))
            self.assertEqual(wav.getnframes(), result.rounds[0].output_samples)
        self.assertEqual({p.name for p in self.root.iterdir()}, {"synthetic.wav", "reply.wav"})


if __name__ == "__main__":
    unittest.main()
