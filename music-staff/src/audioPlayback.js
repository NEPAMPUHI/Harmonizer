// MIDI note number → frequency (Hz). A4 = midi 69 = 440 Hz.
function midiToFreq(midi) {
  return 440 * Math.pow(2, (midi - 69) / 12)
}

// ── Piano synthesis constants ─────────────────────────────────────────────────

// Harmonic partials: [frequency multiplier, relative gain].
// Models the overtone series of a struck string.
const HARMONICS = [
  [1, 1.00],   // fundamental
  [2, 0.40],   // octave
  [3, 0.18],   // perfect 12th
  [4, 0.10],   // 2nd octave
  [5, 0.05],   // major 17th
]

const PEAK_GAIN = 0.10   // per-note peak; 4 voices × harmonic sum ≤ 0.69 before compressor
const SUSTAIN_R = 0.28   // sustain level as fraction of peak
const ATTACK_S  = 0.008  // 8 ms linear attack
const DECAY_S   = 0.28   // target decay duration (clamped to fit note length)
const RELEASE_S = 0.060  // 60 ms release at note end

// ── Node factory ──────────────────────────────────────────────────────────────

/**
 * Creates and schedules Web Audio nodes for one piano-like note.
 * Does not set osc.onended — caller owns lifecycle.
 *
 * Signal chain: osc[N] → harmonicGain[N] → lpFilter → masterGain → destination
 *
 * masterGain always stays at 1.0 during normal play; stopAll() uses it for a
 * clean 25 ms fade-out without needing to know the per-harmonic envelope phase.
 *
 * @returns {{ oscs: {osc, gain}[], filter, masterGain }}
 */
function buildPianoNote(ctx, destination, midiNote, startTime, durationSec) {
  const freq = midiToFreq(midiNote)
  const end  = startTime + durationSec

  // Master gain — 1 throughout; only touched by stopAll()
  const masterGain = ctx.createGain()
  masterGain.gain.setValueAtTime(1, startTime)
  masterGain.connect(destination)

  // Low-pass filter: bright on attack (8 kHz), warms to 4 kHz over 150 ms.
  // Removes the "8-bit" harshness of raw oscillators while keeping clarity.
  const filter = ctx.createBiquadFilter()
  filter.type = 'lowpass'
  filter.Q.value = 0.8
  filter.frequency.setValueAtTime(8000, startTime)
  filter.frequency.exponentialRampToValueAtTime(4000, startTime + 0.15)
  filter.connect(masterGain)

  // Envelope time-points (clamped so nothing exceeds note duration)
  const attackEnd    = startTime + ATTACK_S
  const releaseStart = Math.max(end - RELEASE_S, attackEnd)
  const decayAvail   = Math.max(0, releaseStart - attackEnd)
  const decayDur     = Math.min(DECAY_S, decayAvail * 0.6)
  const decayEnd     = attackEnd + decayDur

  const oscs = []

  for (const [mult, relGain] of HARMONICS) {
    const osc  = ctx.createOscillator()
    const gain = ctx.createGain()
    osc.type = 'sine'
    osc.frequency.value = freq * mult

    const peakG = PEAK_GAIN * relGain
    const sustG = Math.max(peakG * SUSTAIN_R, 0.00001)

    // Attack
    gain.gain.setValueAtTime(0, startTime)
    gain.gain.linearRampToValueAtTime(peakG, attackEnd)
    // Decay → sustain
    if (decayDur > 0.005) {
      gain.gain.exponentialRampToValueAtTime(sustG, decayEnd)
    }
    // Continued slow piano-like decay through the sustain phase
    if (releaseStart > decayEnd + 0.01) {
      gain.gain.exponentialRampToValueAtTime(Math.max(sustG * 0.55, 0.00001), releaseStart)
    }
    // Release
    gain.gain.exponentialRampToValueAtTime(0.00001, end)

    osc.connect(gain)
    gain.connect(filter)
    osc.start(startTime)
    osc.stop(end + 0.05)   // small tail so release envelope completes

    oscs.push({ osc, gain })
  }

  return { oscs, filter, masterGain }
}

// ── Scheduler ─────────────────────────────────────────────────────────────────

/**
 * Manages a single Web Audio context and all note nodes for one playback session.
 *
 * Lifecycle (driven by usePlayback RAF loop):
 *   play  → reset() + ensureStarted() + scheduleFromResume() in calibration frame
 *   frame → scheduleTicks() once per RAF frame
 *   pause → stopAll()
 *   stop  → reset()
 */
