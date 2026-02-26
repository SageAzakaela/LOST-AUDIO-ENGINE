export function encodeWavMono16(audioBuffer) {
  const sr = audioBuffer.sampleRate;
  const ch = audioBuffer.getChannelData(0);
  const n = ch.length;
  const dataSize = n * 2;
  const buf = new ArrayBuffer(44 + dataSize);
  const dv = new DataView(buf);
  let o = 0;
  const wStr = (s) => {
    for (let i = 0; i < s.length; i++) dv.setUint8(o++, s.charCodeAt(i));
  };
  const wU32 = (x) => {
    dv.setUint32(o, x, true);
    o += 4;
  };
  const wU16 = (x) => {
    dv.setUint16(o, x, true);
    o += 2;
  };

  wStr("RIFF");
  wU32(36 + dataSize);
  wStr("WAVE");

  wStr("fmt ");
  wU32(16);
  wU16(1); // PCM
  wU16(1); // mono
  wU32(sr);
  wU32(sr * 2);
  wU16(2);
  wU16(16);

  wStr("data");
  wU32(dataSize);

  const toI16 = (x) => {
    const v = Math.max(-1, Math.min(1, x));
    return v < 0 ? Math.round(v * 32768) : Math.round(v * 32767);
  };
  for (let i = 0; i < n; i++) {
    dv.setInt16(o, toI16(ch[i]), true);
    o += 2;
  }
  return new Uint8Array(buf);
}

