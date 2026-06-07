# Worker Manual Test

## Build

```powershell
cmake -S . -B build
cmake --build build
```

The executable is at `build/Debug/harmonizer_worker.exe` (MSVC) or `build/harmonizer_worker.exe` (MinGW).

---

## Test 1 — Valid job (expect `"status":"success"` + file on disk)

Input JSON (one-liner for piping):

```json
{"jobId":"job_000001","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4}]}}
```

PowerShell command:

```powershell
echo '{"jobId":"job_000001","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected stdout:

```json
{"jobId":"job_000001","status":"success","results":[{"variantId":"variant_001","filePath":"results/job_000001/variant_001.musicxml","score":100}],"errors":[]}
```

Expected file created at `results/job_000001/variant_001.musicxml`.
**Note: this is not yet harmonized output — it contains the input melody as-is.**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC
  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"
  "http://www.musicxml.org/dtds/partwise.dtd">
<!-- jobId: job_000001 | variantId: variant_001 | input melody (not yet harmonized) -->
<score-partwise version="4.0">
  <part-list>
    <score-part id="P1"><part-name>Melody</part-name></score-part>
  </part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>4</divisions>
        <key><fifths>0</fifths></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>4</duration>
        <type>quarter</type>
      </note>
    </measure>
  </part>
</score-partwise>
```

Verify the file was created and contains `<score-partwise`:

```powershell
Get-Content results\job_000001\variant_001.musicxml
```

---

## Test 2 — Missing `notes` field (expect `"status":"error"`)

```powershell
echo '{"jobId":"job_000002","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{}}' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"job_000002","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Missing field: input.notes"}]}
```

---

## Test 3 — Empty `notes` array (expect `"status":"error"`)

```powershell
echo '{"jobId":"job_000003","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[]}}' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"job_000003","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Input notes are empty"}]}
```

---

## Test 4 — Invalid JSON (expect `"status":"error"`)

```powershell
echo 'not-a-json' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Invalid JSON"}]}
```

---

## Test 5 — Five quarter notes in 4/4 (expect two measures in MusicXML)

5 quarter notes exceed one 4/4 measure (capacity = 16 sixteenths = 4 quarters).
The first 4 notes fill measure 1; the 5th note spills into measure 2.

```powershell
echo '{"jobId":"job_000005","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":2,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4},{"step":"D","octave":4,"alter":0,"durationSixteenths":4},{"step":"E","octave":4,"alter":0,"durationSixteenths":4},{"step":"F","octave":4,"alter":0,"durationSixteenths":4},{"step":"G","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected stdout: `"status":"success"` with one result file.

Verify `results/job_000005/variant_001.musicxml` contains two measures:

```powershell
Select-String -Path results\job_000005\variant_001.musicxml -Pattern 'measure number'
```

Expected: two matches — `measure number="1"` and `measure number="2"`.

---

## Test 6 — Anacrusis (anacrusisSixteenths = 4, three quarter notes)

`anacrusisSixteenths = 4` means the first measure holds exactly one quarter note.
The remaining two quarter notes go into measure 1 (the first full measure).
Expected MusicXML: `measure number="0"` (anacrusis) + `measure number="1"` (full measure).

```powershell
echo '{"jobId":"job_000006","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":4,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"G","octave":4,"alter":0,"durationSixteenths":4},{"step":"C","octave":5,"alter":0,"durationSixteenths":4},{"step":"E","octave":5,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected stdout: `"status":"success"` with one result file.

Verify `results/job_000006/variant_001.musicxml` has both measure numbers:

```powershell
Select-String -Path results\job_000006\variant_001.musicxml -Pattern 'measure number'
```

Expected matches:
```
measure number="0"
measure number="1"
```

---

## Test 7 — Key G major (expect `<fifths>1</fifths>`)

```powershell
echo '{"jobId":"job_000007","mode":"harmonize_melody","settings":{"key":"G","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"G","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Verify:

```powershell
Select-String -Path results\job_000007\variant_001.musicxml -Pattern '<fifths>'
```

Expected: `<fifths>1</fifths>`

---

## Test 8 — Key D minor (expect `<fifths>-1</fifths>`)

```powershell
echo '{"jobId":"job_000008","mode":"harmonize_melody","settings":{"key":"D","scaleMode":"minor","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"D","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Verify:

```powershell
Select-String -Path results\job_000008\variant_001.musicxml -Pattern '<fifths>'
```

Expected: `<fifths>-1</fifths>`

---

## Test 9 — Unsupported key "H" (expect `"Unsupported key"`)

```powershell
echo '{"jobId":"job_000009","mode":"harmonize_melody","settings":{"key":"H","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"B","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"job_000009","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Unsupported key"}]}
```

---

## Test 10 — Unsupported scale mode "dorian" (expect `"Unsupported scale mode"`)

```powershell
echo '{"jobId":"job_000010","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"dorian","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"job_000010","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Unsupported scale mode"}]}
```

---

## Test 11 — Unsupported chord name (expect `"Unsupported chord name"`)

```powershell
echo '{"jobId":"job_000011","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","Q99"]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"job_000011","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Unsupported chord name"}]}
```

---

## Test 12 — Empty chord name (expect `"Chord name is empty"`)

```powershell
echo '{"jobId":"job_000012","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":[""]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4}]}}' | .\build\harmonizer_worker.exe
```

Expected output:

```json
{"jobId":"job_000012","status":"error","results":[],"errors":[{"code":"WORKER_ERROR","message":"Chord name is empty"}]}
```

---

## Graceful shutdown

Send `shutdown` to stop the worker loop:

```powershell
echo 'shutdown' | .\build\harmonizer_worker.exe
```

No output — process exits with code 0.

---

## Multi-job session (PowerShell here-string)

```powershell
@'
{"jobId":"job_000001","mode":"harmonize_melody","settings":{"key":"C","scaleMode":"major","measureCount":1,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","S53","D53"]},"input":{"notes":[{"step":"C","octave":4,"alter":0,"durationSixteenths":4}]}}
{"jobId":"job_000002","mode":"harmonize_bass","settings":{"key":"G","scaleMode":"major","measureCount":2,"timeSignature":{"beats":4,"beatType":4},"anacrusisSixteenths":0,"forbiddenRules":[],"allowedChords":["T53","D53"]},"input":{"notes":[{"step":"G","octave":2,"alter":0,"durationSixteenths":8},{"step":"D","octave":3,"alter":0,"durationSixteenths":8}]}}
shutdown
'@ | .\build\harmonizer_worker.exe
```

Expected output (two lines):
```json
{"jobId":"job_000001","status":"success","results":[{"variantId":"variant_001","filePath":"results/job_000001/variant_001.musicxml","score":100}],"errors":[]}
{"jobId":"job_000002","status":"success","results":[{"variantId":"variant_001","filePath":"results/job_000002/bass_variant_001.musicxml","score":100}],"errors":[]}
```
Then clean exit.
