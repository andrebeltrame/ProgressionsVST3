# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Progressions, by Nowhr Dynamics: a VST3/standalone plugin that reads a MIDI clip
(or a progression you type), works out the harmony, and writes new parts over it
— pads, melodies, counter melodies, basslines, arps, plucks.

The product is Progressions; the engine keeps the name `harmonia`. Everything a
user sees — the plugin, its bundle, its app-data folder, the README — says
Progressions. Everything under `core/`, the `harmonia` namespace, `harmonia-cli`
and the `HARMONIA_*` build options stay as they are: the engine is a separate
thing that this plugin happens to wrap, and renaming it would break every
command and every note the repository owner has written down. Plugin-layer
classes follow the product (`ProgressionsProcessor`, `ProgressionsEditor`).

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
# add -DHARMONIA_STYLE_MODEL=<file.style.json> to bake a style model in
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

### A generated part declares its loop

`generate()` sets `NoteSequence::loopLengthTicks` and calls `trimToLoop()`, and
the MIDI writer puts end-of-track there rather than after the last note-off. A
host sizes an imported clip by the end of the file, so without both a four-bar
loop whose pad rings over the bar line came into Live as a five-bar clip, and one
that stopped early came in short. `readFromMemory` records the declared length
back into the same field, so the reader and the writer say the same thing.

### Nothing detects a meter

`analyze()` copies `sequence.timeSignatureAt(0)` - the time-signature meta event
of the file - and defaults to 4/4 when there is none. The style model stores
nothing about metre, so the library cannot influence it. `AnalysisOptions::
forceTimeSignature` is the only way to disagree with a clip, and the plugin
exposes it as **Time**. Do not add meter detection without saying so here: the
current behaviour is "obey the file", and it is what the read-out claims.

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

### The Reese overlaps on purpose, but only when asked

`writeReese` is the only generator whose notes deliberately run into each other:
with `GenerateOptions::glide` set, each note reaches an eighth past where the
next one starts. A monophonic synth only portamentos between overlapping notes,
so a bassline written as separate notes can never glide however the synth is
patched - the MIDI has to allow it. That is also why the Reese is excluded from
timing humanisation in `generate()`: nudging a start earlier eats the overlap.
Anything that shortens or shifts notes has to leave this part alone.

The overlap is not free, which is why it is a switch rather than a rule. On a
*polyphonic* patch those two low notes really do sound together for an eighth:
they sum, they fight in the low end and they eat headroom. With `glide` off the
notes end exactly where the next begins - nothing overlaps, nothing sums, and no
glide is possible either. Both are correct; which one is depends on the synth.

Two rules the part cannot break, whichever way the switch is set. Nothing is
ever shorter than a beat - that is the line between a Reese and the plucked bass
next to it, and it holds however many pieces a chord is cut into. And two runs
of the **same pitch** that touch or overlap are merged into one held note: a
glide from a note to itself is nothing to hear, and a second note-on for a pitch
already sounding leaves the synth with two ons and two offs for one voice, so
the first off cuts the note short. There is a test for each.

### A Pattern is a figure, an Arp is a chord spelled out

`writePattern` exists because an arpeggio cannot be a figure. An arpeggio reads
the chord and spells it, so when the chord changes the notes change: measured on
`Am F C G`, eight bars of the Arp came out as eight different interval shapes and
six different rhythmic grids. Nothing survived a bar. A pattern is the other way
round - the figure is decided once, it repeats, and the harmony moves underneath.
The shape is the part, and everything here exists to keep it.

The contour is walked in **scale steps**, never semitones. Staying in the key is
therefore a property of the construction and not a filter run afterwards: a
figure written in degrees has no way to leave the key, whatever the settings do
to it. There is a test that hammers this across seeds and every value of
`followChords`.

`followChords` sorts notes into four cases and pulls each towards a chord tone
with a different probability - a semitone clash on a strong 16th most, a passing
note on a weak one least. Every one of those probabilities is a power of the same
number, which is what makes the ordering hold all the way up *and* the top of the
dial still mean "everything lands on a chord tone". The first version hard-zeroed
the mildest case and full follow still left notes off the chord. An earlier
version was worse: it pulled every non-chord note, and since half of any key is a
non-chord tone over a triad, a stepwise figure collapsed onto the triad - the
part turned into the exact thing it exists not to be.

The first bar is always the literal figure, whatever `motifDevelopment` is set
to: you have to hear a figure before you can hear it change. Variation drops
notes and nudges them off the grid, but never the note on the downbeat, which is
what the ear uses to recognise the figure at all.

Both paths through the rhythm and the contour work with no library at all. With
one, the bar comes from `pickPattern` - onsets, velocities *and* note lengths, so
the dynamics are the user's own - and the contour from `sampleStep`.

