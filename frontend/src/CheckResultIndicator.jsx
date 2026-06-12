import { useState, useEffect, useRef } from 'react'

const CHECK_ERROR_LABELS = {
  AllVoicesSameDirection:   'всі голоси в одну сторону',
  VoiceCrossing:            'перехрещення',
  ParallelFifths:           'паралельні квінти',
  ParallelOctaves:          'паралельні октави',
  ParallelOctavesOrUnisons: 'паралельні октави або унісони',
  HiddenFifths:             'приховані квінти між крайніми голосами',
  HiddenOctaves:            'приховані октави між крайніми голосами',
}

function getCheckErrorLabel(error) {
  return CHECK_ERROR_LABELS[error.code] || error.message || error.code || '?'
}

export default function CheckResultIndicator({
  checkErrors,
  highlightedCheckErrorIndex,
  onSetHighlightedIndex,
}) {
  const [popupOpen,  setPopupOpen]  = useState(false)
  const [isClosing,  setIsClosing]  = useState(false)
  const buttonRef    = useRef(null)
  const popupRef     = useRef(null)
  const closeTimerRef = useRef(null)
  const fadeTimerRef  = useRef(null)

  function startAutoClose() {
    clearTimeout(closeTimerRef.current)
    clearTimeout(fadeTimerRef.current)
    closeTimerRef.current = setTimeout(() => {
      setIsClosing(true)
      fadeTimerRef.current = setTimeout(() => {
        setPopupOpen(false)
        setIsClosing(false)
      }, 1000)
    }, 5000)
  }

  function closeWithFade() {
    clearTimeout(closeTimerRef.current)
    clearTimeout(fadeTimerRef.current)
    setIsClosing(true)
    fadeTimerRef.current = setTimeout(() => {
      setPopupOpen(false)
      setIsClosing(false)
    }, 1000)
  }

  useEffect(() => {
    if (checkErrors === null) return
    clearTimeout(closeTimerRef.current)
    clearTimeout(fadeTimerRef.current)
    setIsClosing(false)
    setPopupOpen(true)
    startAutoClose()
    return () => {
      clearTimeout(closeTimerRef.current)
      clearTimeout(fadeTimerRef.current)
    }
  }, [checkErrors])  // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    if (!popupOpen) return
    function handler(e) {
      if (
        !buttonRef.current?.contains(e.target) &&
        !popupRef.current?.contains(e.target)
      ) {
        closeWithFade()
      }
    }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [popupOpen])  // eslint-disable-line react-hooks/exhaustive-deps

  function handleButtonClick(e) {
    e.stopPropagation()
    if (popupOpen) {
      closeWithFade()
    } else {
      clearTimeout(closeTimerRef.current)
      clearTimeout(fadeTimerRef.current)
      setIsClosing(false)
      setPopupOpen(true)
    }
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
            isClosing  ? 'closing' : '',
          ].filter(Boolean).join(' ')}
          onMouseDown={e => e.stopPropagation()}
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
