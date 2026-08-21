// Command line front end for the Harmonia engine: analyse MIDI, write parts
// over a progression, and index a folder full of clips.

#include "harmonia/Engine.h"
#include "harmonia/Library.h"
#include "harmonia/Presets.h"
#include "harmonia/Progression.h"
#include "harmonia/StyleModel.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
 #include <io.h>
 #define HARMONIA_STDOUT_IS_TERMINAL (_isatty(_fileno(stdout)) != 0)
#else
 #include <unistd.h>
 #define HARMONIA_STDOUT_IS_TERMINAL (isatty(fileno(stdout)) != 0)
#endif

using namespace harmonia;

namespace
{

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::vector<std::string> split(const std::string& text, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, delimiter))
        if (! item.empty())
            out.push_back(item);
    return out;
}

std::string stem(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    const auto start = slash == std::string::npos ? 0 : slash + 1;
    const auto dot = path.find_last_of('.');
    const auto end = (dot == std::string::npos || dot < start) ? path.size() : dot;
    return path.substr(start, end - start);
}

std::string toLower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool endsWith(const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool parseKeyName(const std::string& text, Key& out)
{
    const std::string cleaned = toLower(text);

    static const char* sharps[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
    static const char* flats[12]  = { "c", "db", "d", "eb", "e", "f", "gb", "g", "ab", "a", "bb", "b" };

    int tonic = -1;
    size_t consumed = 0;
    for (int i = 0; i < 12; ++i)
    {
        for (const char* candidate : { sharps[i], flats[i] })
        {
            const size_t length = std::strlen(candidate);
            if (cleaned.compare(0, length, candidate) == 0 && length > consumed)
            {
                tonic = i;
                consumed = length;
            }
        }
    }
    if (tonic < 0)
        return false;

    const std::string rest = cleaned.substr(consumed);
    ScaleType scale = ScaleType::Major;
    if (rest.find("dorian") != std::string::npos)          scale = ScaleType::Dorian;
    else if (rest.find("phrygian") != std::string::npos)   scale = ScaleType::Phrygian;
    else if (rest.find("lydian") != std::string::npos)     scale = ScaleType::Lydian;
    else if (rest.find("mixolydian") != std::string::npos) scale = ScaleType::Mixolydian;
    else if (rest.find("locrian") != std::string::npos)    scale = ScaleType::Locrian;
    else if (rest.find("harmonic") != std::string::npos)   scale = ScaleType::HarmonicMinor;
    else if (rest.find("melodic") != std::string::npos)    scale = ScaleType::MelodicMinor;
    else if (rest.find("min") != std::string::npos || rest == "m")
        scale = ScaleType::NaturalMinor;

    out = { tonic, scale };
    return true;
}

/** One self-updating line on a terminal, an occasional line when piped to a
    file - 227,000 progress lines in a log helps nobody. */
class ProgressLine
{
public:
    explicit ProgressLine(bool quiet)
        : silent(quiet), terminal(HARMONIA_STDOUT_IS_TERMINAL), started(std::chrono::steady_clock::now())
    {
    }

    void update(size_t done, size_t total, const std::string& label = {})
    {
        if (silent || total == 0)
            return;

        const auto now = std::chrono::steady_clock::now();
        const bool last = done >= total;

        if (terminal)
        {
            if (! last && now - lastDrawn < std::chrono::milliseconds(120))
                return;
        }
        else
        {
            if (! last && done % 5000 != 0)
                return;
        }
        lastDrawn = now;

        const double elapsed = std::chrono::duration<double>(now - started).count();
        const double rate = elapsed > 0.01 ? static_cast<double>(done) / elapsed : 0.0;
        const double remaining = rate > 0.5 ? static_cast<double>(total - done) / rate : -1.0;

        std::ostringstream line;
        line << "  [" << done << "/" << total << "] "
             << (100 * done / total) << "%";
        if (rate > 0.5)
            line << "  " << static_cast<long>(rate) << "/s";
        if (remaining > 1.0)
            line << "  faltam " << formatDuration(remaining);
        if (terminal && ! label.empty())
            line << "  " << shorten(label, 44);

        const auto text = line.str();
        if (terminal)
        {
            std::cout << "\r" << text;
            for (size_t i = text.size(); i < width; ++i)
                std::cout << ' ';
            std::cout << std::flush;
            width = std::max(width, text.size());
        }
        else
        {
            std::cout << text << "\n";
        }
    }

    /** Clears the line so whatever prints next starts clean. */
    void finish()
    {
        if (silent || ! terminal || width == 0)
            return;
        std::cout << "\r" << std::string(width, ' ') << "\r" << std::flush;
        width = 0;
    }

private:
    static std::string formatDuration(double seconds)
    {
        if (seconds < 90.0)
            return std::to_string(static_cast<int>(seconds)) + "s";
        if (seconds < 5400.0)
            return std::to_string(static_cast<int>(seconds / 60.0)) + "min";
        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << (seconds / 3600.0) << "h";
        return out.str();
    }

    static std::string shorten(const std::string& text, size_t limit)
    {
        if (text.size() <= limit)
            return text;
        return "..." + text.substr(text.size() - (limit - 3));
    }

    bool silent = false;
    bool terminal = false;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point lastDrawn {};
    size_t width = 0;
};

void printAnalysis(const Analysis& analysis)
{
    std::cout << "  Key            : " << analysis.key.name()
              << "  (confidence " << std::fixed << std::setprecision(2) << analysis.keyConfidence << ")\n"
              << "  Tempo          : " << std::setprecision(1) << analysis.bpm << " BPM\n"
              << "  Time signature : " << analysis.timeSignature.numerator << "/" << analysis.timeSignature.denominator << "\n"
              << "  Length         : " << analysis.bars << " bars\n"
              << "  Detected role  : " << toString(analysis.role) << "\n"
              << "  Notes per bar  : " << std::setprecision(2) << analysis.rhythm.notesPerBar
              << "   polyphony " << analysis.rhythm.polyphony
              << "   syncopation " << analysis.rhythm.syncopation << "\n"
              << "  Register       : " << noteName(analysis.rhythm.lowestPitch)
              << " .. " << noteName(analysis.rhythm.highestPitch) << "\n"
              << "  Progression    : " << analysis.progressionString() << "\n"
              << "  Roman numerals : " << analysis.romanNumeralString() << "\n";
}

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------

void printUsage()
{
    std::cout <<
        "Harmonia - MIDI idea generator\n"
        "\n"
        "  harmonia-cli <input.mid> [options]      analyse a clip and write parts over it\n"
        "  harmonia-cli --progression \"Am F C G\"    write parts over chords you type\n"
        "  harmonia-cli --preset deep-warm         write parts over a style preset\n"
        "  harmonia-cli presets [--genre X]        list the built-in progressions\n"
        "  harmonia-cli scan <folder> [options]    index a folder of MIDI files\n"
        "  harmonia-cli library [options]          search an index\n"
        "  harmonia-cli learn [options]            build a style model from part of an index\n"
        "\n"
        "Generating:\n"
        "  --part <list>        pad, chords, melody, counter, bass, arp, pluck\n"
"                       (default pad,melody). Parts are written in the order\n"
"                       given, and each one hears the one before it.\n"
        "  --progression <text> \"Am | F | C | G\" or \"i VI III VII\" - replaces the\n"
        "                       detected chords, keeping the clip's tempo and groove\n"
        "  --preset <id>        use a built-in progression (see: harmonia-cli presets)\n"
        "  --groove <file|text> borrow the rhythm of another clip: a .mid path, or a\n"
        "                       substring matched against --index\n"
        "  --style <file>       write in the style learned from your collection\n"
        "                       (the .style.json that 'scan' produces)\n"
        "  --style-amount <0..1> how much of the learned material to use (default 1)\n"
        "  --out <dir>          output folder (default: current directory)\n"
        "  --prefix <name>      output file prefix\n"
        "  --seed <n>           same seed + same settings = the same idea\n"
        "  --variations <n>     write n takes of each part\n"
        "  --bars <n>           length in bars, looping the progression\n"
        "  --bpm <n>            tempo when there is no clip to take it from\n"
        "  --bars-per-chord <n> chord length when there is no clip (default 1)\n"
        "  --density <0..1>     how busy the part is (default 0.5)\n"
        "  --complexity <0..1>  extensions and chromaticism (default 0.5)\n"
        "  --humanize <0..1>    timing and velocity jitter (default 0.15)\n"
        "  --swing <0..1>       swing on off-beat 16ths\n"
        "  --octave <n>         transpose by n octaves\n"
        "  --voices <n>         maximum voices in a chord (default 4)\n"
        "  --chords-per-bar <n> force the harmonic grid of the analysis\n"
        "  --key <name>         force the key, e.g. \"F# minor\", \"Bb major\", \"D dorian\"\n"
        "  --reharm <0..1>      reharmonise before writing\n"
        "  --no-follow          ignore the source groove\n"
        "  --no-avoid           allow the new part into the source's register\n"
        "  --info               print the analysis and stop\n"
        "\n"
        "scan:\n"
        "  --index <file>       where to write the index (default harmonia-library.json)\n"
        "  --no-recursive       only look in the folder itself\n"
        "  --max <n>            stop after n files\n"
        "  --skip-drums         leave percussion out of the index\n"
        "  --dry-run            count what is there without reading anything\n"
        "  --follow-symlinks    walk into folder symlinks and aliases too\n"
        "  --threads <n>        files to read at once (default: one per core)\n"
        "  --style <file>       where to write the style model\n"
        "                       (default: alongside the index, as *.style.json)\n"
        "  --no-learn           only catalogue, do not learn a style model\n"
        "  --quiet              no per-file output\n"
        "\n"
        "library:\n"
        "  --index <file>       index to read (default harmonia-library.json)\n"
        "  --tag <text>         match a folder name or path fragment, e.g. \"deep house\"\n"
        "  --role <name>        bass, lead, chords, pad, pluck, arp, drums\n"
        "  --key <name>         only clips in that key\n"
        "  --bpm <min-max>      tempo range, e.g. 118-126\n"
        "  --contains <text>    substring of the detected progression\n"
        "  --min-bars <n>       skip anything shorter\n"
        "  --with-drums         include percussion\n"
        "  --limit <n>          maximum results (default 40)\n"
        "  --tags               show the tag histogram instead of the clips\n"
        "  --progressions       show the progressions mined from your own clips\n"
        "  --style <file>       style model to read for --progressions\n"
        "\n"
        "learn:  takes the same filters as library, and learns from what matches.\n"
        "        One scan of a big drive, then as many focused models as you want.\n"
        "  --index <file>       index to learn from (default harmonia-library.json)\n"
        "  --style <file>       where to write the model (required)\n"
        "  --quiet              no progress output\n"
        "\n"
        "  harmonia-cli scan /Volumes/Drive --index ~/all.json --no-learn\n"
        "  harmonia-cli learn --index ~/all.json --tag \"melodic house\" --style ~/melodic.style.json\n"
        "  harmonia-cli --preset melodic-lift --style ~/melodic.style.json --part bass,melody\n";
}

// ---------------------------------------------------------------------------
// presets
// ---------------------------------------------------------------------------

int runPresets(const std::vector<std::string>& args)
{
    std::string styleFilter;
    for (size_t i = 0; i < args.size(); ++i)
        if ((args[i] == "--genre" || args[i] == "--style") && i + 1 < args.size())
            styleFilter = toLower(args[++i]);

    std::string currentStyle;
    for (const auto& preset : progressionPresets())
    {
        if (! styleFilter.empty() && toLower(preset.style).find(styleFilter) == std::string::npos)
            continue;

        if (preset.style != currentStyle)
        {
            currentStyle = preset.style;
            std::cout << "\n" << currentStyle << "\n";
        }
        std::cout << "  " << std::left << std::setw(22) << preset.id
                  << std::setw(22) << preset.name
                  << preset.numerals << "\n"
                  << "  " << std::setw(22) << "" << toString(preset.mode) << " - " << preset.note << "\n";
    }
    std::cout << "\nUse one with:  harmonia-cli --preset <id> --key \"F minor\" --part pad,melody\n";
    return 0;
}

// ---------------------------------------------------------------------------
// scan
// ---------------------------------------------------------------------------

int runScan(const std::vector<std::string>& args)
{
    if (args.empty() || args[0].rfind("--", 0) == 0)
    {
        std::cerr << "scan needs a folder: harmonia-cli scan /path/to/midis\n";
        return 1;
    }

    const std::string root = args[0];
    std::string indexPath = "harmonia-library.json";
    std::string stylePath;
    ScanOptions options;
    bool quiet = false;
    bool learn = true;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        const auto value = [&]() -> std::string { return (i + 1 < args.size()) ? args[++i] : std::string(); };

        if (arg == "--index")             indexPath = value();
        else if (arg == "--no-recursive") options.recursive = false;
        else if (arg == "--max")          options.maxFiles = static_cast<size_t>(std::stoul(value()));
        else if (arg == "--skip-drums")   options.skipDrums = true;
        else if (arg == "--style")            stylePath = value();
        else if (arg == "--no-learn")         learn = false;
        else if (arg == "--follow-symlinks")  options.followSymlinks = true;
        else if (arg == "--dry-run")          options.dryRun = true;
        else if (arg == "--threads")          options.threads = static_cast<unsigned>(std::stoul(value()));
        else if (arg == "--quiet")            quiet = true;
        else
        {
            std::cerr << "Unknown scan option: " << arg << "\n";
            return 1;
        }
    }

    std::vector<std::string> errors;
    ProgressLine progressLine(quiet);
    const auto progress = [&progressLine](const std::string& relative, size_t done, size_t total)
    {
        progressLine.update(done, total, relative);
    };

    if (stylePath.empty())
    {
        const auto dot = indexPath.find_last_of('.');
        stylePath = (dot == std::string::npos ? indexPath : indexPath.substr(0, dot)) + ".style.json";
    }

    std::cout << "Scanning " << root << "\n";
    StyleModel style;
    const auto index = scanDirectory(root, options, progress, &errors, learn ? &style : nullptr);
    progressLine.finish();

    const auto& stats = index.stats;

    // What the walk actually saw, so a scan that missed most of a drive is
    // obvious instead of looking like a small drive.
    std::cout << "\nWalked " << stats.directoriesVisited << " folders, "
              << stats.filesSeen << " files\n"
              << "  MIDI files found : " << stats.midiFilesFound << "\n";
    if (! options.dryRun)
        std::cout << "  Indexed          : " << stats.indexed << "\n";
    if (stats.unreadable > 0)
        std::cout << "  Unreadable       : " << stats.unreadable << "\n";
    if (stats.skippedDrums > 0)
        std::cout << "  Skipped (drums)  : " << stats.skippedDrums << "\n";
    if (stats.droppedByLimit > 0)
        std::cout << "  Dropped by --max : " << stats.droppedByLimit << "\n";
    if (stats.systemFilesSkipped > 0)
        std::cout << "  System files     : " << stats.systemFilesSkipped
                  << "   (macOS \"._\" twins and hidden files, ignored)\n";
    if (stats.walkErrors > 0)
        std::cout << "  Folders refused  : " << stats.walkErrors
                  << "   (permissions, or a disk that went away)\n";

    if (! stats.otherExtensions.empty())
    {
        std::vector<std::pair<std::string, int>> others(stats.otherExtensions.begin(),
                                                        stats.otherExtensions.end());
        std::sort(others.begin(), others.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        std::cout << "  Other files      :";
        for (size_t i = 0; i < others.size() && i < 8; ++i)
            std::cout << " " << others[i].first << "(" << others[i].second << ")";
        std::cout << "\n";
    }

    if (stats.droppedByLimit > 0)
        std::cout << "\n  Note: --max stopped the scan early. Drop it to take everything.\n";
    if (stats.midiFilesFound == 0 && stats.filesSeen > 0)
        std::cout << "\n  Note: files were found but none of them are .mid or .midi.\n"
                  << "        If your collection is inside .zip or .als project files,\n"
                  << "        unpack it first - the scanner reads loose MIDI files only.\n";
    if (stats.directoriesVisited <= 1 && stats.filesSeen == 0)
        std::cout << "\n  Note: nothing under that path. Check the path, and try\n"
                  << "        --follow-symlinks if your folders are aliases.\n";

    if (! stats.midiByTopFolder.empty() && stats.midiByTopFolder.size() > 1)
    {
        std::vector<std::pair<std::string, int>> folders(stats.midiByTopFolder.begin(),
                                                         stats.midiByTopFolder.end());
        std::sort(folders.begin(), folders.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        std::cout << "\nMIDI by top-level folder:\n";
        int shown = 0;
        for (const auto& [folder, count] : folders)
        {
            if (shown++ >= 20)
            {
                std::cout << "  ... and " << (folders.size() - 20) << " more folders\n";
                break;
            }
            std::cout << "  " << std::right << std::setw(7) << count << "  " << folder << "\n";
        }
        std::cout << "\nScan one of these on its own to keep the style model focused:\n"
                  << "  harmonia-cli scan \"" << root << "/" << folders.front().first
                  << "\" --index ~/" << "focused.json\n";
    }

    if (options.dryRun)
    {
        std::cout << "\nDry run - nothing was read or written.\n";
        return stats.midiFilesFound > 0 ? 0 : 1;
    }

    if (index.entries.empty())
    {
        std::cerr << "\nNo readable MIDI files were indexed under " << root << "\n";
        for (size_t i = 0; i < errors.size() && i < 10; ++i)
            std::cerr << "  " << errors[i] << "\n";
        return 1;
    }

    std::string error;
    if (! saveIndex(indexPath, index, error))
    {
        std::cerr << "Error: " << error << "\n";
        return 1;
    }

    std::map<std::string, int> roles;
    int drums = 0;
    for (const auto& entry : index.entries)
    {
        ++roles[entry.folderRole.empty() ? toLower(toString(entry.role)) : entry.folderRole];
        if (entry.drums)
            ++drums;
    }

    std::cout << "\nIndexed " << index.entries.size() << " clips into " << indexPath << "\n"
              << "  Percussion     : " << drums << "\n  By role        :";
    for (const auto& [role, count] : roles)
        std::cout << " " << role << "=" << count;
    std::cout << "\n  Top folders    :";
    for (const auto& [tag, count] : tagHistogram(index, 8))
        std::cout << " " << tag << "(" << count << ")";
    std::cout << "\n";

    if (learn && ! style.empty())
    {
        if (! saveStyleModel(stylePath, style, error))
        {
            std::cerr << "Error: " << error << "\n";
            return 1;
        }
        std::cout << "\nLearned a style model into " << stylePath << "\n"
                  << "  " << style.summary() << "\n";

        const auto top = style.topProgressions(5);
        if (! top.empty())
        {
            std::cout << "  Your most common progressions:\n";
            for (const auto& [progression, count] : top)
                std::cout << "    " << std::left << std::setw(40) << progression << count << " clips\n";
        }
        std::cout << "\nWrite in your own style with:\n"
                  << "  harmonia-cli --preset melodic-lift --key \"F minor\" --style " << stylePath
                  << " --part bass,melody\n";
    }

    if (! errors.empty())
    {
        std::cout << "\n" << errors.size() << " files could not be read:\n";
        for (size_t i = 0; i < errors.size() && i < 20; ++i)
            std::cout << "  " << errors[i] << "\n";
        if (errors.size() > 20)
            std::cout << "  ... and " << (errors.size() - 20) << " more\n";
    }

    std::cout << "\nSearch the catalogue with:  harmonia-cli library --index " << indexPath << " --role bass\n";

    if (! errors.empty())
    {
        std::cout << "\nUnreadable files:\n";
        for (size_t i = 0; i < errors.size() && i < 20; ++i)
            std::cout << "  " << errors[i] << "\n";
        if (errors.size() > 20)
            std::cout << "  ... and " << (errors.size() - 20) << " more\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// library
// ---------------------------------------------------------------------------

int runLibrary(const std::vector<std::string>& args)
{
    std::string indexPath = "harmonia-library.json";
    LibraryQuery query;
    query.limit = 40;
    bool showTags = false;
    bool showProgressions = false;
    std::string stylePath;

    for (size_t i = 0; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        const auto value = [&]() -> std::string { return (i + 1 < args.size()) ? args[++i] : std::string(); };

        if (arg == "--index")            indexPath = value();
        else if (arg == "--tag")         query.tag = value();
        else if (arg == "--role")        query.role = value();
        else if (arg == "--contains")    query.containing = value();
        else if (arg == "--min-bars")    query.minBars = std::stoi(value());
        else if (arg == "--with-drums")  query.excludeDrums = false;
        else if (arg == "--limit")       query.limit = static_cast<size_t>(std::stoul(value()));
        else if (arg == "--tags")        showTags = true;
        else if (arg == "--progressions") showProgressions = true;
        else if (arg == "--style")       stylePath = value();
        else if (arg == "--key")
        {
            Key key;
            if (! parseKeyName(value(), key))
            {
                std::cerr << "Could not understand that key name.\n";
                return 1;
            }
            query.tonic = key.tonic;
            query.matchMode = true;
            query.mode = key.scale;
        }
        else if (arg == "--bpm")
        {
            const auto range = split(value(), '-');
            if (! range.empty())
                query.minBpm = std::stod(range[0]);
            if (range.size() > 1)
                query.maxBpm = std::stod(range[1]);
        }
        else
        {
            std::cerr << "Unknown library option: " << arg << "\n";
            return 1;
        }
    }

    if (showProgressions)
    {
        if (stylePath.empty())
        {
            const auto dot = indexPath.find_last_of('.');
            stylePath = (dot == std::string::npos ? indexPath : indexPath.substr(0, dot)) + ".style.json";
        }

        StyleModel style;
        std::string styleError;
        if (! loadStyleModel(stylePath, style, styleError))
        {
            std::cerr << "Error: " << styleError << "\n"
                      << "Run a scan first:  harmonia-cli scan /path/to/midis\n";
            return 1;
        }

        const auto top = style.topProgressions(static_cast<size_t>(query.limit));
        std::cout << "Mined from " << style.clipsLearned << " of your clips\n\n";
        for (const auto& [progression, count] : top)
            std::cout << "  " << std::left << std::setw(46) << progression << count << " clips\n";
        std::cout << "\nUse one with:  harmonia-cli --progression \"" 
                  << (top.empty() ? std::string("i | VI | III | VII") : top.front().first)
                  << "\" --key \"F minor\" --part pad,melody\n";
        return top.empty() ? 1 : 0;
    }

    LibraryIndex index;
    std::string error;
    if (! loadIndex(indexPath, index, error))
    {
        std::cerr << "Error: " << error << "\n"
                  << "Build an index first:  harmonia-cli scan /path/to/midis\n";
        return 1;
    }

    if (showTags)
    {
        std::cout << index.entries.size() << " clips under " << index.root << "\n\n";
        for (const auto& [tag, count] : tagHistogram(index))
            std::cout << "  " << std::left << std::setw(40) << tag << count << "\n";
        return 0;
    }

    const auto results = queryLibrary(index, query);
    std::cout << results.size() << " of " << index.entries.size() << " clips match\n\n";
    for (const auto* entry : results)
        std::cout << "  " << entry->summary() << "\n";

    if (! results.empty())
        std::cout << "\nUse one as a groove donor:  harmonia-cli --preset deep-warm --index "
                  << indexPath << " --groove \"" << results.front()->name << "\" --part pluck\n";
    return results.empty() ? 1 : 0;
}

// ---------------------------------------------------------------------------
// learn
// ---------------------------------------------------------------------------

int runLearn(const std::vector<std::string>& args)
{
    std::string indexPath = "harmonia-library.json";
    std::string stylePath;
    LibraryQuery query;
    bool quiet = false;

    for (size_t i = 0; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        const auto value = [&]() -> std::string { return (i + 1 < args.size()) ? args[++i] : std::string(); };

        if (arg == "--index")            indexPath = value();
        else if (arg == "--style")       stylePath = value();
        else if (arg == "--tag")         query.tag = value();
        else if (arg == "--role")        query.role = value();
        else if (arg == "--contains")    query.containing = value();
        else if (arg == "--min-bars")    query.minBars = std::stoi(value());
        else if (arg == "--with-drums")  query.excludeDrums = false;
        else if (arg == "--limit")       query.limit = static_cast<size_t>(std::stoul(value()));
        else if (arg == "--quiet")       quiet = true;
        else if (arg == "--key")
        {
            Key key;
            if (! parseKeyName(value(), key))
            {
                std::cerr << "Could not understand that key name.\n";
                return 1;
            }
            query.tonic = key.tonic;
            query.matchMode = true;
            query.mode = key.scale;
        }
        else if (arg == "--bpm")
        {
            const auto range = split(value(), '-');
            if (! range.empty())
                query.minBpm = std::stod(range[0]);
            if (range.size() > 1)
                query.maxBpm = std::stod(range[1]);
        }
        else
        {
            std::cerr << "Unknown learn option: " << arg << "\n";
            return 1;
        }
    }

    if (stylePath.empty())
    {
        std::cerr << "learn needs --style <file>, so it does not overwrite the model your scan made.\n"
                  << "  harmonia-cli learn --index ~/all.json --tag \"melodic house\" --style ~/melodic.style.json\n";
        return 1;
    }

    LibraryIndex index;
    std::string error;
    if (! loadIndex(indexPath, index, error))
    {
        std::cerr << "Error: " << error << "\n"
                  << "Build an index first:  harmonia-cli scan /path/to/midis --no-learn\n";
        return 1;
    }

    const auto matches = queryLibrary(index, query);
    std::cout << "Learning from " << matches.size() << " of " << index.entries.size() << " clips\n";
    if (matches.empty())
    {
        std::cerr << "Nothing matched those filters. Try 'harmonia-cli library --index "
                  << indexPath << " --tags' to see what is there.\n";
        return 1;
    }

    std::vector<std::string> errors;
    ProgressLine progressLine(quiet);
    const auto progress = [&progressLine](size_t done, size_t total)
    {
        progressLine.update(done, total);
    };

    const auto model = buildStyleModel(matches, progress, &errors);
    progressLine.finish();

    if (model.empty())
    {
        std::cerr << "None of those clips could be read.\n";
        return 1;
    }

    if (! saveStyleModel(stylePath, model, error))
    {
        std::cerr << "Error: " << error << "\n";
        return 1;
    }

    std::cout << "Wrote " << stylePath << "\n  " << model.summary() << "\n";
    if (! errors.empty())
        std::cout << "  " << errors.size() << " clips could not be read and were skipped\n";

    const auto top = model.topProgressions(6);
    if (! top.empty())
    {
        std::cout << "  Most common progressions here:\n";
        for (const auto& [progression, count] : top)
            std::cout << "    " << std::left << std::setw(40) << progression << count << " clips\n";
    }

    std::cout << "\nWrite with it:\n"
              << "  harmonia-cli --preset melodic-lift --style " << stylePath << " --part bass,melody\n";
    return 0;
}

// ---------------------------------------------------------------------------
// generate
// ---------------------------------------------------------------------------

int runGenerate(const std::vector<std::string>& args)
{
    std::string input;
    std::string outputDir = ".";
    std::string prefix;
    std::string progressionText;
    std::string presetId;
    std::string grooveSpec;
    std::string stylePath;
    std::string indexPath = "harmonia-library.json";
    std::vector<std::string> partNames { "pad", "melody" };
    int variations = 1;
    int barsPerChord = 1;
    double bpm = 122.0;
    float reharm = 0.0f;
    bool infoOnly = false;
    bool haveKey = false;
    Key forcedKey;

    GenerateOptions generateOptions;
    AnalysisOptions analysisOptions;

    for (size_t i = 0; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        const auto value = [&]() -> std::string { return (i + 1 < args.size()) ? args[++i] : std::string(); };

        if (arg == "-h" || arg == "--help")            { printUsage(); return 0; }
        else if (arg == "--part")                      partNames = split(value(), ',');
        else if (arg == "--progression")               progressionText = value();
        else if (arg == "--preset")                    presetId = value();
        else if (arg == "--groove")                    grooveSpec = value();
        else if (arg == "--style")                     stylePath = value();
        else if (arg == "--style-amount")              generateOptions.styleAmount = std::stof(value());
        else if (arg == "--index")                     indexPath = value();
        else if (arg == "--out")                       outputDir = value();
        else if (arg == "--prefix")                    prefix = value();
        else if (arg == "--seed")                      generateOptions.seed = static_cast<uint32_t>(std::stoul(value()));
        else if (arg == "--variations")                variations = std::stoi(value());
        else if (arg == "--bars")                      generateOptions.bars = std::stoi(value());
        else if (arg == "--bpm")                       bpm = std::stod(value());
        else if (arg == "--bars-per-chord")            barsPerChord = std::stoi(value());
        else if (arg == "--density")                   generateOptions.density = std::stof(value());
        else if (arg == "--complexity")                generateOptions.complexity = std::stof(value());
        else if (arg == "--humanize")                  generateOptions.humanize = std::stof(value());
        else if (arg == "--swing")                     generateOptions.swing = std::stof(value());
        else if (arg == "--octave")                    generateOptions.octaveShift = std::stoi(value());
        else if (arg == "--voices")                    generateOptions.maxVoices = std::stoi(value());
        else if (arg == "--chords-per-bar")            analysisOptions.chordsPerBar = std::stoi(value());
        else if (arg == "--reharm")                    reharm = std::stof(value());
        else if (arg == "--no-follow")                 generateOptions.followSourceRhythm = false;
        else if (arg == "--no-avoid")                  generateOptions.avoidSourceCollisions = false;
        else if (arg == "--info")                      infoOnly = true;
        else if (arg == "--key")
        {
            if (! parseKeyName(value(), forcedKey))
            {
                std::cerr << "Could not understand that key name.\n";
                return 1;
            }
            haveKey = true;
        }
        else if (! arg.empty() && arg[0] == '-')
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
        else
        {
            input = arg;
        }
    }

    if (input.empty() && progressionText.empty() && presetId.empty())
    {
        std::cerr << "Give me a MIDI file, a --progression or a --preset.\n\n";
        printUsage();
        return 1;
    }

    Engine engine;
    if (haveKey)
    {
        analysisOptions.forceKey = true;
        analysisOptions.key = forcedKey;
    }
    engine.setAnalysisOptions(analysisOptions);
    engine.setBlankCanvas(bpm, barsPerChord);

    std::string error;
    if (! input.empty())
    {
        if (! engine.loadSourceFromFile(input, error))
        {
            std::cerr << "Error: " << error << "\n";
            return 1;
        }
        std::cout << "Analysed " << input << "\n";
        printAnalysis(engine.analysis());
    }

    if (! presetId.empty())
    {
        if (! engine.applyPreset(presetId, error))
        {
            std::cerr << "Error: " << error << "\n";
            return 1;
        }
        const auto* preset = findProgressionPreset(presetId);
        std::cout << "Preset '" << preset->name << "' (" << preset->style << ")\n";
    }

    if (! progressionText.empty())
    {
        if (! engine.setProgressionText(progressionText, error))
        {
            std::cerr << "Error: " << error << "\n";
            return 1;
        }
    }

    if (! engine.analysis().valid)
    {
        std::cerr << "Error: " << (engine.analysis().message.empty() ? "nothing to work with."
                                                                    : engine.analysis().message) << "\n";
        return 1;
    }

    if (input.empty() || ! presetId.empty() || ! progressionText.empty())
    {
        std::cout << (input.empty() ? "Writing over\n" : "Progression replaced\n");
        std::cout << "  Key            : " << engine.analysis().key.name() << "\n"
                  << "  Progression    : " << engine.analysis().progressionString() << "\n"
                  << "  Roman numerals : " << engine.analysis().romanNumeralString() << "\n"
                  << "  Length         : " << engine.analysis().bars << " bars at "
                  << std::fixed << std::setprecision(1) << engine.analysis().bpm << " BPM\n";
    }

    // A groove donor: either a MIDI file or something out of the library index.
    if (! grooveSpec.empty())
    {
        NoteSequence donorClip;
        std::string donorName;

        if (endsWith(toLower(grooveSpec), ".mid") || endsWith(toLower(grooveSpec), ".midi"))
        {
            if (! midi::readFromFile(grooveSpec, donorClip, error))
            {
                std::cerr << "Error reading groove donor: " << error << "\n";
                return 1;
            }
            donorName = grooveSpec;
            generateOptions.grooveDonor = analyze(donorClip).rhythm;
            generateOptions.useGrooveDonor = true;
        }
        else
        {
            LibraryIndex index;
            if (! loadIndex(indexPath, index, error))
            {
                std::cerr << "Error: " << error << "\n"
                          << "--groove needs either a .mid path or an index built with 'scan'.\n";
                return 1;
            }

            LibraryQuery query;
            query.tag = grooveSpec;
            query.limit = 1;
            const auto matches = queryLibrary(index, query);
            if (matches.empty())
            {
                std::cerr << "Nothing in the library matches '" << grooveSpec << "'.\n";
                return 1;
            }
            donorName = matches.front()->relativePath;
            generateOptions.grooveDonor = matches.front()->rhythm;
            generateOptions.useGrooveDonor = true;
        }
        std::cout << "  Groove from    : " << donorName << "\n";
    }

    StyleModel style;
    if (! stylePath.empty())
    {
        if (! loadStyleModel(stylePath, style, error))
        {
            std::cerr << "Error: " << error << "\n";
            return 1;
        }
        generateOptions.style = &style;
        std::cout << "  Style         : " << style.summary() << "\n";
    }

    if (reharm > 0.0f)
    {
        engine.applyReharmonization(generateOptions.seed, reharm);
        std::cout << "  Reharmonised   : " << engine.analysis().progressionString() << "\n"
                  << "                   " << engine.analysis().romanNumeralString() << "\n";
    }

    if (infoOnly)
        return 0;

    if (prefix.empty())
        prefix = input.empty() ? (presetId.empty() ? "harmonia" : presetId) : stem(input);

    int written = 0;
    NoteSequence previousPart;
    for (const auto& name : partNames)
    {
        PartType part;
        if (! partTypeFromString(name, part))
        {
            std::cerr << "Unknown part '" << name << "', skipping.\n";
            continue;
        }

        GenerateOptions options = generateOptions;
        options.part = part;

        // Each part hears the one written before it, so a melody locks to the
        // bass it is sitting on instead of being composed in isolation.
        options.companion = previousPart.empty() ? nullptr : &previousPart;

        const auto takes = engine.generateVariations(options, std::max(1, variations));
        if (! takes.empty() && ! takes.front().empty())
            previousPart = takes.front();
        for (size_t i = 0; i < takes.size(); ++i)
        {
            if (takes[i].empty())
            {
                std::cerr << "Nothing generated for " << name << ".\n";
                continue;
            }

            std::string path = outputDir + "/" + prefix + "_" + name;
            if (takes.size() > 1)
                path += "_" + std::to_string(i + 1);
            path += ".mid";

            if (! midi::writeToFile(path, takes[i], error))
            {
                std::cerr << "Error: " << error << "\n";
                return 1;
            }
            std::cout << "  -> " << path << "  (" << takes[i].notes.size() << " notes)\n";
            ++written;
        }
    }

    if (written == 0)
    {
        std::cerr << "No parts were written.\n";
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty())
    {
        printUsage();
        return 1;
    }

    const std::string& first = args.front();
    const std::vector<std::string> rest(args.begin() + 1, args.end());

    if (first == "help" || first == "-h" || first == "--help")
    {
        printUsage();
        return 0;
    }
    if (first == "scan")
        return runScan(rest);
    if (first == "library")
        return runLibrary(rest);
    if (first == "presets")
        return runPresets(rest);
    if (first == "learn")
        return runLearn(rest);

    return runGenerate(args);
}