### The Sub is not a lower Reese

They look adjacent and they are not the same idea. The Reese *moves* - a fifth,
an octave, a step towards the next chord - and that movement is the part. A sub
that moves has stopped being a sub, so `writeSub` plays the root of whatever
chord is sounding and nothing else, inside a single octave (`defaultRange` gives
it 24..36, and the octave shift is the only thing that moves it). It is also the
one part `adjustRangeForSource` deliberately leaves alone: every other part gets
pushed out of the clip's way, and a sub pushed up to keep clear of a bassline is
no longer doing its job.

Two invariants, and both exist because breaking either is what wrecks a low end.
**Nothing overlaps** - a sub is one voice by definition, and two sounding at once
sum into something no mixing gets back - and nothing is shorter than a beat,
because a short sub is a kick. `density` is the only option that does anything:
it picks the strike grid (hold, half bar, beat) and then decides *per bar*
whether that bar rolls. Per-beat randomness was tried and is wrong: a sub
dropping single beats does not read as a bar that breathes, it reads as a fault.
It is excluded from timing humanisation for the same reason the Reese is, but a
different one: a fundamental that drifts off the downbeat fights the kick.

`PartType::Sub` is appended to the enum rather than filed next to Bass and Reese
where it belongs musically. The plugin stores the selected part as an index, so
inserting one in the middle would reopen every saved project on a different
part. The smoke test walks all `kNumParts` and checks each writes notes - a part
added to the enum but missed in a `switch` comes out silent, and nothing else
notices.

Anything that means a *particular* part must name it, never count to it. The
Sub's "never two at once" check said `kNumParts - 1`, and the moment `Pattern`
was appended it started asserting the Sub's rules against a different part.

### A locked chord is locked everywhere

`ProgressionsProcessor::lockedChords` is a decision about the session, not
something the clip said, so it lives in the processor rather than in the
`Analysis`. It means one thing and the same thing in all three places a chord
can change: `reharmonize()` skips it (and does not draw from the rng for it, so
locking one is not a silent way of changing another), `surpriseMe()` copies it
back over the invented progression, and `nudgeChord()` returns early. A lock
honoured by two paths out of three would be worse than no lock at all.

The list is cleared whenever the progression is replaced wholesale - a typed
progression, a preset, a reset, a loaded clip - because slot 2 of the new chords
is not the slot 2 that was locked. It survives a reharmonisation, a nudge and a
surprise, which all keep the same slots.

### Surprise me invents, it does not sample

`inventProgression()` (core/src/Progression.cpp) has two paths and needs both.
With a style model it picks one of the learned roman-numeral loops, weighted by
how often it turned up, and reads it in the invented key - so a surprise sounds
like the user's own records. Those loops are numerals and a count, which is the
same reason the style model as a whole carries no one's material. Without a
model, or when the learned loop will not parse, it walks the key's own functional
harmony with the two transition tables in that file. The second path is not a
fallback to be tidied away later: it is what has to work on a fresh installation
with nothing learned.

The plugin's `surpriseMe()` pins the key it chose rather than leaving it on
Auto. A surprise you cannot read the key of is half an answer, and the roman
numerals under the chips only mean anything against a key that is actually set.

### Favourites belong to the installation, not to the project

`plugin/Source/Favourites.cpp` keeps them next to the style model, in
`styleStore::directory()`, for the same reason the style model lives there: a
favourite is something the user found, and finding it again must not depend on
which set happened to be open. `favourites::midiFolder()` follows
`HARMONIA_STYLE_DIR` when it is set, so the smoke test never writes into a real
Music folder.

What is stored is the plugin's state tree, not the notes. Generation is
deterministic, so the state *is* the music - it comes back to the tick, live and
still adjustable, for a few hundred bytes. `deleteFavourite()` removes the entry
and deliberately leaves the exported MIDI where it is: those are the user's files
in the user's music folder, and deleting files nobody asked us to delete is not
something this does quietly.

### A preset takes its own mode, and the key has to follow

`Engine::applyPreset` switches to the mode the preset was written for - "vi IV I
V" only means anything in a major key. When `options.forceKey` is set it has to
update `options.key` too, or the analysis says one thing and the options another:
the next `reanalyse()` reads the same numerals in the old mode and hands back
different chords. The plugin mirrors the new mode back into `keyScale` for the
same reason - otherwise the saved state reopens the project on a different
progression. This surfaced the day `surpriseMe()` started pinning keys, and the
smoke test's "including the exact chords" is what catches it.

### The pad anchors, and never overwrites

`GenerateOptions::anchor` is the part everything else is written around. Lines
enter where it moves (`Context::anchorSlots` pulls the onset grid) and stop
doubling the voice it holds. It is deliberately **not** a groove donor - that is
`companion` - because a pad holding one chord a bar says nothing useful about a
melody's rhythm.

