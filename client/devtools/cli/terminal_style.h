#pragma once
#include <string>
#include <sstream>

namespace term {

// ANSI codes (raw bytes to avoid format string issues)
inline const char* RST  = "\x1b[0m";
inline const char* BOLD = "\x1b[1m";
inline const char* RED  = "\x1b[31m";
inline const char* GRN  = "\x1b[32m";
inline const char* YEL  = "\x1b[33m";
inline const char* CYN  = "\x1b[36m";
inline const char* GRY  = "\x1b[90m";

// Box characters
inline const std::string HLINE(int w) { return std::string(w, 0xC4); } // ─

inline std::string box_header(const std::string& title, int w = 56) {
    std::string h(w, 0xC4);
    int pad = (w - (int)title.size()) / 2;
    std::string s;
    s += GRY; s += 0xDA; s += h; s += 0xBF; s += "\n";       // ┌───┐
    s += (char)0xB3;                                          // │
    s += std::string(pad, ' ') + BOLD + title + RST + GRY;
    s += std::string(w - pad - (int)title.size(), ' ') + (char)0xB3 + "\n"; // │
    s += (char)0xC0; s += h; s += (char)0xD9; s += RST;       // └───┘
    return s;
}

inline std::string prompt() {
    return std::string(BOLD) + CYN + "chrono" + RST
         + GRY + " " + "\xe2\x86\x92" + " " + RST;  // →
}

inline std::string tag_ok()   { return std::string(GRN) + "[OK]" + RST; }
inline std::string tag_info() { return std::string(CYN) + "[*]"  + RST; }
inline std::string tag_warn() { return std::string(YEL) + "[!]"  + RST; }
inline std::string tag_err()  { return std::string(RED) + "[-]"  + RST; }

} // namespace term
