"""
GUI wrapper for the C++ Huffman file/folder compressor.

This file contains ZERO compression logic. It only:
  1. Lets the user pick a file/folder (and where to save the result) via
     native dialogs,
  2. Runs the compiled `compressor` C++ executable as a subprocess,
  3. Shows progress while it runs, and displays a clean result dialog
     (or the raw log, for transparency/debugging) afterwards.

Run with:  python3 gui/compressor_gui.py
(Requires the C++ binary to already be built via `make` in the project root.)
"""

import os
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

# ---------------------------------------------------------------------------
# Locate the compiled binary (built via the project's Makefile).
# ---------------------------------------------------------------------------
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY_NAME = "compressor.exe" if sys.platform.startswith("win") else "compressor"
BINARY_PATH = os.path.join(PROJECT_ROOT, BINARY_NAME)

# ---------------------------------------------------------------------------
# Theme
# ---------------------------------------------------------------------------
BG = "#f4f5fa"          # app background
CARD_BG = "#ffffff"     # section cards
ACCENT = "#4f46e5"      # indigo -- primary buttons
ACCENT_DARK = "#4338ca"
SUCCESS = "#16a34a"
ERROR = "#dc2626"
TEXT_DARK = "#1f2933"
TEXT_MUTED = "#6b7280"
FONT_FAMILY = "Segoe UI" if sys.platform.startswith("win") else "Helvetica"


def human_size(num_bytes):
    """Format a byte count as a short human-readable string (e.g. '12.4 KB')."""
    try:
        n = float(num_bytes)
    except (TypeError, ValueError):
        return str(num_bytes)
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} GB"


def parse_stats(output):
    """Pulls the known 'Key: value' lines out of the CLI's stdout into a dict.
    Returns {} if nothing recognizable was found (e.g. on error)."""
    stats = {}
    for line in output.splitlines():
        for key in ("Original size", "Compressed size", "Space saved",
                    "Written to", "File restored to", "Folder restored to"):
            prefix = key + ":"
            if line.strip().startswith(prefix):
                stats[key] = line.split(":", 1)[1].strip()
    return stats


