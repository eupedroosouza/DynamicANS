#include <bitset>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

#include "Common/Context.h"
#include "Dec/ANSDecoder.h"
#include "Enc/ANSEncoder.h"

static bool g_doEncode;
static bool g_doDecode;
static std::string g_tablesFile;
static std::string g_inputFile;
static std::string g_bitstreamFile;
static uint32_t g_adaptativeInterval;
static bool g_useOutputFile = false;
static std::string g_outputFile;

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

    if (args.count("--output")) {
        g_outputFile = args["--output"];
        g_useOutputFile = true;
    } else if (args.count("-o")) {
        g_outputFile = args["-o"];
        g_useOutputFile = true;
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

    Context context = Context::loadContextFromFile(g_tablesFile, g_adaptativeInterval);

    std::vector<uint8_t> input;
    std::vector<uint8_t> bytestream;
    if (g_doEncode) {
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

        auto encStart = std::chrono::high_resolution_clock::now();

        // Inversed encode
        for (int i = static_cast<int>(input.size() - 1); i >= 0; i--) {
            encoder.encodeBin(input[i]);
        }

        encoder.encodeBins(input.size() , 32);

        bytestream = encoder.finishEncoding();

        auto encEnd = std::chrono::high_resolution_clock::now();
        double encTime = std::chrono::duration<double>(encEnd - encStart).count();

        uint32_t originalBitsSize = input.size();
        uint32_t encodedBitsSize = bytestream.size() * 8;
        double compressionRatio = (static_cast<double>(originalBitsSize) / encodedBitsSize);

        std::cout << "=== ENCODING SUMMARY ===" << std::endl;
        std::cout << "Bins encoded: " + std::to_string(input.size()) << std::endl;
        std::cout << "Original size (bits): " + std::to_string(originalBitsSize) << std::endl;
        std::cout << "Encoded size (bits): " + std::to_string(encodedBitsSize) << std::endl;
        char compressionRatioStr[255];
        snprintf(compressionRatioStr, sizeof(compressionRatioStr), "%.2f", compressionRatio);
        std::cout << "Compression ratio: " + std::to_string(compressionRatio) + " (" + compressionRatioStr + "%)" <<
                std::endl;
        std::cout << "Final state: " << std::to_string(encoder.currentState) << std::endl;
        std::cout << "Encode time: " << std::to_string(encTime) << std::endl;

        // Saving bitstreams
        if (g_useOutputFile) {
            std::ofstream outputFile(g_outputFile, std::ios::out | std::ios::trunc);
            if (!outputFile.is_open()) {
                std::cerr << "Error: Could not open or create the file!" << std::endl;
                return -1;
            }
            for (unsigned char value: bytestream) {
                std::bitset<8> num(value);
                outputFile << num;
            }
            std::cout << "Saved encoded (bitstream) file on: " + g_outputFile + "." << std::endl;
            outputFile.close();
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
        auto decStart = std::chrono::high_resolution_clock::now();
        uint32_t size = decoder.decodeBins(32);
        std::vector<uint8_t> decoded(size);
        for (uint32_t i = 0; i < size; i++) {
            decoded[i] = decoder.decodeBin();
        }
        auto decEnd = std::chrono::high_resolution_clock::now();
        double decTime = std::chrono::duration<double>(decEnd - decStart).count();
        std::cout << "Bins to decode: " << std::to_string(size) << std::endl;
        std::cout << "Decoded size: " << std::to_string(decoded.size()) << std::endl;
        std::cout << "Decode time: " + std::to_string(decTime) << std::endl;

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

    return 0;
}
