"""Bounded, host-only automatic half-duplex Xiaozhi protocol probe.

Credentials and semantic transcripts remain in RAM. This module never discovers
a device, executes MCP, changes cloud settings, or sends ``listen/stop``. Each
turn relies on a server TTS/start event to end the upload. STT has no finality
contract and is recorded without stopping microphone-equivalent input. Only an explicit
``output_wavs`` argument saves answer audio locally. Offline injected traces test
protocol sequencing; they are not evidence of cloud or device audio acceptance.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import json
from pathlib import Path
import time
from typing import Callable
import wave

from xiaozhi_voice_probe import (
    ConnectionConfig, FRAME_MS, LibOpus, PCM_FRAME_BYTES, ProbeError, ProbeResult,
    RATE, RESPONSE_SECONDS_MAX, ReceiveState, _WebSocketTransport, pack_audio,
    read_input,
)

ROUNDS_MAX = 3
SILENCE_SECONDS_MAX = 8
SILENCE_FRAMES_MAX = SILENCE_SECONDS_MAX * 1000 // FRAME_MS
EVENTS_MAX = 256


@dataclass(repr=False)
class AutoRoundResult(ProbeResult):
    round_index: int = 0
    input_frames: int = 0
    silence_frames: int = 0
    upload_stop_reason: str = ""

    def safe_summary(self):
        summary = super().safe_summary()
        summary.update({
            "passed": summary["passed"] and self.upload_stop_reason == "tts_start",
            "round": self.round_index, "mode": "auto",
            "input_frames": self.input_frames, "silence_frames": self.silence_frames,
            "silence_seconds": round(self.silence_frames * FRAME_MS / 1000, 3),
            "upload_stop_reason": self.upload_stop_reason,
        })
        return summary


@dataclass(repr=False)
class AutoProbeResult:
    protocol: int
    requested_rounds: int
    hello_received: bool = False
    server_sample_rate: int = 0
    rounds: list[AutoRoundResult] = field(default_factory=list)
    # Event names are local constants, never copied from arbitrary server JSON.
    events: list[dict] = field(default_factory=list)
    elapsed_seconds: float = 0
    completed: bool = False

    def __repr__(self):
        return f"AutoProbeResult({self.safe_summary()!r})"

    def safe_summary(self):
        rounds = [turn.safe_summary() for turn in self.rounds]
        return {
            "passed": bool(self.completed and self.hello_received and len(rounds) == self.requested_rounds
                           and all(turn["passed"] for turn in rounds)),
            "scope": "host_auto_voice_probe_not_device_audio_acceptance",
            "mode": "auto", "protocol": self.protocol,
            "requested_rounds": self.requested_rounds,
            "completed": self.completed,
            "completed_rounds": sum(turn.tts_stopped for turn in self.rounds),
            "hello_received": self.hello_received, "server_sample_rate": self.server_sample_rate,
            "tx_frames": sum(turn.tx_frames for turn in self.rounds),
            "rx_frames": sum(turn.rx_frames for turn in self.rounds),
            "rounds": rounds, "events": [dict(event) for event in self.events],
            "elapsed_seconds": round(self.elapsed_seconds, 3),
        }


class _AutoReceiveState(ReceiveState):
    def __init__(self, result, decode, note, keep_audio=False):
        super().__init__(result, decode, keep_audio)
        self.note = note

    def consume(self, message):
        first_audio = isinstance(message, bytes) and self.result.rx_frames == 0
        super().consume(message)  # Reuse all size, session, TTS and decode bounds.
        if isinstance(message, bytes):
            if first_audio:
                self.note("audio_first")
            return
        obj = json.loads(message)  # Already validated by ReceiveState.
        kind, state = obj.get("type"), obj.get("state")
        if kind == "stt":
            self.note("stt")
        elif kind == "tts" and state == "start":
            if not self.result.upload_stop_reason:
                self.result.upload_stop_reason = "tts_start"
                self.note("upload_stopped_by_tts_start")
            self.note("tts_start")
        elif kind == "tts" and state == "stop":
            self.note("tts_stop")


def run_auto_probe(config: ConnectionConfig, input_wavs: list[str | Path], *,
                   opus_library: str | Path | None = None,
                   output_wavs: list[str | Path | None] | None = None,
                   transport_factory=None, codec_factory=None, clock=time.monotonic,
                   cancelled: Callable[[], bool] = lambda: False) -> AutoProbeResult:
    """Run 1–3 automatic turns over one WSS connection and session.

    Each input is <=30 s PCM16/16 kHz/mono, followed by <=8 s encoded silence.
    TTS/start stops upload immediately when consumed. STT is recorded but cannot
    stop upload because the protocol has no STT finality contract. No listen/stop is
    sent. The next turn starts only after validated TTS/stop. Response wait is
    <=90 s per turn, upload wall-clock overhead <=5 s, receive slots <=250 ms.
    Existing transport also bounds connect/send/close to 10/3/1 s. Cancellation
    is checked between all operations, with no background worker to leave alive.

    Texts remain in ``result.rounds[i].stt_texts/tts_texts`` for the caller's
    semantic check. Only safe_summary(), ProbeError.code and ProbeError.summary
    are suitable for logs. Injected factories are offline test seams.
    """
    config.validate()
    if not isinstance(input_wavs, list) or not 1 <= len(input_wavs) <= ROUNDS_MAX:
        raise ProbeError("auto_round_count_out_of_bounds")
    outputs = output_wavs if output_wavs is not None else [None] * len(input_wavs)
    if not isinstance(outputs, list) or len(outputs) != len(input_wavs):
        raise ProbeError("auto_output_count_mismatch")
    try:
        output_paths = [Path(path).resolve() for path in outputs if path is not None]
        input_paths = {Path(path).resolve() for path in input_wavs}
        if len(set(output_paths)) != len(output_paths) or any(p in input_paths for p in output_paths):
            raise ProbeError("auto_output_path_conflict")
    except (TypeError, ValueError, OSError):
        raise ProbeError("invalid_output_wav_path") from None

    result = AutoProbeResult(config.version, len(input_wavs))
    started = clock()
    codec = transport = active_round = None
    round_started = started
    try:
        def check_cancelled():
            if cancelled():
                raise ProbeError("cancelled")

        def note(event, turn=0):
            if len(result.events) >= EVENTS_MAX:
                raise ProbeError("auto_event_limit")
            result.events.append({"round": turn, "event": event,
                                  "elapsed_ms": round((clock() - started) * 1000)})

        def send_json(obj):
            check_cancelled()
            transport.send(json.dumps(obj, separators=(",", ":")))

        def receive_until(state, deadline, finished):
            while not finished():
                check_cancelled()
                remaining = deadline - clock()
                if remaining <= 0:
                    return False
                message = transport.receive(min(remaining, 0.25))
                check_cancelled()
                if message is not None:
                    state.consume(message)
            return True

        check_cancelled()
        inputs = []
        for path in input_wavs:
            check_cancelled()
            inputs.append(read_input(path))
        transport = (transport_factory or _WebSocketTransport)(config)
        handshake_result = ProbeResult(config.version, 0)
        handshake = ReceiveState(handshake_result, lambda packet: b"")
        send_json({"type": "hello", "version": config.version, "features": {"mcp": False},
                   "transport": "websocket", "audio_params": {"format": "opus", "sample_rate": RATE,
                   "channels": 1, "frame_duration": FRAME_MS}})
        if not receive_until(handshake, clock() + 10, lambda: handshake_result.hello_received):
            raise ProbeError("hello_timeout")
        result.hello_received = True
        result.server_sample_rate = handshake_result.server_sample_rate
        note("hello")
        # Monotonic packet timestamps span all turns; decoder and encoder state
        # are recreated only after the previous response has fully stopped.
        total_tx_frames = 0
        for index, (pcm, output_path) in enumerate(zip(inputs, outputs), 1):
            check_cancelled()
            round_started = clock()
            active_round = AutoRoundResult(config.version, len(pcm) // 2,
                                           hello_received=True,
                                           server_sample_rate=result.server_sample_rate,
                                           round_index=index)
            result.rounds.append(active_round)
            codec = codec_factory() if codec_factory else LibOpus(opus_library or "")
            state = _AutoReceiveState(active_round, codec.decode,
                                      lambda event, index=index: note(event, index),
                                      keep_audio=output_path is not None)
            state.session_id = handshake.session_id
            state.response_enabled = True
            send_json({"session_id": state.session_id, "type": "listen", "state": "start", "mode": "auto"})
            note("listen_start_auto", index)
            upload_started = clock()
            upload_deadline = upload_started + len(pcm) / (RATE * 2) + SILENCE_SECONDS_MAX + 5
            offset = 0
            silence_started = None
            while not active_round.upload_stop_reason:
                check_cancelled()
                if clock() >= upload_deadline:
                    raise ProbeError("upload_timeout")
                # Drain already-arriving events before encoding another frame;
                # only TTS/start establishes the authoritative upload endpoint.
                receive_until(state, clock() + 0.001, lambda: bool(active_round.upload_stop_reason))
                if active_round.upload_stop_reason:
                    break
                is_input = offset < len(pcm)
                if is_input:
                    source = pcm[offset:offset + PCM_FRAME_BYTES].ljust(PCM_FRAME_BYTES, b"\0")
                    offset += PCM_FRAME_BYTES
                else:
                    if silence_started is None:
                        silence_started = clock()
                        note("silence_begin", index)
                    if (active_round.silence_frames >= SILENCE_FRAMES_MAX or
                            clock() - silence_started >= SILENCE_SECONDS_MAX):
                        raise ProbeError("auto_endpoint_timeout")
                    source = bytes(PCM_FRAME_BYTES)
                encoded = codec.encode(source)
                check_cancelled()
                frame_sent_at = clock()
                transport.send(pack_audio(config.version, total_tx_frames * FRAME_MS, encoded))
                total_tx_frames += 1
                active_round.tx_frames += 1
                active_round.tx_bytes += len(encoded)
                if is_input:
                    active_round.input_frames += 1
                else:
                    active_round.silence_frames += 1
                receive_until(state, frame_sent_at + FRAME_MS / 1000,
                              lambda: bool(active_round.upload_stop_reason))
            # Endpoint is exclusively a server event, never a client stop frame.
            if not receive_until(state, clock() + RESPONSE_SECONDS_MAX, lambda: active_round.tts_stopped):
                raise ProbeError("response_timeout")
            check_cancelled()
            if output_path is not None and state.audio:
                try:
                    with wave.open(str(output_path), "wb") as wav:
                        wav.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
                        wav.writeframes(state.audio)
                except (OSError, wave.Error):
                    raise ProbeError("output_wav_write_failed") from None
            active_round.elapsed_seconds = clock() - round_started
            codec.close()
            codec = None
        result.completed = True
        return result
    except ProbeError as error:
        result.elapsed_seconds = clock() - started
        if active_round is not None:
            active_round.elapsed_seconds = clock() - round_started
        error.summary = result.safe_summary()
        raise
    except Exception:
        result.elapsed_seconds = clock() - started
        if active_round is not None:
            active_round.elapsed_seconds = clock() - round_started
        raise ProbeError("host_auto_probe_failed", result.safe_summary()) from None
    finally:
        result.elapsed_seconds = clock() - started
        if transport is not None:
            try:
                transport.close()
            except Exception:
                pass
        if codec is not None:
            try:
                codec.close()
            except Exception:
                pass


if __name__ == "__main__":
    raise SystemExit("Import run_auto_probe from an authorized in-memory wrapper; credentials are not CLI arguments.")