class ResultDialog(tk.Toplevel):
    """A small, styled popup summarizing what just happened, instead of
    dumping raw CLI text on the user."""

    def __init__(self, parent, success, action, stats, raw_output):
        super().__init__(parent)
        self.configure(bg=CARD_BG)
        self.title("Success" if success else "Error")
        self.resizable(False, False)
        self.transient(parent)
        self.grab_set()

        icon = "\u2705" if success else "\u26a0\ufe0f"
        color = SUCCESS if success else ERROR
        headline = {
            ("compress", True): "Compression complete",
            ("compress", False): "Compression failed",
            ("decompress", True): "Decompression complete",
            ("decompress", False): "Decompression failed",
        }[(action, success)]

        pad = tk.Frame(self, bg=CARD_BG)
        pad.pack(padx=24, pady=20)

        tk.Label(pad, text=icon, font=(FONT_FAMILY, 28), bg=CARD_BG).pack()
        tk.Label(pad, text=headline, font=(FONT_FAMILY, 13, "bold"),
                 fg=color, bg=CARD_BG).pack(pady=(6, 14))

        if success and action == "compress" and "Original size" in stats:
            self._stat_row(pad, "Original size", stats.get("Original size", "-"))
            self._stat_row(pad, "Compressed size", stats.get("Compressed size", "-"))
            self._stat_row(pad, "Space saved", stats.get("Space saved", "-"), bold=True)
            dest = stats.get("Written to")
            if dest:
                self._path_row(pad, dest)
        elif success:
            dest = stats.get("File restored to") or stats.get("Folder restored to")
            if dest:
                self._path_row(pad, dest)
        else:
            msg = raw_output.strip() or "Unknown error."
            tk.Label(pad, text=msg, font=(FONT_FAMILY, 9), fg=TEXT_MUTED,
                     bg=CARD_BG, wraplength=320, justify="left").pack(pady=(0, 10))

        self._details_box = None
        details = ttk.Button(pad, text="Show details", command=lambda: self._toggle_details(pad, raw_output))
        details.pack(pady=(4, 10))

        ttk.Button(pad, text="OK", command=self.destroy, style="Accent.TButton").pack()

        self.update_idletasks()
        x = parent.winfo_rootx() + (parent.winfo_width() // 2) - (self.winfo_width() // 2)
        y = parent.winfo_rooty() + (parent.winfo_height() // 2) - (self.winfo_height() // 2)
        self.geometry(f"+{max(x, 0)}+{max(y, 0)}")

    def _stat_row(self, parent, label, value, bold=False):
        row = tk.Frame(parent, bg=CARD_BG)
        row.pack(fill="x", pady=2)
        tk.Label(row, text=label, font=(FONT_FAMILY, 10), fg=TEXT_MUTED,
                 bg=CARD_BG, anchor="w", width=16).pack(side="left")
        tk.Label(row, text=value, font=(FONT_FAMILY, 10, "bold" if bold else "normal"),
                 fg=SUCCESS if bold else TEXT_DARK, bg=CARD_BG, anchor="w").pack(side="left")

    def _path_row(self, parent, path):
        tk.Label(parent, text=path, font=(FONT_FAMILY, 9), fg=TEXT_MUTED,
                 bg=CARD_BG, wraplength=320, justify="left").pack(pady=(6, 4))

    def _toggle_details(self, parent, raw_output):
        if self._details_box is not None:
            self._details_box.destroy()
            self._details_box = None
            return
        box = scrolledtext.ScrolledText(parent, height=6, width=44, font=("Courier", 9))
        box.insert("1.0", raw_output)
        box.configure(state="disabled")
        box.pack(pady=(0, 10))
        self._details_box = box


class CompressorGUI:
    def __init__(self, root):
        self.root = root
        root.title("File/Folder Compression Tool (Huffman Coding)")
        root.geometry("600x480")
        root.resizable(False, False)
        root.configure(bg=BG)

        self._setup_style()

        # --- Header ---
        header = tk.Frame(root, bg=BG)
        header.pack(fill="x", pady=(22, 10))
        tk.Label(header, text="File / Folder Compression Tool", font=(FONT_FAMILY, 17, "bold"),
                 fg=TEXT_DARK, bg=BG).pack()
        tk.Label(header, text="Huffman coding \u00b7 C++ core \u00b7 Python GUI", font=(FONT_FAMILY, 9),
                 fg=TEXT_MUTED, bg=BG).pack(pady=(2, 0))

        # --- Compress card ---
        compress_card = self._card(root, "COMPRESS")
        btn_row = tk.Frame(compress_card, bg=CARD_BG)
        btn_row.pack(pady=(4, 2))
        self._big_button(btn_row, "Compress a File", self.compress_file).pack(side="left", padx=6)
        self._big_button(btn_row, "Compress a Folder", self.compress_folder).pack(side="left", padx=6)

        # --- Decompress card ---
        decompress_card = self._card(root, "DECOMPRESS")
        self._big_button(decompress_card, "Decompress a .huff File", self.decompress,
                          wide=True).pack(pady=(4, 2))

        # --- Progress bar (hidden until a job runs) ---
        self.progress = ttk.Progressbar(root, mode="indeterminate", length=520)

        # --- Status / log card ---
        log_card = self._card(root, "ACTIVITY LOG")
        self.log = scrolledtext.ScrolledText(log_card, height=8, width=68, state="disabled",
                                              font=("Consolas" if sys.platform.startswith("win") else "Courier", 9),
                                              bg="#fafafa", relief="flat", borderwidth=0)
        self.log.pack(fill="both", expand=True, pady=(2, 0))

        if not os.path.isfile(BINARY_PATH):
            self._log(f"Warning: binary not found at {BINARY_PATH}\nRun 'make' in the project root first.\n")

    # -----------------------------------------------------------------
    # Styling helpers
    # -----------------------------------------------------------------
    def _setup_style(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Accent.TButton", background=ACCENT, foreground="white",
                         font=(FONT_FAMILY, 10, "bold"), padding=(14, 8), borderwidth=0)
        style.map("Accent.TButton", background=[("active", ACCENT_DARK)])
        style.configure("TButton", font=(FONT_FAMILY, 9), padding=(8, 4))
        style.configure("TProgressbar", troughcolor=BG, background=ACCENT, thickness=6)

    def _card(self, parent, title):
        outer = tk.Frame(parent, bg=BG)
        outer.pack(fill="x", padx=22, pady=6)
        tk.Label(outer, text=title, font=(FONT_FAMILY, 8, "bold"), fg=TEXT_MUTED, bg=BG).pack(anchor="w")
        card = tk.Frame(outer, bg=CARD_BG, highlightbackground="#e5e7eb", highlightthickness=1)
        card.pack(fill="x", pady=(4, 0))
        inner = tk.Frame(card, bg=CARD_BG, padx=14, pady=10)
        inner.pack(fill="both", expand=True)
        return inner

    def _big_button(self, parent, text, command, wide=False):
        return ttk.Button(parent, text=text, command=command, style="Accent.TButton",
                           width=34 if wide else 22)

    # -----------------------------------------------------------------
    # UI actions
    # -----------------------------------------------------------------
    def compress_file(self):
        input_path = filedialog.askopenfilename(title="Select a file to compress")
        if not input_path:
            return
        output_path = filedialog.asksaveasfilename(
            title="Save compressed file as", defaultextension=".huff",
            filetypes=[("Huffman archive", "*.huff")],
            initialfile=os.path.basename(input_path) + ".huff")
        if not output_path:
            return
        self._run_async("compress", ["compress", input_path, output_path])

    def compress_folder(self):
        input_path = filedialog.askdirectory(title="Select a folder to compress")
        if not input_path:
            return
        output_path = filedialog.asksaveasfilename(
            title="Save compressed folder as", defaultextension=".huff",
            filetypes=[("Huffman archive", "*.huff")],
            initialfile=os.path.basename(input_path.rstrip("/")) + ".huff")
        if not output_path:
            return
        self._run_async("compress", ["compress", input_path, output_path])

    def decompress(self):
        input_path = filedialog.askopenfilename(
            title="Select a .huff file to decompress",
            filetypes=[("Huffman archive", "*.huff"), ("All files", "*.*")])
        if not input_path:
            return

        header = self._peek_header(input_path)
        if header is None:
            messagebox.showerror("Error", "Not a valid .huff file.")
            return
        is_folder, original_name = header

        if is_folder:
            output_path = filedialog.askdirectory(title="Choose a folder to restore into", mustexist=False)
        else:
            # Pre-fill the exact original filename (with its real extension)
            # so the user never has to remember or retype it.
            output_path = filedialog.asksaveasfilename(
                title="Save decompressed file as",
                initialfile=original_name or "restored_file")
        if not output_path:
            return

        self._run_async("decompress", ["decompress", input_path, output_path])

    # -----------------------------------------------------------------
    # Helpers
    # -----------------------------------------------------------------
    def _peek_header(self, huff_path):
        """Reads just the header (magic + flag + originalSize + name) to
        learn whether the archive holds a single file or a packed folder,
        and what its original name/extension was -- without doing a full
        decompress. Mirrors the .huff format documented in README.md.
        Returns (is_folder, original_name) or None if invalid."""
        try:
            with open(huff_path, "rb") as f:
                magic = f.read(4)
                if magic != b"HUFC":
                    return None
                flag = f.read(1)[0]
                f.read(8)  # originalSize, not needed here
                name_len = int.from_bytes(f.read(2), "little")
                original_name = f.read(name_len).decode("utf-8", errors="replace")
            return (flag == 1, original_name)
        except (OSError, IndexError):
            return None

    def _run_async(self, action, args):
        """Runs the compressor binary in a background thread so the GUI
        doesn't freeze, shows a progress bar while it works, then reports
        the result back on the main thread as a styled dialog."""
        if not os.path.isfile(BINARY_PATH):
            messagebox.showerror("Error", f"Binary not found:\n{BINARY_PATH}\n\nRun 'make' first.")
            return

        self._log(f"$ {os.path.basename(BINARY_PATH)} {' '.join(args)}\n")
        self.progress.pack(padx=22, pady=(2, 6))
        self.progress.start(12)
        threading.Thread(target=self._run, args=(action, args), daemon=True).start()

    def _run(self, action, args):
        try:
            result = subprocess.run([BINARY_PATH] + args, capture_output=True, text=True)
            output = (result.stdout + result.stderr)
            success = result.returncode == 0
        except Exception as e:
            output = f"Failed to run binary: {e}\n"
            success = False
        self.root.after(0, self._finish, action, output, success)

    def _finish(self, action, output, success):
        self.progress.stop()
        self.progress.pack_forget()
        self._log(output + "\n")
        stats = parse_stats(output)
        ResultDialog(self.root, success, action, stats, output)

    def _log(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text)
        self.log.see("end")
        self.log.configure(state="disabled")


if __name__ == "__main__":
    root = tk.Tk()
    app = CompressorGUI(root)
    root.mainloop()
