#include "archive.h"
#include <filesystem>
#include <fstream>
#include <cstdint>

namespace fs = std::filesystem;

namespace archive {

// Entry layout in the flat buffer:
//   [4 bytes: path length][path bytes][4 bytes: file size][file bytes]
// repeated for every file, back to back, with no end marker needed --
// we know exactly how many total bytes there are once decompressed.

static void appendUint32(std::vector<unsigned char>& buf, uint32_t v) {
    for (int i = 0; i < 4; i++) buf.push_back((unsigned char)((v >> (8 * i)) & 0xFF));
}

static uint32_t readUint32(const std::vector<unsigned char>& buf, size_t& pos) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= ((uint32_t)buf[pos + i]) << (8 * i);
    pos += 4;
    return v;
}

std::vector<unsigned char> packFolder(const std::string& folderPath) {
    std::vector<unsigned char> out;
    fs::path root(folderPath);

    for (auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;

        std::string relPath = fs::relative(entry.path(), root).generic_string();

        std::ifstream in(entry.path(), std::ios::binary);
        std::vector<unsigned char> content((std::istreambuf_iterator<char>(in)),
                                            std::istreambuf_iterator<char>());

        appendUint32(out, (uint32_t)relPath.size());
        out.insert(out.end(), relPath.begin(), relPath.end());
        appendUint32(out, (uint32_t)content.size());
        out.insert(out.end(), content.begin(), content.end());
    }
    return out;
}

void unpackFolder(const std::vector<unsigned char>& data, const std::string& outputFolder) {
    fs::create_directories(outputFolder);
    size_t pos = 0;
    while (pos < data.size()) {
        uint32_t nameLen = readUint32(data, pos);
        std::string relPath(data.begin() + pos, data.begin() + pos + nameLen);
        pos += nameLen;

        uint32_t fileLen = readUint32(data, pos);
        std::vector<unsigned char> content(data.begin() + pos, data.begin() + pos + fileLen);
        pos += fileLen;

        fs::path outPath = fs::path(outputFolder) / relPath;
        fs::create_directories(outPath.parent_path());
        std::ofstream out(outPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(content.data()), (std::streamsize)content.size());
    }
}

} // namespace archive
