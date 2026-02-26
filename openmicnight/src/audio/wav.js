function clamp(x, lo, hi) {
  return Math.min(hi, Math.max(lo, x));
}

function writeAscii(view, offset, text) {
  for (let i = 0; i < text.length; i++) view.setUint8(offset + i, text.charCodeAt(i) & 0xff);
}

function floatToInt16(x) {
  const y = clamp(x, -1, 1);
  return y < 0 ? Math.round(y * 32768) : Math.round(y * 32767);
}

export function encodeWavPcm16(audioBuffer) {
  if (!audioBuffer) throw new Error("encodeWavPcm16: missing AudioBuffer");
  const numChannels = Math.max(1, Math.min(2, audioBuffer.numberOfChannels || 1));
  const sampleRate = audioBuffer.sampleRate >>> 0;
  const length = audioBuffer.length >>> 0;

  const bytesPerSample = 2;
  const blockAlign = numChannels * bytesPerSample;
  const byteRate = sampleRate * blockAlign;
  const dataSize = length * blockAlign;

  const buf = new ArrayBuffer(44 + dataSize);
  const view = new DataView(buf);

  writeAscii(view, 0, "RIFF");
  view.setUint32(4, 36 + dataSize, true);
  writeAscii(view, 8, "WAVE");

  writeAscii(view, 12, "fmt ");
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, numChannels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, byteRate, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, 16, true);

  writeAscii(view, 36, "data");
  view.setUint32(40, dataSize, true);

  const ch0 = audioBuffer.getChannelData(0);
  const ch1 = numChannels > 1 ? audioBuffer.getChannelData(1) : null;

  let off = 44;
  for (let i = 0; i < length; i++) {
    view.setInt16(off, floatToInt16(ch0[i] || 0), true);
    off += 2;
    if (numChannels > 1) {
      view.setInt16(off, floatToInt16(ch1[i] || 0), true);
      off += 2;
    }
  }

  return new Uint8Array(buf);
}

