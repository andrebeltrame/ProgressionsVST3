# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Harmonia: a VST3/standalone plugin that reads a MIDI clip (or a progression you
type), works out the harmony, and writes new parts over it — pads, melodies,
counter melodies, basslines, arps, plucks.

Development happens on the branch `claude/vst3-music-idea-generator-vqgacc`.

## Commands

The engine builds in seconds with no third-party dependencies. Work here first
whenever the change is not specifically about the plugin UI or its audio thread.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/tests/harmonia_tests              # all 80 tests, ~60ms
./build/tests/harmonia_tests Progression  # only tests whose name contains this
```

Manual listening/inspection without a DAW:

```bash
./build/cli/harmonia-cli resources/examples/bass_loop.mid --info
./build/cli/harmonia-cli --preset deep-warm --key "F minor" --part pad,melody --out /tmp/out
./build/cli/harmonia-cli scan <folder> --index /tmp/lib.json && ./build/cli/harmonia-cli library --index /tmp/lib.json
```

The plugin needs JUCE plus the usual Linux dev packages (listed in the README):

```bash
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build-plugin -G Ninja -DCMAKE_BUILD_TYPE=Release -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build-plugin        # ~5 min cold, LTO link dominates
ctest --test-dir build-plugin     # core tests + the plugin smoke test
```

`plugin/tests/PluginSmokeTest.cpp` is the way to verify plugin changes without a
DAW or a display: it drives the processor through 400 audio blocks, checks every
note-on is matched by a note-off, round-trips the state, and **renders the editor
to a PNG** — pass an output path and look at the image after any UI change.

```bash
./build-plugin/plugin/harmonia_plugin_tests_artefacts/Release/harmonia_plugin_tests /tmp/ui.png
```

## Architecture

### `core/` is JUCE-free, on purpose

Nothing under `core/` may include JUCE. That is what keeps analysis and
generation testable in milliseconds and reusable behind another wrapper. The
plugin depends on the core; never the other way round.

### `Analysis` is the only currency

Every path converges on one `Analysis` struct (key, progression, rhythm profile,
role, tempo, meter, length), and the generators read nothing else. There are two
ways to produce one:

- `analyze(sequence)` — from a MIDI clip (`core/src/Analysis.cpp`)
- `analysisFromProgression(...)` / `applyProgressionTo(...)` — from chords the
  user typed or a preset (`core/src/Progression.cpp`)

`Engine` (`core/src/Engine.cpp`) hides which one happened. It keeps the detected
progression and any written one side by side so `resetProgression()` can go back.
When a clip is loaded, a written progression replaces only the chords — tempo,
meter, length, groove and detected role stay.

### The style model is statistics, never phrases

`core/src/StyleModel.cpp` learns from a whole MIDI collection during the same
pass that builds the library index. It stores counts only — bar onset masks per
role, scale-step transitions, intervals over the chord root by metric strength,
voicing spacings, progression frequencies. It must stay that way: no stored
phrases, so the model file carries no one's material and generation cannot
plagiarise. Learned voicings are spacing templates that get snapped onto the
chord actually playing, which is what keeps a major shape from dragging a major
third onto a minor chord.

Every generator has a fallback path for when the corpus is empty or
`styleAmount` is low. Keep it that way — the engine has to work with no library
at all.

### Generation is deterministic

Same seed plus same `GenerateOptions` must always produce byte-identical MIDI.
The plugin stores the seed in its state rather than the notes, and the CLI's
`--seed` is the whole reproducibility story. Any randomness added to a generator
has to go through `Context::rng`; never `rand()`, a clock, or an address.

### The library walk has to survive a real drive

`scanDirectory` walks with an explicit stack of folders, not
`recursive_directory_iterator` - that iterator becomes `end()` the moment any
increment fails, so a single folder macOS refuses (`.Spotlight-V100` returns
EPERM, which `skip_permission_denied` does not cover) silently ended the scan of
an entire drive and looked like a small collection. Keep the per-folder
isolation, and keep reporting `walkErrors`: a scan that skipped something must
say so.

`saveIndex` streams JSON straight to the file for the same reason - a few
hundred thousand entries will not fit as a document tree plus a serialised
string.

`LibraryEntry::effectiveRole()` is the single answer to "what is this clip":
the folder's verdict when it has one, the detected role otherwise. Both
`queryLibrary` and the style learner go through it, so a query for bass cannot
end up training the melody bank - they disagreed once and it was invisible in
the output.

Files are parsed across threads, split into one fixed block per thread rather
than pulled from a shared queue, so the index and the style model come out
byte-identical whatever `ScanOptions::threads` is. There is a test for that; do
not replace the blocks with work stealing without replacing that guarantee.

### Analysis constants are tuned, not derived

`core/src/Analysis.cpp` holds numbers that were set empirically against real
clips: `kFirstBassBonus`/`kLastBassBonus` (relative major vs minor), the frame
`evidence` weighting (a single pitch class must not outvote a held chord), the
metric weighting on the bass bonus, `changePenaltyScale` per source role, and
`AnalysisOptions::diatonicBias`. Changing any of them shifts detection
everywhere. Verify against all three clips in `resources/examples/` — a bass, a
pad and a lead — plus the tests, before and after.

`docs/COMO_FUNCIONA.md` explains why each of these exists.

### Plugin threading

Generation happens on the message thread and is published as a
`RenderedPart` (`plugin/Source/RenderedPart.h`) — a reference-counted list of
events in quarter-note positions — under a `SpinLock`. `processBlock` uses
`ScopedTryLockType` and skips a block rather than ever blocking. Because events
are stored in PPQ, host sync and the internal Play button share one code path;
looping is a modulo over `lengthPPQ`, split when a block crosses the loop point.

Parameter changes go through an `AsyncUpdater`, so regeneration never happens on
the audio thread.

## Conventions

- Code and code comments in English. `README.md` and `docs/` are in Portuguese —
  that is the repository owner's language; keep it that way.
- Roman numerals are counted in the key's own scale: A minor reads
  `i VI III VII`, not `i bVI bIII bVII`.
- `Chord::name()` spells with sharps; `Analysis::progressionString()` and
  `Key::name()` use the accidentals the key actually calls for. Use the latter
  for anything a user reads.
- Tests are plain functions registered by the `TEST(name)` macro in
  `tests/TestHarness.h`; add new ones to the matching `TestXxx.cpp`. There is no
  mocking framework and no external test dependency.
- New MIDI files for tests go through `tests/Fixtures.h`, not on disk.
