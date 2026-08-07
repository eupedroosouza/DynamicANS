#include <bitset>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

#include "Common/Context.h"
#include "Dec/ANSDecoder.h"
#include "Enc/ANSEncoder.h"
#include "Utils/Log.h"
#include "Utils/Memory.h"
#include "Utils/Stats.h"

static bool g_doEncode;
static bool g_doDecode;
static std::string g_tablesFile;
static std::string g_inputFile;
static std::string g_bitstreamFile;
static uint32_t g_adaptativeInterval;
static bool g_useEncodedFile = false;
static std::string g_encodedFile;
static bool g_useDecodedFile = false;
static std::string g_decodedFile;

static bool parseArgs(const int argc, char *argv[]) {
    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i < argc; i++) {
        std::string key = argv[i];

        if (key == "--encode") {
            g_doEncode = true;
        } else if (key == "--decode") {
            g_doDecode = true;
        }
        // starts with -- (or -)
        else if (key.rfind("--", 0) == 0 || key.rfind('-', 0) == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << key << "\n";
                return false;
            }

            args[key] = argv[i + 1];
            i++;
        } else {
            std::cerr << "Unknown argument: " << key << "\n";
            return false;
        }
    }

    // Default: if neither specified → do both
    if (!g_doEncode && !g_doDecode) {
        g_doEncode = true;
        g_doDecode = true;
    }

    // Required arguments
    if (g_doEncode) {
        if (args.count("--input")) {
            g_inputFile = args["--input"];
        }
        if (args.count("-i")) {
            g_inputFile = args["-i"];
        } else {
            std::cerr << "--input (-i) path required for encode\n";
            return false;
        }
    }

    if (g_doDecode) {
        if (!g_doEncode && (!args.count("--bitstream") || !args.count("--bs"))) {
            std::cerr << "Decode-only mode requires --bitstream (-bs)\n";
            return false;
        }
        if (args.count("--bitstream")) {
            g_bitstreamFile = args["--bitstream"];
        } else if (args.count("-bs")) {
            g_bitstreamFile = args["-bs"];
        }
    }

    if (args.count("--tables")) {
        g_tablesFile = args["--tables"];
    } else if (args.count("-t")) {
        g_tablesFile = args["-t"];
    } else {
        std::cerr << "--tables (-t) is required\n";
        return false;
    }

    if (args.count("--adaptativeInterval")) {
        g_adaptativeInterval = static_cast<uint32_t>(std::stoi(args["--adaptativeInterval"]));
    } else if (args.count("-ai")) {
        g_adaptativeInterval = static_cast<uint32_t>(std::stoi(args["-ai"]));
    } else {
        std::cerr << "--adaptativeInterval (-ai) is required\n";
        return false;
    }

    if (args.count("--encodedFile")) {
        g_encodedFile = args["--encodedFile"];
        g_useEncodedFile = true;
    } else if (args.count("-ef")) {
        g_encodedFile = args["-ef"];
        g_useEncodedFile = true;
    }

    if (args.count("--decodedFile")) {
        g_decodedFile = args["--decodedFile"];
        g_useDecodedFile = true;
    } else if (args.count("-df")) {
        g_decodedFile = args["-df"];
        g_useDecodedFile = true;
    }

    return true;
}

