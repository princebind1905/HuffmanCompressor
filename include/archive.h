#pragma once
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Archive module
//
// Huffman coding compresses a single stream of bytes, not a folder. So to
// support compressing a whole folder, we first flatten ("pack") every file
// under it into one byte buffer (path + size + content, repeated), similar
// in spirit to a .tar file. That flat buffer is then fed into huffman::compress
// like any other file. unpackFolder() reverses the process after decompression.
// ---------------------------------------------------------------------------

namespace archive {

// Recursively packs every regular file under folderPath into one flat buffer.
std::vector<unsigned char> packFolder(const std::string& folderPath);

// Reverses packFolder(): recreates the original folder structure under
// outputFolder from a buffer produced by packFolder().
void unpackFolder(const std::vector<unsigned char>& data, const std::string& outputFolder);

} // namespace archive
