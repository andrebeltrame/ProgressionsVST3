#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/Analysis.h"

using namespace harmonia;

namespace
{
std::vector<int> progressionRoots(const Analysis& analysis)
{
    std::vector<int> roots;
    for (const auto& segment : analysis.progression)
        roots.push_back(segment.chord.root);
    return roots;
}
}

TEST(KeyDetectionOnDiatonicMaterial)
{
    // C - Am - F - G, the most ordinary C major loop there is.
    const auto clip = fixtures::chordClip({ { 48, 60, 64, 67 },
                                            { 45, 57, 60, 64 },
                                            { 41, 53, 57, 60 },
                                            { 43, 55, 59, 62 } });
    float confidence = 0.0f;
    const Key key = detectKey(clip, &confidence);
    CHECK_EQ(key.tonic, 0);
    CHECK(! key.isMinorMode());
    CHECK(confidence > 0.4f);
}

TEST(KeyDetectionOnMinorMaterial)
{
    // Am - F - C - G, same notes but centred on A.
    const auto clip = fixtures::chordClip({ { 45, 57, 60, 64 },
                                            { 41, 53, 57, 60 },
                                            { 45, 57, 60, 64 },
                                            { 40, 52, 55, 59 } });
    const Key key = detectKey(clip, nullptr);
    CHECK_EQ(key.tonic, 9);
    CHECK(key.isMinorMode());
}

TEST(ProgressionFromPolyphonicClip)
{
    const auto clip = fixtures::chordClip({ { 48, 60, 64, 67 },
                                            { 45, 57, 60, 64 },
                                            { 41, 53, 57, 60 },
                                            { 43, 55, 59, 62 } });
    const auto analysis = analyze(clip);
    CHECK(analysis.valid);
    CHECK_EQ(analysis.bars, 4);
    CHECK_EQ(progressionRoots(analysis), (std::vector<int> { 0, 9, 5, 7 }));
    CHECK_EQ(analysis.progression.size(), size_t(4));
    for (const auto& segment : analysis.progression)
        CHECK_EQ(segment.lengthTick, static_cast<int64_t>(kPPQ) * 4);
}

TEST(ProgressionFromBassLineOnly)
{
    // The headline feature: a mono bass line has to imply a usable progression.
    const auto clip = fixtures::bassClip({ 36, 33, 29, 31 }); // C2 A1 F1 G1
    const auto analysis = analyze(clip);

    CHECK(analysis.valid);
    CHECK(analysis.role == SourceRole::Bass);
    CHECK_EQ(progressionRoots(analysis), (std::vector<int> { 0, 9, 5, 7 }));

    // And the qualities should be the diatonic ones for the detected key.
    CHECK_EQ(analysis.key.tonic, 0);
    CHECK(analysis.progression[1].chord.containsPitchClass(0));  // Am contains C
    CHECK(analysis.progression[3].chord.containsPitchClass(11)); // G contains B
}

TEST(ProgressionFollowsFasterHarmonicRhythm)
{
    // Two chords per bar over two bars.
    auto clip = fixtures::emptySequence();
    const int64_t half = kPPQ * 2;
    const std::vector<std::vector<int>> chords { { 60, 64, 67 }, { 62, 65, 69 },
                                                 { 64, 67, 71 }, { 65, 69, 72 } };
    for (size_t i = 0; i < chords.size(); ++i)
        for (int pitch : chords[i])
            clip.notes.push_back({ static_cast<int64_t>(i) * half, half - 20, pitch, 96, 0 });
    clip.sort();

    const auto analysis = analyze(clip);
    CHECK_EQ(analysis.progression.size(), size_t(4));
    CHECK_EQ(progressionRoots(analysis), (std::vector<int> { 0, 2, 4, 5 }));
}

TEST(RoleDetection)
{
    const auto bass = fixtures::bassClip({ 36, 33 });
    CHECK(detectRole(bass, buildRhythmProfile(bass, { 4, 4, 0 }, kPPQ)) == SourceRole::Bass);

    const auto pad = fixtures::chordClip({ { 60, 64, 67 }, { 62, 65, 69 } });
    CHECK(detectRole(pad, buildRhythmProfile(pad, { 4, 4, 0 }, kPPQ)) == SourceRole::Chords);

    const auto lead = fixtures::melodyClip({ 72, 74, 76, 77, 79, 77, 76, 74 });
    CHECK(detectRole(lead, buildRhythmProfile(lead, { 4, 4, 0 }, kPPQ)) == SourceRole::Lead);
}

TEST(RhythmProfileCapturesTheGroove)
{
    const auto clip = fixtures::bassClip({ 36, 36 });
    const auto profile = buildRhythmProfile(clip, { 4, 4, 0 }, kPPQ);

    CHECK_NEAR(profile.notesPerBar, 4.0f, 0.1f);
    CHECK(profile.monophonic);
    CHECK_NEAR(profile.syncopation, 0.0f, 0.01f);
    CHECK(profile.grid[0] > 0.5f);
    CHECK(profile.grid[4] > 0.5f);
    CHECK_NEAR(profile.grid[1], 0.0f, 0.001f);

    const auto pad = fixtures::chordClip({ { 60, 64, 67 } });
    const auto padProfile = buildRhythmProfile(pad, { 4, 4, 0 }, kPPQ);
    CHECK(! padProfile.monophonic);
    CHECK_NEAR(padProfile.polyphony, 3.0f, 0.05f);
}

TEST(ProgressionStringsAreReadable)
{
    const auto clip = fixtures::chordClip({ { 48, 60, 64, 67 }, { 43, 55, 59, 62 } });
    const auto analysis = analyze(clip);
    CHECK_EQ(analysis.progressionString(), std::string("C | G"));
    CHECK_EQ(analysis.romanNumeralString(), std::string("I | V"));
}

TEST(ForcedKeyIsRespected)
{
    AnalysisOptions options;
    options.forceKey = true;
    options.key = { 2, ScaleType::Dorian };

    const auto clip = fixtures::bassClip({ 38, 43 });
    const auto analysis = analyze(clip, options);
    CHECK_EQ(analysis.key.tonic, 2);
    CHECK(analysis.key.scale == ScaleType::Dorian);
    CHECK_NEAR(analysis.keyConfidence, 1.0f, 0.001f);
}

TEST(EmptyClipIsRejectedGracefully)
{
    const auto analysis = analyze(fixtures::emptySequence());
    CHECK(! analysis.valid);
    CHECK(! analysis.message.empty());
}