Regenerating the pad leaves the other parts exactly as they were and marks them
stale instead: losing a melody you had just got right because you nudged a chord
is worse than being told it no longer fits. The stamp is a fingerprint of the
pad's notes, not its seed, so turning a knob counts as a different pad.

### One part per channel, all alive at once

The processor keeps a `NoteSequence` per part and publishes a single
`RenderedPart` carrying all the live ones, each event tagged with its own
channel (`channelForPart` = base + part index, wrapped into 1..16). `panic()`
therefore sends all-notes-off on all sixteen channels: the part being replaced
is not necessarily the one that was sounding.

`previewCharacters` maps channel to a `PreviewCharacter` so the built-in sound
tells a bass from a pluck once several parts play together. It is written from
the message thread and read by the voices; a torn read is one note in the wrong
timbre and nothing worse.

### Undo is a settled-state snapshot

`captureSettledState()` runs at the end of every `regenerate()`, and
`pushUndoState()` pushes that snapshot - the state *before* the change - onto a
32-deep stack. Parameter changes arrive one at a time while a knob is being
dragged, so they only push after 500 ms of quiet; discrete actions (a preset, a
typed progression, a new idea) always push. `applyStateTree()` is shared with
`setStateInformation`, which is what makes undo cover the engine's progression
and not just the parameters.

### Plugin threading

Generation happens on the message thread and is published as a
`RenderedPart` (`plugin/Source/RenderedPart.h`) — a reference-counted list of
events in quarter-note positions — under a `SpinLock`. `processBlock` uses
`ScopedTryLockType` and skips a block rather than ever blocking. Because events
are stored in PPQ, host sync and the internal Play button share one code path;
looping is a modulo over `lengthPPQ`, split when a block crosses the loop point.

Parameter changes go through an `AsyncUpdater`, so regeneration never happens on
the audio thread.

`publish()` raises `pendingPanic`, and `processBlock` clears it by calling
`panic()` before it renders anything. Without that, every note the outgoing
part was holding keeps sounding forever: its note-off lived in a
`RenderedPart` that no longer exists. Switching part while the transport runs
is the obvious way to hit it.

### The plugin carries the model, not a path

A saved absolute path to someone's drive is worthless the moment the drive is
unmounted or the project moves, so `plugin/Source/StyleStore.cpp` resolves the
style model from three places, in this order: a file loaded into one instance
(the only one written into the project state), the copy installed in the user's
app-data folder, and one compiled into the binary with
`-DHARMONIA_STYLE_MODEL=<file>` through `juce_add_binary_data`. Loading a model
in the editor installs it, which is why every new instance already has a brain.

Two rules hold that together: `styleStore::install()` parses before it copies,
so a broken model cannot poison every future instance, and restoring state
loads with `install = false`, so opening a project never rewrites the installed
model. `parseStyleModel()` exists so the compiled-in model never needs a
temporary file. `styleStore::directory()` spells out the path per platform
rather than using `userApplicationDataDirectory`, which is `~/Library` on macOS
and the bare home folder on Linux; `harmonia-cli learn --install` mirrors those
paths and has to be changed with it.

The plugin smoke test points `HARMONIA_STYLE_DIR` at a temp folder before it
constructs anything - without that it would overwrite a real installation.

### Scanning happens in the plugin, and has to be interruptible

`plugin/Source/LibraryScan.cpp` runs `scanDirectory` on a low-priority
`juce::Thread` so a producer never has to open a terminal to give the plugin a
brain. Two `ScanOptions` fields exist for it and matter: `keepEntries = false`,
because a quarter of a million `LibraryEntry` objects is hundreds of megabytes
to hold inside a host for a model that is a few hundred KB, and `shouldStop`,
polled in the walk and before every file — it is called from every worker
thread at once, so it must only read an atomic.

The processor finishes the scan from a `juce::Timer` calling
`pollLibraryScan()`. That step is public because the smoke test has no message
loop (`JUCE_MODAL_LOOPS_PERMITTED=0` rules out `runDispatchLoopUntil`) and
drives it directly.

### The VST3 manifest stays off

`VST3_AUTO_MANIFEST FALSE` is not a style choice. JUCE writes
`Contents/Resources/moduleinfo.json` into the bundle as a post-build step, after
the bundle has been sealed, and on macOS that leaves the code signature invalid:
`codesign -v --strict` reports "a sealed resource is missing or invalid", the
system refuses to load the plugin, and the host skips it silently - it just
never appears in the plugin list, with no error anywhere to explain it. This
cost a long debugging session; do not turn it back on without checking
`codesign -v --strict` on the built bundle. The manifest only lets a host
enumerate classes without loading the module, so its absence costs scan speed
and nothing else.

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