int main(const int argc, char *argv[]) {
    if (!parseArgs(argc, argv)) {
        std::cout <<
                "Usage:\n [--encode]\n [--decode]\n --tables (-t) <tables path>\n --adaptativeInterval (-ai) <adaptative interval>\n [--input (-in) <input path>]\n [--bitstream (-bs)]"
                << std::endl;
        return -1;
    }
    time_t timestamp = std::time(nullptr);
    Stats::init();

    Logger::init("logs/" + std::to_string(timestamp) + ".txt");
    Logger::get().log("Starting codec at " + std::to_string(timestamp));

    Logger::get().log("Configuration: ");
    Logger::get().log(" Context (tables/range): " + g_tablesFile);
    Logger::get().log(" Adaptive interval: " + std::to_string(g_adaptativeInterval));
    Logger::get().log(" Input file: " + g_inputFile);


    std::cout << "=== CONTEXT ===" << std::endl;
    size_t beforeLoadContextMemUsage = getCurrentRSS();
    Context context = Context::loadContextFromFile(g_tablesFile, g_adaptativeInterval);
    size_t afterLoadContextMemUsage = getCurrentRSS();
    size_t memUsageByContext = afterLoadContextMemUsage - beforeLoadContextMemUsage;
    std::cout << "Memory usage (MB):" << std::endl;
    std::cout << "  before: " << std::to_string(toMB(beforeLoadContextMemUsage)) << std::endl;
    std::cout << "  after: " << std::to_string(toMB(afterLoadContextMemUsage)) << std::endl;
    std::cout << "  usage by context: " << std::to_string(toMB(memUsageByContext)) << std::endl;


    std::vector<uint8_t> input;
    std::vector<uint8_t> bytestream;
    if (g_doEncode) {
        std::cout << "=== ENCODING SUMMARY ===" << std::endl;
        ANSEncoder encoder(&context);

        // Load input
        std::ifstream file(g_inputFile);
        if (!file.is_open()) {
            std::cerr << "Fail open input file: " + g_inputFile;
            return -1;
        }

        std::string s;
        char c;
        while (file >> c) {
            input.push_back(c - '0');
        }
        file.close();
        Logger::get().log("Loaded " + std::to_string(input.size()) + " bits from input.");

        PeakMemorySampler encSampler;
        size_t baselineEncMem = getCurrentRSS();
        encSampler.start();
        auto encStart = std::chrono::high_resolution_clock::now();

        Logger::get().log("Starting encode...");
        // Inversed encode
        for (uint8_t bit : input) {
            encoder.encodeBin(bit);
        }
        encoder.encodeBins(input.size() , 32);

        bytestream = encoder.finishEncoding();

        MemoryStats encMemStats = encSampler.stop(baselineEncMem);
        auto encEnd = std::chrono::high_resolution_clock::now();
        double encTime = std::chrono::duration<double>(encEnd - encStart).count();

        uint32_t originalBitsSize = input.size();
        uint32_t encodedBitsSize = bytestream.size() * 8;
        double compressionRatio = (static_cast<double>(originalBitsSize) / encodedBitsSize);
        double compression = (1.0 - (static_cast<double>(encodedBitsSize) / originalBitsSize)) * 100.0;



        std::cout << "Bins encoded: " + std::to_string(input.size()) << std::endl;
        std::cout << "Original size (bits): " + std::to_string(originalBitsSize) << std::endl;
        std::cout << "Encoded size (bits): " + std::to_string(encodedBitsSize) << std::endl;
        std::cout << "Compression ratio: " + std::to_string(compressionRatio)  << std::endl;
        std::cout << "Compression (%): " + std::to_string(compression) << std::endl;
        std::cout << "Final state: " << std::to_string(encoder.currentState) << std::endl;
        std::cout << "Encode time: " << std::to_string(encTime) << std::endl;
        printMemStats("Encode memory:", encMemStats);

        // Saving bitstreams
        if (g_useEncodedFile) {
            std::ofstream outputFile(g_encodedFile, std::ios::out | std::ios::trunc);
            if (!outputFile.is_open()) {
                std::cerr << "Error: Could not open or create the file!" << std::endl;
                return -1;
            }
            for (unsigned char value: bytestream) {
                std::bitset<8> num(value);
                outputFile << num;
            }
            std::cout << "Saved encoded (bitstream) file on: " + g_encodedFile + "." << std::endl;
            Logger::get().log(" Saved encoded (bitstream) file on: " + g_encodedFile + ".");
            outputFile.close();

            Logger::get().log("Finished encoding!");

            Logger::get().log("Stats:");
            Logger::get().log(" Adaptive: ");
            Logger::get().log("  Updates: " + std::to_string(Stats::get().updates));
            Logger::get().log("  Bitstream bits: " + std::to_string(Stats::get().bitstreamBits));
            Logger::get().log("  State bits: " + std::to_string(Stats::get().stateBits));
            Logger::get().log("  Table bits: " + std::to_string(Stats::get().tableBits));
            Logger::get().log("  Adaptive (state + tables) bits: " + std::to_string(Stats::get().adaptiveBits));
            Logger::get().log("  Misc bits (final state, final table, offset, etc...): " + std::to_string(Stats::get().miscBits));
        }


    }

    context.clear();

    if (g_doDecode && !g_doEncode) {
        std::ifstream file(g_bitstreamFile);
        if (!file.is_open()) {
            std::cerr << "Fail open input file: " + g_inputFile;
            return -1;
        }
        std::string s;
        char ch;
        while (file.get(ch)) {
            auto value = static_cast<uint8_t>(ch);
            bytestream.push_back(value);
        }
        file.close();
    }

    if (g_doDecode) {
        std::cout << "=== DECODING SUMMARY ===" << std::endl;
        ANSDecoder decoder(&context, std::move(bytestream));

        std::cout << "Initial state (final state of encode): " << std::to_string(decoder.currentState) << std::endl;
        PeakMemorySampler decSampler;
        size_t baselineDecMem = getCurrentRSS();
        decSampler.start();
        auto decStart = std::chrono::high_resolution_clock::now();
        uint32_t size = decoder.decodeBins(32);
        std::cout << "Bins to decode: " << std::to_string(size) << std::endl;
        std::vector<uint8_t> decoded(size);
        for (uint32_t i = 0; i < size; i++) {
            decoded[i] = decoder.decodeBin();
        }
        MemoryStats decMemStats = decSampler.stop(baselineDecMem);
        auto decEnd = std::chrono::high_resolution_clock::now();
        double decTime = std::chrono::duration<double>(decEnd - decStart).count();
        std::cout << "Decoded size: " << std::to_string(decoded.size()) << std::endl;
        std::cout << "Decode time: " + std::to_string(decTime) << std::endl;
        printMemStats("Decode memory:", decMemStats);

        if (g_useDecodedFile) {
            std::ofstream outputFile(g_decodedFile, std::ios::out | std::ios::trunc);
            if (!outputFile.is_open()) {
                std::cerr << "Error: Could not open or create the file!" << std::endl;
                return -1;
            }
            for (uint8_t bit : decoded ) {
                std::bitset<1> num(bit);
                outputFile << num;
            }
            std::cout << "Saved decoded (bitstream) file on: " + g_decodedFile + "." << std::endl;
            outputFile.close();
        }

        std::cout << "=== CHECKING DATA ===" << std::endl;
        if (!input.empty()) {
            bool eq = true;
            std::string reason;
            if (input.size() != decoded.size()) {
                eq = false;
                reason = "input size not equal decoded size";
            }else {
                for (int i = 0; i < input.size(); i++) {
                    if (input[i] != decoded[i]) {
                        eq = false;
                        reason = "vales on idx " + std::to_string(i) + " not equal";
                        break;
                    }
                }
            }
            if (eq) {
                std::cout << "Values of input it's equal of the decoded. Coder work :)" << std::endl;
            } else {
                std::cout << "Values of input it's not equal of the decoded: " << reason << "." << std::endl;
            }
        } else {
            std::cout << "Skipping check input and decode, need's run encode also." << std::endl;
        }
    }

    Logger::get().close();

    return 0;
}
