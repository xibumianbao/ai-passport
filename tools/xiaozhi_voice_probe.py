"""Bounded host-only Xiaozhi voice probe; no device discovery or credential files.

Call ``run_probe(ConnectionConfig(...), input_wav, opus_library=...)`` from a
trusted wrapper holding discovery credentials in memory. Input must be a
nonprivate 16 kHz, mono, signed 16-bit PCM WAV, at most 30 seconds. The module
never prints, invokes remote tools, changes agent configuration, or persists
transcripts. ``output_wav`` is opt-in and contains the spoken answer.

Dependencies: websocket-client and a local libopus shared library. Packet layout
follows FoloToy/folo-ai-passport-xiaozhi main/protocols/websocket_protocol.cc at
d24fce080d86d7cc642f71585f6efde40fb99104. This is an independent implementation.
"""

from __future__ import annotations

import array
import ctypes
from dataclasses import dataclass, field
import json
import logging
import math
from pathlib import Path
import socket
import struct
import sys
import time
from typing import Callable
from urllib.parse import urlsplit
import wave

RATE = 16000
FRAME_MS = 60
FRAME_SAMPLES = RATE * FRAME_MS // 1000
PCM_FRAME_BYTES = FRAME_SAMPLES * 2
OPUS_MAX = 1275
MESSAGE_MAX = 8192
INPUT_SECONDS_MAX = 30
RESPONSE_SECONDS_MAX = 90


class ProbeError(RuntimeError):
    """Only fixed codes and nonprivate statistics cross an error boundary."""

    def __init__(self, code: str, summary: dict | None = None):
        super().__init__(code)
        self.code = code
        self.summary = summary or {}


@dataclass(repr=False, frozen=True)
class ConnectionConfig:
    url: str
    token: str
    device_id: str
    client_id: str
    version: int = 1

    def __repr__(self):
        return f"ConnectionConfig(redacted=True, version={self.version})"

    def validate(self):
        if self.version not in (1, 2, 3):
            raise ProbeError("unsupported_protocol")
        if not isinstance(self.url, str) or len(self.url) > 2048:
            raise ProbeError("invalid_wss_url")
        try:
            parsed = urlsplit(self.url)
            valid = (parsed.scheme == "wss" and parsed.hostname and
                     not parsed.username and not parsed.password and
                     not parsed.fragment and (parsed.port is None or parsed.port > 0))
        except ValueError:
            valid = False
        if not valid or any(ord(c) <= 32 or ord(c) == 127 for c in self.url):
            raise ProbeError("invalid_wss_url")
        for value, limit in ((self.token, 4096), (self.device_id, 128),
                             (self.client_id, 128)):
            if (not isinstance(value, str) or len(value) > limit or
                    any(ord(c) < 32 or ord(c) > 126 for c in value)):
                raise ProbeError("invalid_header")
        if not self.device_id or not self.client_id:
            raise ProbeError("missing_identity")


def pack_audio(version: int, timestamp_ms: int, opus: bytes) -> bytes:
    if not 0 < len(opus) <= OPUS_MAX or not 0 <= timestamp_ms <= 0xFFFFFFFF:
        raise ProbeError("invalid_audio_packet")
    if version == 1:
        return opus
    if version == 2:
        return struct.pack("!HHIII", 2, 0, 0, timestamp_ms, len(opus)) + opus
    if version == 3:
        return struct.pack("!BBH", 0, 0, len(opus)) + opus
    raise ProbeError("unsupported_protocol")


def unpack_audio(version: int, packet: bytes) -> bytes:
    offset = {1: 0, 2: 16, 3: 4}.get(version)
    if offset is None:
        raise ProbeError("unsupported_protocol")
    if not offset < len(packet) <= offset + OPUS_MAX:
        raise ProbeError("invalid_audio_packet")
    if version == 2:
        v, kind, reserved, _timestamp, length = struct.unpack("!HHIII", packet[:16])
        if v != 2 or kind != 0 or reserved != 0 or length != len(packet) - 16:
            raise ProbeError("invalid_audio_header")
    if version == 3:
        kind, reserved, length = struct.unpack("!BBH", packet[:4])
        if kind or reserved or length != len(packet) - 4:
            raise ProbeError("invalid_audio_header")
    return packet[offset:]


