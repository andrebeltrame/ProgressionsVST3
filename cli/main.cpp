// Command line front end for the Harmonia engine: analyse MIDI, write parts
// over a progression, and index a folder full of clips.

#include "harmonia/Engine.h"
#include "harmonia/Library.h"
#include "harmonia/Presets.h"
#include "harmonia/Progression.h"

#include <cstring>
#include <map>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
        "  harmonia-cli presets [--style X]        list the built-in progressions\n"
        "  harmonia-cli scan <folder> [options]    index a folder of MIDI files\n"
        "  harmonia-cli library [options]          search an index\n"
        "\n"
        "Generating:\n"
        "  --part <list>        pad, chords, melody, counter, bass, arp (default pad,melody)\n"
        "  --progression <text> \"Am | F | C | G\" or \"i VI III VII\" - replaces the\n"
        "                       detected chords, keeping the clip's tempo and groove\n"
        "  --preset <id>        use a built-in progression (see: harmonia-cli presets)\n"
        "  --groove <file|text> borrow the rhythm of another clip: a .mid path, or a\n"
        "                       substring matched against --index\n"
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
        "  --tags               show the tag histogram instead of the clips\n";
}

// ---------------------------------------------------------------------------
// presets
// ---------------------------------------------------------------------------

int runPresets(const std::vector<std::string>& args)
{
    std::string styleFilter;
    for (size_t i = 0; i < args.size(); ++i)
        if (args[i] == "--style" && i + 1 < args.size())
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
    ScanOptions options;
    bool quiet = false;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        const auto value = [&]() -> std::string { return (i + 1 < args.size()) ? args[++i] : std::string(); };

        if (arg == "--index")             indexPath = value();
        else if (arg == "--no-recursive") options.recursive = false;
        else if (arg == "--max")          options.maxFiles = static_cast<size_t>(std::stoul(value()));
        else if (arg == "--skip-drums")   options.skipDrums = true;
        else if (arg == "--quiet")        quiet = true;
        else
        {
            std::cerr << "Unknown scan option: " << arg << "\n";
            return 1;
        }
    }

    std::vector<std::string> errors;
    const auto progress = [quiet](const std::string& relative, size_t done, size_t total)
    {
        if (! quiet)
            std::cout << "  [" << done << "/" << total << "] " << relative << "\n";
    };

    std::cout << "Scanning " << root << "\n";
    const auto index = scanDirectory(root, options, progress, &errors);

    if (index.entries.empty())
    {
        std::cerr << "No readable MIDI files found under " << root << "\n";
        for (const auto& error : errors)
            std::cerr << "  " << error << "\n";
        return 1;
    }

    std::string error;
    if (! saveIndex(indexPath, index, error))
    {
        std::cerr << "Error: " << error << "\n";
        return 1;
    }

    // A quick summary of what the collection actually contains.
    std::map<std::string, int> roles;
    int drums = 0;
    for (const auto& entry : index.entries)
    {
        ++roles[entry.folderRole.empty() ? toLower(toString(entry.role)) : entry.folderRole];
        if (entry.drums)
            ++drums;
    }

    std::cout << "\nIndexed " << index.entries.size() << " clips into " << indexPath << "\n";
    if (! errors.empty())
        std::cout << "  " << errors.size() << " files could not be read (use --quiet off to see them)\n";
    std::cout << "  Percussion     : " << drums << "\n  By role        :";
    for (const auto& [role, count] : roles)
        std::cout << " " << role << "=" << count;
    std::cout << "\n  Top folders    :";
    for (const auto& [tag, count] : tagHistogram(index, 8))
        std::cout << " " << tag << "(" << count << ")";
    std::cout << "\n\nSearch it with:  harmonia-cli library --index " << indexPath << " --role bass\n";

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

        const auto takes = engine.generateVariations(options, std::max(1, variations));
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

    return runGenerate(args);
}
