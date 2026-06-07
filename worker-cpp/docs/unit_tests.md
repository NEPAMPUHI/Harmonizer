# Unit Tests

Tests are written with [Catch2 v3](https://github.com/catchorg/Catch2) and fetched automatically by CMake.

## Build

```powershell
cmake -S . -B build
cmake --build build --target harmonizer_tests
```

## Run via CTest

```powershell
cd build
ctest --output-on-failure
```

## Run the test binary directly

```powershell
.\build\Debug\harmonizer_tests.exe
```

Pass a filter to run a specific tag or test name:

```powershell
.\build\Debug\harmonizer_tests.exe "[Note]"
.\build\Debug\harmonizer_tests.exe "[Chord]"
.\build\Debug\harmonizer_tests.exe "[HarmonyGraph]"
```

## Test files

| File | Covers |
|------|--------|
| `tests/NoteTests.cpp` | `Note::getSemitone`, comparison operators |
| `tests/ChordNameTests.cpp` | `Chord::getName` for all harmonic functions and chord types |
| `tests/HarmonyGraphTests.cpp` | `HarmonyGraph::buildNodes` level count, node IDs, `empty()` |
