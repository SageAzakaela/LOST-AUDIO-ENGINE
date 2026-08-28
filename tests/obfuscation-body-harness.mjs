import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";

const processorPath = new URL("../occlusion-engine/src/audio/obfuscation-body-processor.js", import.meta.url);
const source = readFileSync(processorPath, "utf8");
let Processor = null;

class FakeAudioWorkletProcessor {
  constructor() {
    this.port = { onmessage: null };
  }
}

vm.runInNewContext(source, {
  AudioWorkletProcessor: FakeAudioWorkletProcessor,
  registerProcessor(name, ctor) {
    assert.equal(name, "obfuscation-body");
    Processor = ctor;
  },
  sampleRate: 48000,
  Math,
});

assert.ok(Processor, "processor registered");

function render({ amount, looseness, brightness = 0.7, seed = 1234, seconds = 1 }) {
  const processor = new Processor({ processorOptions: { seed } });
  const frames = Math.floor(48000 * seconds);
  const rendered = new Float32Array(frames);
  const blockSize = 128;
  for (let offset = 0; offset < frames; offset += blockSize) {
    const length = Math.min(blockSize, frames - offset);
    const input = new Float32Array(length);
    for (let i = 0; i < length; i++) {
      const sample = offset + i;
      if (sample % 2400 === 0) input[i] = sample % 4800 === 0 ? 0.9 : -0.72;
      else input[i] = Math.sin((sample / 48000) * Math.PI * 2 * 110) * 0.08;
    }
    const output = new Float32Array(length);
    processor.process([[input]], [[output]], {
      amount: [amount], looseness: [looseness], brightness: [brightness], bodyHz: [620], cavityHz: [1180],
    });
    rendered.set(output, offset);
  }
  return rendered;
}

const silenceProcessor = new Processor({ processorOptions: { seed: 9 } });
const silence = new Float32Array(128);
const silenceOut = new Float32Array(128);
silenceProcessor.process([[silence]], [[silenceOut]], {
  amount: [1], looseness: [1], brightness: [1], bodyHz: [720], cavityHz: [1700],
});
assert.equal(Math.max(...silenceOut.map(Math.abs)), 0, "silence remains exact silence");

const disabled = render({ amount: 0, looseness: 1 });
assert.equal(Math.max(...disabled.map(Math.abs)), 0, "zero amount is exact bypass silence on the rattle branch");

const looseA = render({ amount: 0.9, looseness: 0.95, seed: 42 });
const looseB = render({ amount: 0.9, looseness: 0.95, seed: 42 });
let peak = 0;
let energy = 0;
let clampSamples = 0;
for (let i = 0; i < looseA.length; i++) {
  assert.equal(looseA[i], looseB[i], "same seed renders deterministically");
  assert.ok(Number.isFinite(looseA[i]), "output remains finite");
  peak = Math.max(peak, Math.abs(looseA[i]));
  if (Math.abs(looseA[i]) >= 0.299999) clampSamples++;
  energy += looseA[i] * looseA[i];
}
assert.ok(peak > 0.005, `loose construction responds audibly (peak ${peak})`);
assert.ok(peak <= 0.300001, `rattle branch respects safety clamp (peak ${peak})`);
assert.ok(Math.sqrt(energy / looseA.length) < 0.16, "rattle energy remains bounded");
assert.ok(clampSamples / looseA.length < 0.0025, `hard clamp remains exceptional (${clampSamples} samples)`);

console.log(JSON.stringify({ peak, rms: Math.sqrt(energy / looseA.length), clampSamples, silencePeak: 0 }));
