#pragma once

#include "harmonia/Library.h"
#include "harmonia/StyleModel.h"

#include <juce_core/juce_core.h>

#include <atomic>

/** Reads a folder of MIDI off the user's disk and learns a style model from it,
    on a background thread.

    This is the whole point of the plugin having a brain at all: a producer
    points it at their own collection and it writes with their habits. Nobody
    should have to open a terminal for that, and nothing leaves the machine.

    The scan can take minutes on a real drive - a quarter of a million files is
    normal - so it has to be interruptible and it has to say where it is. The
    index itself is thrown away; only the model is kept, which is what keeps a
    huge collection from costing hundreds of megabytes inside a host. */
class LibraryScan : private juce::Thread
{
public:
    explicit LibraryScan(const juce::File& folderToScan);
    ~LibraryScan() override;

    void start();
    /** Asks the scan to stop and returns immediately; poll `finished()`. */
    void cancel();
    bool running() const { return isThreadRunning(); }
    bool finished() const { return done.load(); }

    /** True while the walk is still counting files, when there is no total to
        show a progress bar against yet. */
    bool counting() const { return walking.load(); }
    int filesDone() const { return processed.load(); }
    int filesTotal() const { return total.load(); }
    bool wasCancelled() const { return cancelled.load(); }

    const juce::File& folder() const { return root; }
    /** Only valid once `finished()` is true. */
    const harmonia::StyleModel& result() const { return model; }
    const harmonia::ScanStats& stats() const { return scanStats; }

private:
    void run() override;

    juce::File root;
    harmonia::StyleModel model;
    harmonia::ScanStats scanStats;

    std::atomic<bool> walking { true };
    std::atomic<bool> done { false };
    std::atomic<bool> cancelled { false };
    std::atomic<int> processed { 0 };
    std::atomic<int> total { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryScan)
};
