# HarmonyGraph::getMaxDistinctChordNameCount() — debug test

## Build order in HarmonyGraph::build(...)

```
buildNodes(chordsByPosition)
buildEdges()
pruneDeadEnds()
rebuildUniqueChordNamesByLevel()   ← runs AFTER pruning, so dead-end nodes are excluded
```

`getMaxDistinctChordNameCount()` reads from `uniqueChordNamesByLevel`, which is populated by
`rebuildUniqueChordNamesByLevel()` only after `pruneDeadEnds()` has removed dead-end nodes.
This guarantees that only reachable nodes contribute to the chord-name count.

---

## Manual scenario

### Input chordsByPosition

| Level | Chords present |
|-------|----------------|
| 0     | T53, T53, D7   |
| 1     | S53, II6, II6, D7 |

### After rebuildUniqueChordNamesByLevel()

| Level | Unique chord names       | Set size |
|-------|--------------------------|----------|
| 0     | { "T53", "D7" }          | 2        |
| 1     | { "S53", "II6", "D7" }   | 3        |

### Expected result

```
getMaxDistinctChordNameCount() == 3
```

The method iterates `uniqueChordNamesByLevel` and returns the maximum `size()` across all levels.
Duplicates within a level (e.g. two II6 nodes) are collapsed by the `unordered_set`, so each
distinct name is counted once per level.
