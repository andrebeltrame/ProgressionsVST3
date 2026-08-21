#include "LibraryScan.h"

LibraryScan::LibraryScan(const juce::File& folderToScan)
    : juce::Thread("Progressions library scan"), root(folderToScan)
{
}

LibraryScan::~LibraryScan()
{
    cancelled.store(true);
    stopThread(10000);
}

void LibraryScan::start()
{
    // Below the audio thread and below the UI: a scan must never make either
    // stutter, however many cores it takes.
    startThread(juce::Thread::Priority::low);
}

void LibraryScan::cancel()
{
    cancelled.store(true);
    signalThreadShouldExit();
}

void LibraryScan::run()
{
    harmonia::ScanOptions options;
    options.skipDrums = false;   // drums are skipped by the learner, not the walk
    options.keepEntries = false; // only the model is wanted
    options.shouldStop = [this] { return cancelled.load() || threadShouldExit(); };

    const auto progress = [this](const std::string&, size_t at, size_t of)
    {
        walking.store(false);
        total.store(static_cast<int>(of));
        processed.store(static_cast<int>(at));
    };

    harmonia::StyleModel learned;
    const auto index = harmonia::scanDirectory(root.getFullPathName().toStdString(),
                                               options, progress, nullptr, &learned);

    model = std::move(learned);
    scanStats = index.stats;
    walking.store(false);
    done.store(true);
}