export class AudioScheduler {
  constructor() {
    this._ctx        = null
    this._compressor = null
    this._nodes      = new Map()   // eventKey → { oscs, filter, masterGain }
    this._scheduled  = new Set()   // keys already triggered this play session
  }

  // ── Context ────────────────────────────────────────────────────────

  ensureStarted() {
    if (!this._ctx || this._ctx.state === 'closed') {
      this._ctx        = new (window.AudioContext || window.webkitAudioContext)()
      this._compressor = this._ctx.createDynamicsCompressor()
      this._compressor.connect(this._ctx.destination)
    }
    if (this._ctx.state === 'suspended') this._ctx.resume()
    return this._ctx.currentTime
  }

  get audioTime() {
    return this._ctx ? this._ctx.currentTime : 0
  }

  // ── Internal ───────────────────────────────────────────────────────

  _key(ev) {
    return `${ev.voice}@${ev.startTick}`
  }

  _scheduleNote(key, midiNote, startTime, durationSec) {
    const nodes = buildPianoNote(this._ctx, this._compressor, midiNote, startTime, durationSec)

    // Idempotent cleanup — first osc.onended to fire does the full teardown.
    // If stopAll() cleared _nodes first, has() returns false and this is a no-op.
    const cleanup = () => {
      if (!this._nodes.has(key)) return
      const n = this._nodes.get(key)
      for (const { osc, gain } of n.oscs) {
        try { osc.disconnect(); gain.disconnect() } catch {}
      }
      try { n.filter.disconnect(); n.masterGain.disconnect() } catch {}
      this._nodes.delete(key)
    }
    for (const { osc } of nodes.oscs) osc.onended = cleanup

    this._nodes.set(key, nodes)
  }

  // ── Public API ─────────────────────────────────────────────────────

  /**
   * Called in the first RAF calibration frame (play / resume).
   * Schedules the remaining tail of any note already active at resumeTick.
   */
  scheduleFromResume(timeline, resumeTick, tps, audioNow) {
    for (const ev of timeline) {
      if (ev.midiNote === null) continue
      if (ev.startTick > resumeTick) continue
      const endTick = ev.startTick + ev.durationTicks
      if (endTick <= resumeTick) continue
      const key = this._key(ev)
      if (this._scheduled.has(key)) continue
      const remainingSec = (endTick - resumeTick) / tps
      if (remainingSec < 0.015) continue
      this._scheduleNote(key, ev.midiNote, audioNow, remainingSec)
      this._scheduled.add(key)
    }
  }

  /**
   * Called every RAF frame.
   * Schedules notes whose startTick falls in the half-open window (prevTick, newTick].
   * audioNow must be read from audioCtx.currentTime at the start of the same frame.
   */
  scheduleTicks(timeline, prevTick, newTick, tps, audioNow) {
    if (!this._ctx) return
    for (const ev of timeline) {
      if (ev.midiNote === null) continue
      if (ev.startTick <= prevTick || ev.startTick > newTick) continue
      const key = this._key(ev)
      if (this._scheduled.has(key)) continue
      const offsetSec   = (ev.startTick - prevTick) / tps
      const durationSec = ev.durationTicks / tps
      this._scheduleNote(key, ev.midiNote, audioNow + offsetSec, durationSec)
      this._scheduled.add(key)
    }
  }

  /**
   * Fades out all active notes over 25 ms (Pause or Stop).
   * Uses masterGain (always at 1) for a clean, click-free stop.
   * Disconnects nodes 50 ms later via setTimeout to avoid graph leaks.
   */
  stopAll() {
    if (!this._ctx) { this._nodes.clear(); this._scheduled.clear(); return }
    const now = this._ctx.currentTime
    for (const { oscs, filter, masterGain } of this._nodes.values()) {
      try {
        masterGain.gain.cancelScheduledValues(0)
        masterGain.gain.setValueAtTime(1, now)
        masterGain.gain.linearRampToValueAtTime(0, now + 0.025)
        for (const { osc } of oscs) {
          try { osc.stop(now + 0.03) } catch {}
        }
        // Disconnect from the graph after oscillators stop to prevent leaks
        setTimeout(() => {
          for (const { osc, gain } of oscs) {
            try { osc.disconnect(); gain.disconnect() } catch {}
          }
          try { filter.disconnect(); masterGain.disconnect() } catch {}
        }, 50)
      } catch {}
    }
    this._nodes.clear()
    this._scheduled.clear()
  }

  reset() {
    this.stopAll()
  }
}