def read_input(path: str | Path) -> bytes:
    try:
        with wave.open(str(path), "rb") as wav:
            if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(),
                    wav.getcomptype()) != (1, 2, RATE, "NONE"):
                raise ProbeError("input_requires_16k_mono_pcm16")
            count = wav.getnframes()
            if not 0 < count <= RATE * INPUT_SECONDS_MAX:
                raise ProbeError("input_duration_out_of_bounds")
            pcm = wav.readframes(count)
            if len(pcm) != count * 2:
                raise ProbeError("truncated_input_wav")
            return pcm
    except ProbeError:
        raise
    except (OSError, EOFError, wave.Error):
        raise ProbeError("invalid_input_wav") from None


class LibOpus:
    """Small ctypes adapter. Output is always 16 kHz regardless of server rate."""

    def __init__(self, library: str | Path):
        self.encoder = self.decoder = None
        try:
            self.lib = ctypes.CDLL(str(library))
            lib = self.lib
            lib.opus_encoder_create.argtypes = [ctypes.c_int32, ctypes.c_int,
                                                ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
            lib.opus_encoder_create.restype = ctypes.c_void_p
            lib.opus_decoder_create.argtypes = [ctypes.c_int32, ctypes.c_int,
                                                ctypes.POINTER(ctypes.c_int)]
            lib.opus_decoder_create.restype = ctypes.c_void_p
            lib.opus_encoder_destroy.argtypes = [ctypes.c_void_p]
            lib.opus_decoder_destroy.argtypes = [ctypes.c_void_p]
            lib.opus_encoder_ctl.argtypes = [ctypes.c_void_p, ctypes.c_int]
            lib.opus_encoder_ctl.restype = ctypes.c_int
            lib.opus_encode.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int16),
                                        ctypes.c_int, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int32]
            lib.opus_encode.restype = ctypes.c_int32
            lib.opus_decode.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ubyte),
                                        ctypes.c_int32, ctypes.POINTER(ctypes.c_int16),
                                        ctypes.c_int, ctypes.c_int]
            lib.opus_decode.restype = ctypes.c_int
            err = ctypes.c_int()
            self.encoder = lib.opus_encoder_create(RATE, 1, 2048, ctypes.byref(err))
            if err.value or not self.encoder:
                raise ProbeError("host_opus_encoder_init_failed")
            for request, value in ((4002, 24000), (4010, 0), (4006, 1), (4012, 0), (4016, 0)):
                if lib.opus_encoder_ctl(self.encoder, request, ctypes.c_int(value)):
                    raise ProbeError("host_opus_encoder_config_failed")
            self.decoder = lib.opus_decoder_create(RATE, 1, ctypes.byref(err))
            if err.value or not self.decoder:
                raise ProbeError("host_opus_decoder_init_failed")
        except ProbeError:
            self.close()
            raise
        except (OSError, AttributeError):
            self.close()
            raise ProbeError("host_opus_library_unavailable") from None

    def encode(self, pcm: bytes) -> bytes:
        if len(pcm) != PCM_FRAME_BYTES:
            raise ProbeError("invalid_pcm_frame")
        source = (ctypes.c_int16 * FRAME_SAMPLES).from_buffer_copy(pcm)
        dest = (ctypes.c_ubyte * OPUS_MAX)()
        count = self.lib.opus_encode(self.encoder, source, FRAME_SAMPLES, dest, OPUS_MAX)
        if not 0 < count <= OPUS_MAX:
            raise ProbeError("host_opus_encode_failed")
        return bytes(dest[:count])

    def decode(self, opus: bytes) -> bytes:
        if not 0 < len(opus) <= OPUS_MAX:
            raise ProbeError("invalid_audio_packet")
        source = (ctypes.c_ubyte * len(opus)).from_buffer_copy(opus)
        dest = (ctypes.c_int16 * (RATE * 120 // 1000))()
        count = self.lib.opus_decode(self.decoder, source, len(opus), dest, len(dest), 0)
        if not 0 < count <= len(dest):
            raise ProbeError("host_opus_decode_failed")
        return ctypes.string_at(dest, count * 2)

    def close(self):
        if self.encoder:
            self.lib.opus_encoder_destroy(self.encoder)
            self.encoder = None
        if self.decoder:
            self.lib.opus_decoder_destroy(self.decoder)
            self.decoder = None


@dataclass(repr=False)
class ProbeResult:
    protocol: int
    input_samples: int
    hello_received: bool = False
    server_sample_rate: int = 0
    tx_frames: int = 0
    tx_bytes: int = 0
    rx_frames: int = 0
    rx_bytes: int = 0
    output_samples: int = 0
    output_peak: int = 0
    output_sum_squares: int = 0
    output_clipped_samples: int = 0
    tts_started: bool = False
    tts_stopped: bool = False
    ignored_messages: int = 0
    elapsed_seconds: float = 0
    # Kept only in RAM for the caller's explicit semantic check, never in summary.
    stt_texts: list[str] = field(default_factory=list, repr=False)
    tts_texts: list[str] = field(default_factory=list, repr=False)

    def __repr__(self):
        return f"ProbeResult({self.safe_summary()!r})"

    def safe_summary(self) -> dict:
        rms = math.sqrt(self.output_sum_squares / self.output_samples) if self.output_samples else 0
        return {
            "passed": bool(self.hello_received and self.tx_frames and self.rx_frames and
                           self.stt_texts and self.tts_started and self.tts_stopped and rms > 0),
            "protocol": self.protocol, "hello_received": self.hello_received,
            "server_sample_rate": self.server_sample_rate, "output_sample_rate": RATE,
            "input_seconds": round(self.input_samples / RATE, 3),
            "output_seconds": round(self.output_samples / RATE, 3),
            "tx_frames": self.tx_frames, "tx_opus_bytes": self.tx_bytes,
            "rx_frames": self.rx_frames, "rx_opus_bytes": self.rx_bytes,
            "stt_present": bool(self.stt_texts), "tts_text_present": bool(self.tts_texts),
            "tts_started": self.tts_started, "tts_stopped": self.tts_stopped,
            "output_peak": self.output_peak, "output_rms": round(rms, 2),
            "output_clipped_samples": self.output_clipped_samples,
            "ignored_messages": self.ignored_messages,
            "elapsed_seconds": round(self.elapsed_seconds, 3),
        }


class ReceiveState:
    """A complete-message protocol consumer, also used by synthetic trace tests."""

    def __init__(self, result: ProbeResult, decode: Callable[[bytes], bytes], keep_audio=False):
        self.result, self.decode = result, decode
        self.session_id = ""
        self.response_enabled = False
        self.audio = bytearray() if keep_audio else None

    def consume(self, message: str | bytes):
        r = self.result
        if isinstance(message, bytes):
            if not self.response_enabled or not r.tts_started or r.tts_stopped:
                raise ProbeError("audio_outside_tts")
            opus = unpack_audio(r.protocol, message)
            pcm = self.decode(opus)
            if not 0 < len(pcm) <= RATE * 120 // 1000 * 2 or len(pcm) % 2:
                raise ProbeError("invalid_decoded_audio")
            samples = array.array("h", pcm)
            if sys.byteorder != "little":
                samples.byteswap()
            if r.output_samples + len(samples) > RATE * RESPONSE_SECONDS_MAX:
                raise ProbeError("response_audio_limit")
            r.rx_frames += 1
            r.rx_bytes += len(opus)
            r.output_samples += len(samples)
            r.output_peak = max(r.output_peak, max(abs(x) for x in samples))
            r.output_sum_squares += sum(x * x for x in samples)
            r.output_clipped_samples += sum(abs(x) >= 32767 for x in samples)
            if self.audio is not None:
                self.audio.extend(pcm)
            return
        if not isinstance(message, str) or len(message.encode("utf-8")) > MESSAGE_MAX:
            raise ProbeError("invalid_json_message_size")
        try:
            obj = json.loads(message)
        except (ValueError, RecursionError):
            raise ProbeError("invalid_json_message") from None
        if not isinstance(obj, dict):
            raise ProbeError("invalid_json_message")
        kind = obj.get("type")
        if kind == "hello":
            params = obj.get("audio_params")
            sid = obj.get("session_id")
            if (r.hello_received or obj.get("transport") != "websocket" or
                    not isinstance(sid, str) or not 0 < len(sid) <= 96 or
                    not isinstance(params, dict) or params.get("format") != "opus" or
                    params.get("channels") != 1 or params.get("frame_duration") != FRAME_MS or
                    params.get("sample_rate") not in (8000, 12000, 16000, 24000, 48000)):
                raise ProbeError("unsupported_server_hello")
            if "version" in obj and obj["version"] != r.protocol:
                raise ProbeError("protocol_version_mismatch")
            self.session_id = sid
            r.hello_received = True
            r.server_sample_rate = params["sample_rate"]
            return
        if not r.hello_received:
            raise ProbeError("message_before_hello")
        if "session_id" in obj and obj["session_id"] != self.session_id:
            raise ProbeError("session_mismatch")
        if kind == "stt":
            self._text(obj, r.stt_texts)
        elif kind == "tts":
            state = obj.get("state")
            if state == "start":
                if not self.response_enabled or r.tts_started:
                    raise ProbeError("unexpected_tts_start")
                r.tts_started = True
            elif state == "stop":
                if not r.tts_started or r.tts_stopped:
                    raise ProbeError("unexpected_tts_stop")
                r.tts_stopped = True
            elif state in ("sentence_start", "sentence_end"):
                if not r.tts_started or r.tts_stopped:
                    raise ProbeError("sentence_outside_tts")
                if state == "sentence_start":
                    self._text(obj, r.tts_texts)
            else:
                raise ProbeError("unsupported_tts_state")
        elif kind == "error":
            raise ProbeError("remote_error")
        else:
            # MCP / IoT / commands are deliberately never executed or replied to.
            r.ignored_messages += 1

    @staticmethod
    def _text(obj, destination):
        text = obj.get("text")
        if isinstance(text, str) and text.strip():
            if sum(len(x) for x in destination) + len(text) > MESSAGE_MAX * 2:
                raise ProbeError("transcript_limit")
            destination.append(text)


def _deadline_websocket_type(websocket, clock=time.monotonic):
    class DeadlineWebSocket(websocket.WebSocket):
        """Apply one deadline across all reads composing a WS message."""

        def __init__(self, *args, **kwargs):
            self.receive_deadline = None
            super().__init__(*args, **kwargs)

        def _recv(self, bufsize):
            if self.receive_deadline is not None:
                remaining = self.receive_deadline - clock()
                if remaining <= 0:
                    raise websocket.WebSocketTimeoutException("receive_slot_expired")
                self.sock.settimeout(remaining)
            # websocket-client buffers partial reads/continuations across a
            # timeout. Refuse an oversized length before reading its payload.
            frame = self.frame_buffer
            if frame.header is not None and frame.length is not None:
                opcode = frame.header[4]
                if opcode in (0, 1, 2):
                    previous = self.cont_frame.cont_data
                    used = len(previous[1]) if previous is not None else 0
                    if frame.length > MESSAGE_MAX - used:
                        raise ProbeError("websocket_message_limit")
            return super()._recv(bufsize)

    return DeadlineWebSocket


class _WebSocketTransport:
    def __init__(self, config: ConnectionConfig):
        try:
            import websocket
        except ImportError:
            raise ProbeError("websocket_client_unavailable") from None
        self.module = websocket
        websocket.enableTrace(False)
        # Dependency error/debug logging can include URLs or handshake headers.
        # This probe owns one connection at a time and restores caller logging.
        self.logger = logging.getLogger("websocket")
        self.previous_logging_disabled = self.logger.disabled
        self.logger.disabled = True
        headers = {"Protocol-Version": str(config.version), "Device-Id": config.device_id,
                   "Client-Id": config.client_id}
        if config.token:
            headers["Authorization"] = config.token if " " in config.token else "Bearer " + config.token
        try:
            self.ws = websocket.create_connection(config.url, class_=_deadline_websocket_type(websocket),
                                                   header=headers, timeout=10,
                                                   enable_multithread=False, redirect_limit=0)
            # Never forward discovery credentials to a redirected host, including
            # a downgrade to ws://. websocket-client needs an explicit status check
            # when its redirect loop is disabled.
            if self.ws.getstatus() != 101:
                self.ws.shutdown()
                raise ProbeError("websocket_upgrade_rejected")
        except ProbeError:
            self.logger.disabled = self.previous_logging_disabled
            raise
        except Exception:
            self.logger.disabled = self.previous_logging_disabled
            raise ProbeError("websocket_connect_failed") from None

    def send(self, message: str | bytes):
        self.ws.settimeout(3)
        try:
            if isinstance(message, bytes):
                self.ws.send_binary(message)
            else:
                self.ws.send(message)
        except Exception:
            raise ProbeError("websocket_send_failed") from None

    def receive(self, timeout: float) -> str | bytes | None:
        self.ws.receive_deadline = time.monotonic() + timeout
        self.ws.settimeout(max(0.001, timeout))
        try:
            opcode, data = self.ws.recv_data(control_frame=True)
        except (self.module.WebSocketTimeoutException, socket.timeout):
            return None
        except ProbeError:
            raise
        except Exception:
            raise ProbeError("websocket_receive_failed") from None
        finally:
            self.ws.receive_deadline = None
        if opcode == self.module.ABNF.OPCODE_CLOSE:
            raise ProbeError("websocket_closed")
        if opcode == self.module.ABNF.OPCODE_BINARY:
            return data
        if opcode == self.module.ABNF.OPCODE_TEXT:
            try:
                return data.decode("utf-8") if isinstance(data, bytes) else data
            except UnicodeDecodeError:
                raise ProbeError("invalid_utf8_message") from None
        return None  # websocket-client answers ping frames internally.

    def close(self):
        try:
            self.ws.close(timeout=1)
        finally:
            self.logger.disabled = self.previous_logging_disabled


def run_probe(config: ConnectionConfig, input_wav: str | Path, *,
              opus_library: str | Path | None = None, output_wav: str | Path | None = None,
              transport_factory=None, codec_factory=None, clock=time.monotonic,
              cancelled: Callable[[], bool] = lambda: False) -> ProbeResult:
    """Run one manual voice turn. Factories are injection points for offline tests.

    ``ProbeError.code`` and ``ProbeError.summary`` are safe to report. Credentials,
    raw server JSON and transcripts are intentionally omitted from summaries.
    A successful handshake alone never makes ``safe_summary()['passed']`` true.
    """
    config.validate()
    pcm = read_input(input_wav)
    result = ProbeResult(config.version, len(pcm) // 2)
    started = clock()
    codec = transport = None
    try:
        def check_cancelled():
            if cancelled():
                raise ProbeError("cancelled")

        check_cancelled()
        codec = codec_factory() if codec_factory else LibOpus(opus_library or "")
        state = ReceiveState(result, codec.decode, keep_audio=output_wav is not None)
        transport = (transport_factory or _WebSocketTransport)(config)

        def send_json(obj):
            check_cancelled()
            transport.send(json.dumps(obj, separators=(",", ":")))

        def receive_until(deadline, finished):
            while not finished():
                check_cancelled()
                remaining = deadline - clock()
                if remaining <= 0:
                    return False
                message = transport.receive(min(remaining, 0.25))
                if message is not None:
                    state.consume(message)
            return True

        send_json({"type": "hello", "version": config.version,
                   "features": {"mcp": False}, "transport": "websocket",
                   "audio_params": {"format": "opus", "sample_rate": RATE,
                                    "channels": 1, "frame_duration": FRAME_MS}})
        if not receive_until(clock() + 10, lambda: result.hello_received):
            raise ProbeError("hello_timeout")
        send_json({"session_id": state.session_id, "type": "listen", "state": "start", "mode": "manual"})
        audio_start = clock()
        for offset in range(0, len(pcm), PCM_FRAME_BYTES):
            check_cancelled()
            source = pcm[offset:offset + PCM_FRAME_BYTES].ljust(PCM_FRAME_BYTES, b"\0")
            opus = codec.encode(source)
            frame_sent_at = clock()
            transport.send(pack_audio(config.version, result.tx_frames * FRAME_MS, opus))
            result.tx_frames += 1
            result.tx_bytes += len(opus)
            # Drain incoming events while retaining real-time 60 ms upload pacing.
            # Start a fresh slot if transport/encoding ever overruns. Do not
            # burst queued frames to catch up with an old absolute schedule.
            receive_until(frame_sent_at + FRAME_MS / 1000, lambda: False)
            if clock() - audio_start > INPUT_SECONDS_MAX + 5:
                raise ProbeError("upload_timeout")
        state.response_enabled = True
        send_json({"session_id": state.session_id, "type": "listen", "state": "stop"})
        if not receive_until(clock() + RESPONSE_SECONDS_MAX, lambda: result.tts_stopped):
            raise ProbeError("response_timeout")
        if output_wav is not None and state.audio:
            try:
                with wave.open(str(output_wav), "wb") as wav:
                    wav.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
                    wav.writeframes(state.audio)
            except (OSError, wave.Error):
                raise ProbeError("output_wav_write_failed") from None
        return result
    except ProbeError as error:
        result.elapsed_seconds = clock() - started
        error.summary = result.safe_summary()
        raise
    except Exception:
        result.elapsed_seconds = clock() - started
        raise ProbeError("host_probe_failed", result.safe_summary()) from None
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
    raise SystemExit("Import run_probe from an authorized in-memory wrapper; credentials are not CLI arguments.")
