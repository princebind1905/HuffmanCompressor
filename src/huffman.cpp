#include "huffman.h"
#include <queue>
#include <vector>
#include <string>

namespace huffman {

// ---------------------------------------------------------------------------
// Bit-level I/O helpers
// Huffman codes are variable-length bit strings ("0101101...") -- these two
// small classes pack/unpack those bits into actual bytes.
// ---------------------------------------------------------------------------

class BitWriter {
public:
    std::vector<unsigned char> buffer;
    unsigned char current = 0;
    int bitCount = 0;

    void writeBit(int bit) {
        current = (unsigned char)((current << 1) | (bit & 1));
        bitCount++;
        if (bitCount == 8) {
            buffer.push_back(current);
            current = 0;
            bitCount = 0;
        }
    }

    void writeCode(const std::string& code) {
        for (char c : code) writeBit(c - '0');
    }

    // Flush any leftover bits, zero-padding the final byte.
    void flush() {
        if (bitCount > 0) {
            current = (unsigned char)(current << (8 - bitCount));
            buffer.push_back(current);
            current = 0;
            bitCount = 0;
        }
    }
};

class BitReader {
public:
    const std::vector<unsigned char>& buffer;
    size_t bytePos = 0;
    int bitPos = 0;

    explicit BitReader(const std::vector<unsigned char>& b) : buffer(b) {}

    // Returns 0 or 1, or -1 once the buffer is exhausted.
    int readBit() {
        if (bytePos >= buffer.size()) return -1;
        int bit = (buffer[bytePos] >> (7 - bitPos)) & 1;
        bitPos++;
        if (bitPos == 8) { bitPos = 0; bytePos++; }
        return bit;
    }
};

// ---------------------------------------------------------------------------
// Tree construction (the actual "DSA" part: greedy + min-heap)
// ---------------------------------------------------------------------------

// Min-heap comparator: lower frequency = higher priority.
// Ties are broken by insertion "order" (not frequency), which guarantees
// the exact same tree shape is rebuilt on both the compress and decompress
// side, as long as both start from the same frequency table.
struct Compare {
    bool operator()(const std::pair<Node*, long long>& a,
                     const std::pair<Node*, long long>& b) const {
        if (a.first->freq != b.first->freq) return a.first->freq > b.first->freq;
        return a.second > b.second;
    }
};

static Node* buildTree(const std::unordered_map<unsigned char, long long>& freqTable) {
    std::priority_queue<std::pair<Node*, long long>,
                         std::vector<std::pair<Node*, long long>>,
                         Compare> pq;

    long long order = 0;
    // Walk symbols 0..255 in a *fixed* order (instead of iterating the
    // unordered_map directly) so compress() and decompress() -- which use
    // two different map instances holding the same data -- always build
    // an identical tree.
    for (int sym = 0; sym < 256; sym++) {
        auto it = freqTable.find((unsigned char)sym);
        if (it != freqTable.end() && it->second > 0) {
            pq.push({new Node((unsigned char)sym, it->second), order++});
        }
    }

    if (pq.empty()) return nullptr;

    // Classic Huffman merge step: repeatedly combine the two least-frequent
    // nodes into a new parent, until only the root remains.
    while (pq.size() > 1) {
        auto a = pq.top(); pq.pop();
        auto b = pq.top(); pq.pop();
        Node* parent = new Node(0, a.first->freq + b.first->freq, a.first, b.first);
        pq.push({parent, order++});
    }
    return pq.top().first;
}

// Walk the tree to assign each symbol a binary code (left = '0', right = '1').
static void generateCodes(Node* node, const std::string& prefix,
                           std::unordered_map<unsigned char, std::string>& codes) {
    if (!node) return;
    if (node->isLeaf()) {
        // Edge case: only one distinct symbol in the whole input -> the
        // "tree" is a single leaf with no left/right edges, so prefix is
        // empty. We still need at least 1 bit per symbol to encode it.
        codes[node->ch] = prefix.empty() ? "0" : prefix;
        return;
    }
    generateCodes(node->left, prefix + "0", codes);
    generateCodes(node->right, prefix + "1", codes);
}

static void freeTree(Node* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<unsigned char> compress(const std::vector<unsigned char>& data,
                                     std::unordered_map<unsigned char, long long>& freqTable) {
    freqTable.clear();
    for (unsigned char b : data) freqTable[b]++;

    if (data.empty()) return {};

    Node* root = buildTree(freqTable);
    std::unordered_map<unsigned char, std::string> codes;
    generateCodes(root, "", codes);

    BitWriter writer;
    for (unsigned char b : data) writer.writeCode(codes[b]);
    writer.flush();

    freeTree(root);
    return writer.buffer;
}

std::vector<unsigned char> decompress(const std::vector<unsigned char>& compressed,
                                       const std::unordered_map<unsigned char, long long>& freqTable,
                                       long long originalSize) {
    std::vector<unsigned char> output;
    if (originalSize == 0 || freqTable.empty()) return output;
    output.reserve((size_t)originalSize);

    Node* root = buildTree(freqTable);

    // Single distinct symbol: no real tree to walk, just repeat it.
    if (root->isLeaf()) {
        for (long long i = 0; i < originalSize; i++) output.push_back(root->ch);
        freeTree(root);
        return output;
    }

    BitReader reader(compressed);
    Node* node = root;
    while ((long long)output.size() < originalSize) {
        int bit = reader.readBit();
        if (bit == -1) break; // safety guard against malformed input
        node = (bit == 0) ? node->left : node->right;
        if (node->isLeaf()) {
            output.push_back(node->ch);
            node = root;
        }
    }

    freeTree(root);
    return output;
}

} // namespace huffman
