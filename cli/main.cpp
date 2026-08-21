// Command line front end for the Harmonia engine: analyse a MIDI clip and
// render new parts over its harmony without needing a DAW.

#include "harmonia/Engine.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace harmonia;

namespace
{

void printUsage()
{
    std::cout <<
        "Harmonia - MIDI idea generator\n"
        "\n"
        "  harmonia-cli <input.mid> [options]\n"
        "\n"
        "Options:\n"
        "  --part <list>       Parts to write, comma separated: pad, chords, melody,\n"
        "                      counter, bass, arp. Default: pad,melody\n"
        "  --out <dir>         Output directory (default: current directory)\n"
        "  --prefix <name>     Output file prefix (default: input file stem)\n"
        "  --seed <n>          Random seed (default 1). Same seed = same idea.\n"
        "  --variations <n>    Write n takes of each part (default 1)\n"
        "  --bars <n>          Length in bars, looping the progression (default: clip length)\n"
        "  --density <0..1>    How busy the part is (default 0.5)\n"
        "  --complexity <0..1> Extensions and chromaticism (default 0.5)\n"
        "  --humanize <0..1>   Timing and velocity jitter (default 0.15)\n"
        "  --swing <0..1>      Swing on off-beat 16ths (default 0)\n"
        "  --octave <n>        Transpose the part by n octaves (default 0)\n"
        "  --voices <n>        Maximum voices in a chord (default 4)\n"
        "  --chords-per-bar <n> Force the harmonic grid (default: auto)\n"
        "  --key <name>        Force the key, e.g. \"F# minor\", \"Bb major\", \"D dorian\"\n"
        "  --reharm <0..1>     Reharmonise the detected progression before writing\n"
        "  --no-follow         Ignore the source rhythm, use the part's own feel\n"
        "  --no-avoid          Allow the new part to share the source's register\n"
        "  --info              Only print the analysis, write nothing\n"
        "  -h, --help          This text\n";
}

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

bool parseKey(const std::string& text, Key& out)
{
    std::string cleaned;
    for (char c : text)
        cleaned.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    static const char* names[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
    static const char* flats[12] = { "c", "db", "d", "eb", "e", "f", "gb", "g", "ab", "a", "bb", "b" };

    int tonic = -1;
    size_t consumed = 0;
    for (int i = 0; i < 12; ++i)
    {
        for (const char* candidate : { names[i], flats[i] })
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
    else if (rest.find("min") != std::string::npos || rest.find(" m") != std::string::npos || rest == "m")
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

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    std::string input;
    std::string outputDir = ".";
    std::string prefix;
    std::vector<std::string> partNames { "pad", "melody" };
    int variations = 1;
    float reharm = 0.0f;
    bool infoOnly = false;

    GenerateOptions generateOptions;
    AnalysisOptions analysisOptions;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };

        if (arg == "-h" || arg == "--help")            { printUsage(); return 0; }
        else if (arg == "--part")                      partNames = split(value(), ',');
        else if (arg == "--out")                       outputDir = value();
        else if (arg == "--prefix")                    prefix = value();
        else if (arg == "--seed")                      generateOptions.seed = static_cast<uint32_t>(std::stoul(value()));
        else if (arg == "--variations")                variations = std::stoi(value());
        else if (arg == "--bars")                      generateOptions.bars = std::stoi(value());
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
            Key key;
            if (! parseKey(value(), key))
            {
                std::cerr << "Could not understand that key name.\n";
                return 1;
            }
            analysisOptions.forceKey = true;
            analysisOptions.key = key;
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

    if (input.empty())
    {
        std::cerr << "No input MIDI file given.\n";
        return 1;
    }

    Engine engine;
    engine.setAnalysisOptions(analysisOptions);

    std::string error;
    if (! engine.loadSourceFromFile(input, error))
    {
        std::cerr << "Error: " << error << "\n";
        return 1;
    }

    const auto& analysis = engine.analysis();
    if (! analysis.valid)
    {
        std::cerr << "Error: " << analysis.message << "\n";
        return 1;
    }

    std::cout << "Analysed " << input << "\n";
    printAnalysis(analysis);

    if (reharm > 0.0f)
    {
        engine.applyReharmonization(generateOptions.seed, reharm);
        std::cout << "  Reharmonised   : " << engine.analysis().progressionString() << "\n"
                  << "                   " << engine.analysis().romanNumeralString() << "\n";
    }

    if (infoOnly)
        return 0;

    if (prefix.empty())
        prefix = stem(input);

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
