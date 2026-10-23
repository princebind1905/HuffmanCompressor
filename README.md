# File/Folder Compression Tool (Huffman Coding) — C++

A command-line tool that compresses and decompresses a **single file or an
entire folder** using **Huffman coding**, written in C++17 with no external
libraries.

---

## Why this project (resume line)

> Built a lossless file/folder compression tool in C++ using Huffman coding
> (min-heap + greedy tree construction); achieves ~35–45% size reduction on
> text data; supports recursive folder compression via a custom flat-archive
> format.

---

## How it works

### 1. Huffman Coding (the core DSA idea)
1. Count the frequency of every byte (0–255) in the input.
2. Put each byte in a **min-heap** (`priority_queue`), keyed by frequency.
3. Repeatedly pop the two lowest-frequency nodes, merge them into a new
   internal node (`freq = left + right`), and push it back. This is the
   classic **greedy algorithm** — always merge the two cheapest options first.
4. After `n-1` merges, one node remains: the **Huffman tree** root.
5. Walk the tree: every left edge = `0`, every right edge = `1`. The path
   from root to a leaf is that byte's code. Frequent bytes end up near the
   root (short codes); rare bytes end up deep (long codes).
6. Replace every byte in the input with its code and pack the resulting bit
   string into bytes (`BitWriter`).

Decompression rebuilds the *identical* tree from the saved frequency table,
then walks it bit by bit, emitting a byte every time it reaches a leaf.

### 2. Folder support
Huffman coding compresses a single byte stream, not a directory tree. So a
folder is first **flattened into one buffer** (`archive::packFolder`):

```
[4B path length][path][4B file size][file bytes]   <- repeated per file
```

That flat buffer is then Huffman-compressed exactly like a normal file.
`archive::unpackFolder` reverses this after decompression, recreating every
file at its original relative path.

### 3. The `.huff` container format
```
"HUFC"        4 bytes   magic number
flag          1 byte    0 = single file, 1 = packed folder
originalSize  8 bytes   byte count before compression
nameLen       2 bytes   length of the original file/folder name
originalName  nameLen bytes   e.g. "report.pdf" or "my_folder"
tableSize     2 bytes   number of distinct symbols
table         tableSize × (1 byte symbol + 8 bytes frequency)
data          the Huffman-coded bitstream
```
The frequency table is small (at most 256 entries) and lets the decompressor
rebuild the exact same tree — no need to store the tree itself or the codes.

Storing the original name means decompression can restore the exact
filename and extension automatically: pass an existing folder as the
decompress output and the tool creates `<folder>/<originalName>` for you,
instead of requiring you to retype the filename and extension by hand.

---

## Project structure
```
file-compressor/
├── include/
│   ├── huffman.h      Huffman API (compress/decompress)
│   └── archive.h       Folder pack/unpack API
├── src/
│   ├── huffman.cpp     Tree building, bit I/O, encode/decode
│   ├── archive.cpp     Recursive folder <-> flat buffer conversion
│   └── main.cpp        CLI + .huff file format read/write
├── gui/
│   └── compressor_gui.py   Optional Tkinter GUI wrapper (see below)
├── Makefile
└── README.md
```

## Build
```bash
make
```
Produces a `compressor` executable. Requires a C++17 compiler (uses
`<filesystem>`).

## Usage
```bash
# Compress a single file
./compressor compress notes.txt notes.huff

# Decompress it back (explicit output filename)
./compressor decompress notes.huff notes_restored.txt

# Or point at an existing folder and let it restore the original name/extension automatically
./compressor decompress notes.huff ./some_output_folder

# Compress an entire folder (recursively)
./compressor compress my_folder my_folder.huff

# Decompress back into a folder
./compressor decompress my_folder.huff my_folder_restored
```
Each run prints original size, compressed size, and % space saved.

## Optional GUI

A small Tkinter GUI (`gui/compressor_gui.py`) wraps the CLI with a themed
interface — card-style sections, an indigo accent color, a progress bar
while a job runs, and a clean result popup (checkmark + stats, or a warning
+ error message) instead of raw terminal text. It contains **no compression
logic of its own** — it only launches the compiled `compressor` binary as a
subprocess and displays its output. This keeps 100% of the DSA
implementation in C++, which is what matters for the interview; the GUI is
just a thin, styled convenience layer on top.

```bash
make                       # build the C++ binary first
python3 gui/compressor_gui.py
```

Requires Python 3 with Tkinter (included by default on most systems; on
some minimal Linux installs, `sudo apt install python3-tk`).

The GUI offers:
- **Compress a File** / **Compress a Folder** — pick input, then choose
  where to save the resulting `.huff` file.
- **Decompress a .huff File** — pick a `.huff` file; the GUI peeks at its
  header (magic bytes + flag + stored original name) to detect whether it
  holds a file or a folder, then pre-fills the save dialog with the exact
  original filename and extension automatically.
- A result dialog after every run: a green checkmark with original size /
  compressed size / space-saved stats for compression, a restored-path
  confirmation for decompression, or a red warning with the actual error
  message if something went wrong. A "Show details" toggle reveals the raw
  CLI output underneath, for anyone who wants to see exactly what ran.
- An activity log at the bottom keeps a running history of every command
  executed and its raw output, for transparency/debugging.

---

## Complexity
- Let `n` = number of bytes in input, `k` = number of distinct byte values
  (≤ 256).
- Building the frequency table: `O(n)`
- Building the Huffman tree: `O(k log k)` (heap push/pop for each merge)
- Encoding: `O(n)` (each byte's code is looked up and written once)
- Decoding: `O(n)` (one tree walk per output byte, tree depth ≤ `k`)
- **Overall: O(n + k log k)**, i.e. effectively linear in file size.

## Interview talking points
- Why a **min-heap**? We always need the two lowest-frequency nodes next —
  a heap gives `O(log k)` extract-min instead of `O(k)` for a linear scan.
- Why is Huffman **optimal**? It's a textbook greedy-exchange proof: any
  optimal prefix code can be transformed into one where the two
  least-frequent symbols are siblings at the deepest level, which is exactly
  what the algorithm builds.
- Why store a **frequency table** instead of the codes themselves? The table
  is smaller (≤256×9 bytes) and is enough to deterministically rebuild the
  same tree — I made tree construction order-independent by iterating
  symbols 0–255 in a fixed order, so both sides reconstruct identical trees.
- Edge cases handled: empty file, file with only one distinct byte (needs a
  1-bit code even though there's no real branching), binary (non-text)
  files, nested folders, empty sub-files.
- Limitation to mention if asked: this is a simple **flat/uncompressed
  archive + Huffman** design, not a true general-purpose compressor like
  DEFLATE (which also does LZ77 dictionary matching for repeated
  substrings) — a natural "next step" to discuss if pushed further.
