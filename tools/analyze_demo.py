"""Numerical sanity check of the rendered demo WAV.

Reads the 24-bit PCM file, reports level stats and compares the strongest
spectral peaks against the notes that were scheduled by the render test.
"""
import struct
import sys
import wave

import numpy as np

path = sys.argv[1] if len(sys.argv) > 1 else "dist/NeonRift-demo.wav"

with wave.open(path, "rb") as w:
    channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(frames)

assert width == 3, f"expected 24-bit, got {width * 8}-bit"

data = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
# little-endian 24-bit -> int32
vals = (data[:, 0].astype(np.int32) | (data[:, 1].astype(np.int32) << 8) | (data[:, 2].astype(np.int32) << 16))
vals = np.where(vals >= 1 << 23, vals - (1 << 24), vals).astype(np.float32) / float(1 << 23)
audio = vals.reshape(-1, channels)

print(f"file      : {path}")
print(f"format    : {rate} Hz, {channels} ch, {frames / rate:.2f} s")
print(f"peak      : {np.abs(audio).max():.4f}")
print(f"rms       : {np.sqrt((audio ** 2).mean()):.4f}")
print(f"dc offset : {audio.mean(axis=0)}")

# spectrum of a window inside the held chord (starts at 4.2 s)
start = int(4.6 * rate)
window = audio[start:start + 16384, 0] * np.hanning(16384)
spec = np.abs(np.fft.rfft(window))
freqs = np.fft.rfftfreq(16384, 1.0 / rate)

peaks = []
for i in range(1, len(spec) - 1):
    if spec[i] > spec[i - 1] and spec[i] > spec[i + 1]:
        peaks.append((spec[i], freqs[i]))
peaks.sort(reverse=True)

print("top spectral peaks (chord region):")
for mag, f in peaks[:8]:
    print(f"  {f:8.1f} Hz   magnitude {mag:10.1f}")

expected = [72.4, 130.8, 196.0, 261.6, 329.6, 392.0]  # C2 G2 G3 C4 E4 G4
found = [min(expected, key=lambda e: abs(e - f)) for _, f in peaks[:6]]
matched = sum(1 for e, f in zip(sorted(found), expected) if abs(e - f) < 6)
print(f"chord notes matched: {matched}/{len(expected)}")

# 3 s of release tail should be quiet
tail = audio[-int(0.2 * rate):]
print(f"tail rms  : {np.sqrt((tail ** 2).mean()):.5f}")
