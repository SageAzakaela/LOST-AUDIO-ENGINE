# Lost Audio Engine expansion priorities

Status: approved direction and future scope
Decision date: 2026-08-27

## Priority decision

The next expansion focus is **space emulation** because the current Space
category is sparse compared with the recorded-device and transmission
categories.

The first-priority space engines are:

1. **Ductwork**
2. **Water Boundary**

They should be distinct engines with their own physical models and control
surfaces. They may share reusable space-processing primitives, but neither
should be reduced to a themed reverb or a preset inside Obfuscation.

The preferred future device engines are:

1. **Dictaphone**
2. **Surveillance**
3. **Lossy Media**
4. **Turntable**

These device engines remain important, but follow the current space-emulation
push unless testing or product needs justify changing the order.

## First-priority space engines

### Ductwork

**Identity:** Audio traveling through vents, pipes, HVAC trunks, service
shafts, and resonant metal or plastic channels.

**Signal model:**

- source coupling into an opening rather than a generic wet/dry reverb send;
- length, diameter, cross-section, bends, branches, and constrictions;
- material resonances for galvanized steel, aluminum, plastic, and lined duct;
- frequency-dependent distance loss and modal buildup;
- vent-grille filtering and rattle at the source and listener ends;
- fan motor coupling, airflow turbulence, pressure flutter, and mechanical
  vibration;
- optional stereo emergence from multiple outlets.

**Surface controls:**

- Path / Geometry
- Distance
- Material
- Airflow
- Vent Rattle
- Mix

**Advanced areas:**

- source and listener aperture;
- duct dimensions;
- bend and branch count;
- resonance density and damping;
- constriction and leakage;
- fan speed, motor coupling, and turbulence;
- grille stiffness and enclosure vibration;
- stereo outlet spacing.

**Preset territories:**

- Voice Through Apartment Vent
- Industrial HVAC Trunk
- Bathroom Exhaust
- Metal Service Shaft
- Plastic Dryer Hose
- Fan Behind the Grille
- Impossible Endless Duct

### Water Boundary

**Identity:** Sound within water, recorded by a hydrophone, or transmitted
across the air-water boundary.

**Required propagation modes:**

- submerged source to submerged listener;
- air source to underwater listener;
- underwater source to air listener;
- resonant tank or vessel;
- direct hydrophone recording.

**Signal model:**

- strong boundary transmission/reflection behavior;
- depth- and distance-dependent absorption;
- water-body size and enclosure resonances;
- surface motion, wave modulation, and changing boundary coupling;
- bubbles, cavitation, particulate movement, and current as optional events;
- hydrophone/transducer resonance, self-noise, cable handling, and mounting;
- pressure-oriented low-frequency behavior without relying on a simple
  low-pass-filter stereotype.

**Surface controls:**

- Boundary Mode
- Depth / Distance
- Water Body
- Surface Motion
- Hydrophone
- Debris / Bubbles
- Mix

**Advanced areas:**

- source and listener depth;
- salinity or absorption character;
- surface roughness;
- tank dimensions and wall material;
- reflection strength and modal damping;
- hydrophone type, mounting, cable noise, and preamp;
- bubble density, size, and event rate;
- current and Doppler-like motion.

**Preset territories:**

- Poolside Voice From Below
- Hydrophone in a Lake
- Metal Water Tank
- Submerged Machinery
- Voice Above the Surface
- Flooded Concrete Room
- Deep Impossible Pressure

## Preferred future device engines

### Dictaphone

Portable and office voice recorders, especially microcassette and compact
dictation systems. This engine should emphasize speech-speed transport,
tiny electret microphones, automatic level behavior, narrow record/playback
heads, plastic enclosure resonance, motor coupling, and start/stop mechanics.
It must feel distinct from the broader Tape engine rather than merely using a
smaller tape preset.

### Surveillance

CCTV, inexpensive IP cameras, doorbell cameras, baby monitors, body cameras,
and concealed security microphones. The characteristic path includes poor mic
placement, enclosure resonance, aggressive AGC, speech gating, denoising,
variable codec quality, wind or handling exposure, and remote-stream failure.
It should remain distinct from Camcorder and Conference by centering
unattended capture and monitoring systems.

### Lossy Media

MP3, early web audio, RealAudio-like streaming, low-bitrate archives, and
files damaged by repeated transcoding. Important behaviors include
psychoacoustic pre-echo, watery spectral smearing, birdies, stereo-image
collapse, bitrate pumping, block transitions, bandwidth switching, generation
loss, buffering, and damaged-frame concealment. It must not behave like a
bitcrusher or duplicate Conference packet loss.

### Turntable

Modern vinyl, worn records, shellac playback, phonographs, and wax-cylinder
territory. The model should include groove tracing and wear, eccentricity,
wow, rumble, stylus profile, surface contamination, inner-groove loss,
preamp/equalization character, acoustic horn or cabinet resonance, and
physical handling. Stylized presets may exaggerate looping scratches and
groove failure while protected defaults remain believable.

## Stretch goals

### Space stretch goals

- **Vehicle Cabin:** car, trunk, bus, train, and subway interiors.
- **Institutional Architecture:** stairwells, school halls, gymnasiums,
  parking garages, hospitals, and bunkers.
- **Containers:** boxes, lockers, refrigerators, closets, pockets, bags, and
  metal bins.
- **Open Air:** alleys, courtyards, forests, snowfields, fog, wind, and sparse
  distant reflections.

### Device stretch goals

- **Optical Soundtrack:** film-projector transport and optical audio damage.
- **Early Sampler:** vintage converters, interpolation, memory, and
  pitch-dependent artifacts.
- **Toy Electronics:** talking dolls, greeting cards, toy keyboards, dying
  batteries, and plastic speakers.
- **Wire Recorder:** magnetic wire transport, narrow recording response, and
  office-machine mechanics.

Stretch goals are preserved for later exploration and are not current
implementation commitments.

## Shared quality bar

Every expansion engine must:

- model a recognizable physical or signal path instead of applying a themed
  EQ-and-reverb combination;
- cover believable subtle behavior through intentionally exaggerated states;
- expose immediate physical/audible Surface controls and deeper Advanced
  controls;
- provide safe, useful presets with deterministic random behavior where
  applicable;
- explain which behaviors are evidence-based, reference-informed, plausible,
  or stylized;
- pass parameter-influence, silence, non-finite, clipping, automation, and
  listening QA before being treated as complete.
