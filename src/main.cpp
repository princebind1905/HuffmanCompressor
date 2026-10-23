#include "huffman.h"
#include "archive.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <stdexcept>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// .huff file layout:
//   "HUFC"        4 bytes  magic number
//   flag          1 byte   0 = single file, 1 = packed folder
//   originalSize  8 bytes  number of bytes before compression
//   nameLen       2 bytes  length of the original file/folder name below
//   originalName  nameLen bytes  original base name (e.g. "report.pdf"),
//                                so decompress can restore the same name
//                                and extension without the user retyping it
//   tableSize     2 bytes  number of distinct symbols
//   table entries  tableSize * (1 byte symbol + 8 bytes frequency)
//   compressed data  the Huffman-coded bitstream (rest of the file)
// ---------------------------------------------------------------------------

static void writeUint64(std::ofstream& out, uint64_t v) {
    for (int i = 0; i < 8; i++) out.put((char)((v >> (8 * i)) & 0xFF));
}
static uint64_t readUint64(std::ifstream& in) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        unsigned char b = (unsigned char)in.get();
        v |= ((uint64_t)b) << (8 * i);
    }
    return v;
}
static void writeUint16(std::ofstream& out, uint16_t v) {
    out.put((char)(v & 0xFF));
    out.put((char)((v >> 8) & 0xFF));
}
static uint16_t readUint16(std::ifstream& in) {
    unsigned char b0 = (unsigned char)in.get();
    unsigned char b1 = (unsigned char)in.get();
    return (uint16_t)(b0 | (b1 << 8));
}

static std::vector<unsigned char> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + path);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
}

static void writeCompressedFile(const std::string& outPath, unsigned char flag,
                                 size_t originalSize, const std::string& originalName,
                                 const std::vector<unsigned char>& compressedData,
                                 const std::unordered_map<unsigned char, long long>& freqTable) {
    std::ofstream out(outPath, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output: " + outPath);

    out.write("HUFC", 4);
    out.put((char)flag);
    writeUint64(out, (uint64_t)originalSize);
    writeUint16(out, (uint16_t)originalName.size());
    out.write(originalName.data(), (std::streamsize)originalName.size());
    writeUint16(out, (uint16_t)freqTable.size());
    for (auto& kv : freqTable) {
        out.put((char)kv.first);
        writeUint64(out, (uint64_t)kv.second);
    }
    out.write(reinterpret_cast<const char*>(compressedData.data()),
              (std::streamsize)compressedData.size());
}

struct CompressedFile {
    unsigned char flag;
    uint64_t originalSize;
    std::string originalName;
    std::unordered_map<unsigned char, long long> freqTable;
    std::vector<unsigned char> compressedData;
};

static CompressedFile readCompressedFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + path);

    char magic[4];
    in.read(magic, 4);
    if (std::strncmp(magic, "HUFC", 4) != 0)
        throw std::runtime_error("Not a valid .huff file (bad magic number)");

    CompressedFile cf;
    cf.flag = (unsigned char)in.get();
    cf.originalSize = readUint64(in);
    uint16_t nameLen = readUint16(in);
    cf.originalName.resize(nameLen);
    if (nameLen > 0) in.read(&cf.originalName[0], nameLen);
    uint16_t tableSize = readUint16(in);
    for (int i = 0; i < tableSize; i++) {
        unsigned char sym = (unsigned char)in.get();
        long long freq = (long long)readUint64(in);
        cf.freqTable[sym] = freq;
    }
    cf.compressedData = std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                                     std::istreambuf_iterator<char>());
    return cf;
}

static void printUsage() {
    std::cout << "File/Folder Compression Tool (Huffman Coding)\n\n"
              << "Usage:\n"
              << "  compressor compress   <input file or folder> <output .huff file>\n"
              << "  compressor decompress <input .huff file> <output file or folder>\n";
}

int main(int argc, char** argv) {
    if (argc != 4) { printUsage(); return 1; }

    std::string mode = argv[1];
    std::string inPath = argv[2];
    std::string outPath = argv[3];

    try {
        if (mode == "compress") {
            std::vector<unsigned char> raw;
            unsigned char flag;

            // Base name only (no directory path) -- e.g. "report.pdf" or
            // "my_folder" -- saved so decompress can restore it automatically.
            std::string baseName = fs::path(inPath).filename().string();

            if (fs::is_directory(inPath)) {
                raw = archive::packFolder(inPath);   // flatten folder -> bytes
                flag = 1;
            } else {
                raw = readFile(inPath);
                flag = 0;
            }

            std::unordered_map<unsigned char, long long> freqTable;
            std::vector<unsigned char> compressed = huffman::compress(raw, freqTable);
            writeCompressedFile(outPath, flag, raw.size(), baseName, compressed, freqTable);

            std::cout << "Original size:   " << raw.size() << " bytes\n";
            std::cout << "Compressed size: " << compressed.size() << " bytes\n";
            if (!raw.empty()) {
                double ratio = 100.0 * (1.0 - (double)compressed.size() / (double)raw.size());
                std::cout << "Space saved:     " << ratio << "%\n";
            }
            std::cout << "Written to:      " << outPath << "\n";

        } else if (mode == "decompress") {
            CompressedFile cf = readCompressedFile(inPath);
            std::vector<unsigned char> raw =
                huffman::decompress(cf.compressedData, cf.freqTable, (long long)cf.originalSize);

            if (cf.flag == 1) {
                archive::unpackFolder(raw, outPath);   // rebuild folder structure
                std::cout << "Folder restored to: " << outPath << "\n";
            } else {
                // If the user pointed at an existing folder instead of a
                // specific filename, restore using the original stored name
                // (with its original extension) instead of requiring it
                // to be typed out again.
                std::string finalPath = outPath;
                if (fs::exists(outPath) && fs::is_directory(outPath) && !cf.originalName.empty()) {
                    finalPath = (fs::path(outPath) / cf.originalName).string();
                }

                std::ofstream out(finalPath, std::ios::binary);
                if (!out) throw std::runtime_error("Cannot open output: " + finalPath);
                out.write(reinterpret_cast<const char*>(raw.data()), (std::streamsize)raw.size());
                std::cout << "File restored to: " << finalPath << "\n";
            }
        } else {
            printUsage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
