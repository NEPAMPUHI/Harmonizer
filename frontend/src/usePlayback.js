import { useState, useEffect, useRef, useMemo } from 'react'
import { buildPlaybackTimeline, totalPlaybackTicks } from './playback'
import { AudioScheduler } from './audioPlayback'

// Default tempo in beats per minute.
const DEFAULT_BPM = 120

/**
 * Returns the playback BPM for the given time signature and speed mode.
 * x/8 signatures use a different tempo range than x/4 (compound vs simple metre).
 */
export function getPlaybackBpm(timeSignature, speedMode) {
  const denominator = parseInt(timeSignature?.split('/')[1] ?? '4', 10)
  if (denominator === 8) {
    return speedMode === 'slow' ? 40 : 70
  }
  return speedMode === 'slow' ? 70 : 120
}

// At DEFAULT_BPM: (120 beats/min × 4 sixteenths/beat) / 60 s/min = 8 ticks/s
function ticksPerSecond(bpm) {
  return (bpm * 4) / 60
}

/**
 * Manages playback state, a requestAnimationFrame-based tick counter, and audio scheduling.
 *
 * Playback states:
 *   'idle'    — stopped, currentTick = 0
 *   'playing' — tick counter advances in real time; audio is playing
 *   'paused'  — tick counter frozen; audio silenced
 *
 * Audio is driven by currentTick — the RAF loop is the single source of truth for timing.
 * AudioScheduler.scheduleTicks() is called every frame with the (prevTick, newTick] window;
 * notes whose startTick falls in that window are scheduled precisely onto the Web Audio clock.
 *
 * @param {{ measures, timeSignature, tonality, anacruisTicks, mode, audioClef?, bpm? }} params
 *   audioClef — when set, only events whose clef matches this value are sent to the audio
 *               scheduler (cursor is unaffected). Pass 'treble'/'bass' in harmonize mode,
 *               null/undefined in check mode (all voices play).
 * @returns {{ playbackState, currentTick, totalTicks, timeline, play, pause, stop }}
 */
export function usePlayback({ measures, timeSignature, tonality, anacruisTicks, mode, audioClef, bpm = DEFAULT_BPM }) {
  const [playbackState, setPlaybackState] = useState('idle')
  const [currentTick,   setCurrentTick]   = useState(0)

  // Refs so RAF callbacks always see fresh values without stale closures
  const rafRef            = useRef(null)
  const lastTimeRef       = useRef(null)    // null = "not yet started in this play session"
  const currentTickRef    = useRef(0)
  const audioRef          = useRef(new AudioScheduler())
  const warmupTimeoutRef  = useRef(null)

  // Build timeline and total length only when score content changes
  const timeline = useMemo(
    () => buildPlaybackTimeline(measures, timeSignature, tonality, anacruisTicks, mode),
    [measures, timeSignature, tonality, anacruisTicks, mode]
  )

  const totalTicks = useMemo(
    () => totalPlaybackTicks(measures, timeSignature, anacruisTicks),
    [measures, timeSignature, anacruisTicks]
  )

  // Keep timeline accessible inside the RAF closure without stale references.
  // When audioClef is set (harmonize mode), filter to that clef so only the
  // active staff's notes are sent to the audio scheduler. The cursor never reads
  // this ref, so filtering here has no effect on playback position or the cursor.
  const timelineRef = useRef([])
  useEffect(() => {
    timelineRef.current = audioClef
      ? timeline.filter(ev => ev.clef === audioClef)
      : timeline
  }, [timeline, audioClef])

  // ── RAF loop ─────────────────────────────────────────────────────
  useEffect(() => {
    if (playbackState !== 'playing') {
      cancelAnimationFrame(rafRef.current)
      lastTimeRef.current = null
      return
    }

    if (totalTicks === 0) {
      setPlaybackState('idle')
      return
    }

    // Cancel any leftover RAF and clear scheduled audio on every entry into 'playing'.
    // Covers initial play, resume, and BPM changes mid-playback.
    // Setting lastTimeRef to null ensures the first frame calls scheduleFromResume
    // so audio re-syncs to the current tick at the new tempo.
    cancelAnimationFrame(rafRef.current)
    audioRef.current.reset()
    lastTimeRef.current = null

    const tps   = ticksPerSecond(bpm)
    const audio = audioRef.current

    function frame(timestamp) {
      if (lastTimeRef.current === null) {
        // First frame of a new play/resume session — calibrate time reference and
        // schedule any notes that are already active at the current tick position.
        lastTimeRef.current = timestamp
        const audioNow = audio.ensureStarted()
        audio.scheduleFromResume(timelineRef.current, currentTickRef.current, tps, audioNow)
        rafRef.current = requestAnimationFrame(frame)
        return
      }

      // Read audio clock before advancing ticks so scheduled offsets are accurate.
      const audioNow = audio.audioTime
      const elapsed  = (timestamp - lastTimeRef.current) / 1000
      lastTimeRef.current = timestamp

      const prevTick = currentTickRef.current
      const newTick  = Math.min(prevTick + elapsed * tps, totalTicks)

      // Schedule notes whose startTick entered the playback window this frame
      audio.scheduleTicks(timelineRef.current, prevTick, newTick, tps, audioNow)

      currentTickRef.current = newTick
      setCurrentTick(newTick)

      if (newTick >= totalTicks) {
        currentTickRef.current = 0
        setCurrentTick(0)
        setPlaybackState('idle')
        audio.reset()
        return
      }

      rafRef.current = requestAnimationFrame(frame)
    }

    rafRef.current = requestAnimationFrame(frame)
    return () => cancelAnimationFrame(rafRef.current)
  }, [playbackState, totalTicks, bpm])

  // Cleanup warmup timeout and audio on unmount.
  useEffect(() => {
    return () => {
      if (warmupTimeoutRef.current) clearTimeout(warmupTimeoutRef.current)
      audioRef.current.reset()
    }
  }, [])

  // ── Handlers ─────────────────────────────────────────────────────

  function play() {
    if (playbackState === 'playing') return
    if (warmupTimeoutRef.current) return  // already warming up
    const isNew = !audioRef.current.isInitialized
    audioRef.current.reset()
    if (isNew) {
      // Pre-warm AudioContext here (inside user gesture handler) so Chrome
      // doesn't auto-suspend it. Then delay actual playback start by 1 second
      // to let the audio hardware fully initialize — otherwise the first ~1 s
      // of notes is silent.
      audioRef.current.ensureStarted()
      warmupTimeoutRef.current = setTimeout(() => {
        warmupTimeoutRef.current = null
        setPlaybackState('playing')
      }, 1000)
    } else {
      setPlaybackState('playing')
    }
  }

  function pause() {
    if (warmupTimeoutRef.current) {
      clearTimeout(warmupTimeoutRef.current)
      warmupTimeoutRef.current = null
      return
    }
    if (playbackState !== 'playing') return
    audioRef.current.stopAll()
    setPlaybackState('paused')
  }

  function stop() {
    if (warmupTimeoutRef.current) {
      clearTimeout(warmupTimeoutRef.current)
      warmupTimeoutRef.current = null
    }
    cancelAnimationFrame(rafRef.current)
    audioRef.current.reset()
    currentTickRef.current = 0
    setCurrentTick(0)
    setPlaybackState('idle')
  }

  return { playbackState, currentTick, totalTicks, timeline, play, pause, stop }
}
