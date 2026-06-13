import { useState, useEffect, useLayoutEffect, useRef } from 'react'

const CHECK_ERROR_LABELS = {
  UnknownChord:                       'невідомий акорд',
  VoiceRangeViolation:                'голос виходить за допустимий діапазон',
  MoreThanOctaveBetweenAdjacentVoices:'відстань між голосами більше октави',
  AllVoicesSameDirection:             'всі голоси в одну сторону',
  VoiceCrossing:                      'перехрещення',
  ChromaticSemitoneTransfer:          'хроматична передача напівтону',
  ParallelFifths:                     'паралельні квінти',
  ParallelOctaves:                    'паралельні октави',
  ParallelOctavesOrUnisons:           'паралельні октави або унісони',
  HiddenFifths:                       'приховані квінти між крайніми голосами',
  HiddenOctaves:                      'приховані октави між крайніми голосами',
  AugmentedIntervalInBass:            'збільшений інтервал в басу',
  VoiceLeapGreaterThanOctave:         'стрибок голосу більше допустимого',
  ConsecutiveFourthsInBass:           '2 послідовні ходи по квартам в басу',
  ConsecutiveFifthsInBass:            '2 послідовні ходи по квінтам в басу',
  FunctionalProgressionError:         'порушення функціональної послідовності',
}

function getCheckErrorLabel(error) {
  return CHECK_ERROR_LABELS[error.code] || error.message || error.code || '?'
}

const AUTO_CLOSE_DELAY = 3000   // ms to wait before starting fade
const FADE_DURATION    = 1000   // ms of fade animation

export default function CheckResultIndicator({
  checkErrors,
  highlightedCheckErrorIndex,
  onSetHighlightedIndex,
}) {
  const [popupOpen,    setPopupOpen]    = useState(false)
  const [isAutoFading, setIsAutoFading] = useState(false)

  const buttonRef      = useRef(null)
  const popupRef       = useRef(null)
  const waitTimerRef   = useRef(null)   // fires after AUTO_CLOSE_DELAY → starts fade
  const fadeTimerRef   = useRef(null)   // fires after FADE_DURATION → unmounts popup
  const isAutoOpenRef  = useRef(false)  // true only when popup was opened by check result

  // Match popup width to the .btn-check button whenever popup opens.
  useLayoutEffect(() => {
    if (!popupOpen || !popupRef.current) return
    const btn = document.querySelector('.btn-check')
    if (!btn) return
    const { width } = btn.getBoundingClientRect()
    if (width > 0) {
      const w = `${Math.round(width)}px`
      popupRef.current.style.width    = w
      popupRef.current.style.minWidth = w
      popupRef.current.style.maxWidth = w
    }
  }, [popupOpen])

  function clearTimers() {
    clearTimeout(waitTimerRef.current)
    clearTimeout(fadeTimerRef.current)
  }

  // Start the countdown: wait → fade → close.
  // Resets any in-progress timers first; also cancels an ongoing fade.
  function scheduleAutoClose() {
    clearTimers()
    setIsAutoFading(false)
    waitTimerRef.current = setTimeout(() => {
      setIsAutoFading(true)
      fadeTimerRef.current = setTimeout(() => {
        setPopupOpen(false)
        setIsAutoFading(false)
      }, FADE_DURATION)
    }, AUTO_CLOSE_DELAY)
  }

  // Close immediately with no animation (manual action).
  function closeNow() {
    clearTimers()
    setIsAutoFading(false)
    setPopupOpen(false)
  }

  // New check result → open and start auto-close countdown.
  useEffect(() => {
    if (checkErrors === null) return
    clearTimers()
    setIsAutoFading(false)
    isAutoOpenRef.current = true
    setPopupOpen(true)
    scheduleAutoClose()
    return clearTimers
  }, [checkErrors])  // eslint-disable-line react-hooks/exhaustive-deps

  // Click outside → close immediately, no animation.
  useEffect(() => {
    if (!popupOpen) return
    function handler(e) {
      if (
        !buttonRef.current?.contains(e.target) &&
        !popupRef.current?.contains(e.target)
      ) {
        closeNow()
      }
    }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [popupOpen])  // eslint-disable-line react-hooks/exhaustive-deps

  // Toggle button → manual open/close, no animation, no auto-close.
  function handleButtonClick(e) {
    e.stopPropagation()
    if (popupOpen) {
      closeNow()
    } else {
      clearTimers()
      setIsAutoFading(false)
      isAutoOpenRef.current = false
      setPopupOpen(true)
    }
  }

  // Hovering over popup → cancel any pending auto-close / in-progress fade.
  function handleMouseEnter() {
    clearTimers()
    setIsAutoFading(false)
  }

  // Leaving popup → restart the countdown only for auto-opened popups.
  function handleMouseLeave() {
    if (isAutoOpenRef.current) scheduleAutoClose()
  }

  if (checkErrors === null) return null

  const hasErrors = checkErrors.length > 0

  return (
    <div className="check-result-indicator">
      <button
        ref={buttonRef}
        className={`check-result-button${hasErrors ? ' fail' : ' ok'}`}
        onClick={handleButtonClick}
        title={hasErrors ? `Знайдено помилок: ${checkErrors.length}` : 'Помилок не виявлено'}
      >
        {hasErrors ? '✕' : '✓'}
      </button>

      {popupOpen && (
        <div
          ref={popupRef}
          className={[
            'check-result-popup',
            hasErrors ? 'fail' : 'ok',
            isAutoFading ? 'auto-fading' : '',
          ].filter(Boolean).join(' ')}
          onMouseDown={e => e.stopPropagation()}
          onMouseEnter={handleMouseEnter}
          onMouseLeave={handleMouseLeave}
        >
          <div className="check-result-popup-header">
            Знайдено помилок: {checkErrors.length}
          </div>
          {hasErrors && (
            <ul className="check-result-popup-error-list">
              {checkErrors.map((error) => (
                <li
                  key={error.__index}
                  className={[
                    'check-result-popup-error-item',
                    error.__index === highlightedCheckErrorIndex ? 'active' : '',
                  ].filter(Boolean).join(' ')}
                  onClick={() => onSetHighlightedIndex?.(
                    error.__index === highlightedCheckErrorIndex ? null : error.__index
                  )}
                >
                  {getCheckErrorLabel(error)}
                </li>
              ))}
            </ul>
          )}
        </div>
      )}
    </div>
  )
}
