#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ---------------------------------------------------------------------------
// Huffman Coding module
//
// Core DSA idea: build an optimal prefix-code binary tree using a min-heap
// (greedy algorithm). Frequent symbols get short codes, rare symbols get
// longer codes, so the total encoded length is minimized (this is the
// classic Huffman/greedy-exchange argument).
// ---------------------------------------------------------------------------

namespace huffman {

// A node in the Huffman tree.
struct Node {
    unsigned char ch;   // symbol (only meaningful for leaf nodes)
    long long freq;      // frequency / weight of this subtree
    Node* left;
    Node* right;

    Node(unsigned char c, long long f, Node* l = nullptr, Node* r = nullptr)
        : ch(c), freq(f), left(l), right(r) {}

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

// Compress a buffer of raw bytes using Huffman coding.
// Fills `freqTable` with the symbol frequencies of `data` -- this table
// must be saved alongside the compressed output, because decompress()
// needs it to rebuild the *exact same* tree.
std::vector<unsigned char> compress(const std::vector<unsigned char>& data,
                                     std::unordered_map<unsigned char, long long>& freqTable);

// Decompress a bitstream previously produced by compress().
// `originalSize` is the number of symbols (bytes) in the original data,
// used to know when to stop (since the last byte of the bitstream is
// zero-padded and would otherwise be ambiguous).
std::vector<unsigned char> decompress(const std::vector<unsigned char>& compressed,
                                       const std::unordered_map<unsigned char, long long>& freqTable,
                                       long long originalSize);

} // namespace huffman
