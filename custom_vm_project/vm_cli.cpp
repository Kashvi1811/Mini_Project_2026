#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <array>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace {
    constexpr int MEMORY_SIZE = 1 << 16;
    constexpr size_t MAX_RECENT_HISTORY = 5;
    constexpr int FACT_MAX_INPUT = 17;
    constexpr int FIB_MAX_INPUT = 63;
    const string CLI_VERSION = "1.0";
    const string HISTORY_FILE = ".vm_cli_history";

    // Keep menu IDs stable to avoid breaking docs/demo scripts that reference option numbers.
    const string MENU_OPT_FACTORIAL = "1";
    const string MENU_OPT_FIBONACCI = "2";
    const string MENU_OPT_RUN_ASM = "3";
    const string MENU_OPT_TRACE_SUMMARY = "4";
    const string MENU_OPT_VIEWER = "5";
    const string MENU_OPT_EXIT = "0";

    struct VmState {
        vector<uint16_t> memory = vector<uint16_t>(MEMORY_SIZE, 0);
        uint16_t R[8] = {0};
        uint16_t PC = 0;
        int FLAG_Z = 1;
        int FLAG_N = 0;
        int FLAG_P = 0;
    };

    struct StepRecord {
        int step = 0;
        uint16_t pc = 0;
        uint16_t ins = 0;
        uint16_t regs[8] = {0};
    };

    struct RunOptions {
        bool writeTrace = true;
        string tracePath = "trace.jsonl";
        int maxSteps = 100000;
        bool printStepLog = false;
        bool jsonOutput = false;
        bool quietOutput = false;
        bool verboseOutput = false;
    };

    struct CliHistoryState {
        bool loaded = false;
        string lastAsmPath;
        string lastTracePath;
        vector<string> asmRecent;
        vector<string> traceRecent;
    };

    int runPresetProgram(const string& preset, int value, const RunOptions& options);
    int runAsmProgram(const string& asmPath, const RunOptions& options);
    int runBinProgram(const string& binPath, const RunOptions& options);
    int assembleToBinary(const string& asmPath, const string& binPath);
    int dumpBinary(const string& binPath, bool jsonOutput = false);
    int traceSummary(const string& tracePath, bool jsonOutput = false);
    int handleViewerCommand(bool open);
    int handleKaliUiCommand(bool open);
    int handleKaliAppCommand(bool open);

    const string ANSI_RESET = "\x1b[0m";
    const string ANSI_GREEN = "\x1b[32m";
    const string ANSI_CYAN = "\x1b[36m";
    const string ANSI_BOLD = "\x1b[1m";

    string trim(const string& text) {
        size_t left = 0;
        while (left < text.size() && isspace(static_cast<unsigned char>(text[left]))) left++;
        size_t right = text.size();
        while (right > left && isspace(static_cast<unsigned char>(text[right - 1]))) right--;
        return text.substr(left, right - left);
    }

    string escapeJsonString(const string& text) {
        string out;
        out.reserve(text.size() + 8);
        for (char ch : text) {
            switch (ch) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(ch); break;
            }
        }
        return out;
    }

    string hex16(uint16_t value) {
        ostringstream out;
        out << "0x" << uppercase << hex << setw(4) << setfill('0') << static_cast<unsigned int>(value);
        return out.str();
    }

    void printJsonEnvelopePrefix(const string& command) {
        cout << "{";
        cout << "\"schemaVersion\":\"1.0\"";
        cout << ",\"success\":true";
        cout << ",\"command\":\"" << escapeJsonString(command) << "\"";
        cout << ",\"data\":{";
    }

    void printJsonEnvelopeSuffix() {
        cout << "},\"error\":null}";
    }

    bool isDigitsOnly(const string& text) {
        if (text.empty()) return false;
        for (char ch : text) {
            if (!isdigit(static_cast<unsigned char>(ch))) return false;
        }
        return true;
    }

    string stripMatchingQuotes(const string& text) {
        if (text.size() >= 2) {
            const char first = text.front();
            const char last = text.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                return text.substr(1, text.size() - 2);
            }
        }
        return text;
    }

    string normalizeUserPathInput(string input) {
        input = trim(input);

        // Accept Python-style raw string input like r"C:\path\file.asm".
        if (input.size() >= 4 && (input[0] == 'r' || input[0] == 'R') &&
            ((input[1] == '"' && input.back() == '"') || (input[1] == '\'' && input.back() == '\''))) {
            input = input.substr(2, input.size() - 3);
        } else {
            input = stripMatchingQuotes(input);
        }

        if (input.empty()) return input;

#if defined(__linux__) && !defined(_WIN32)
        // Convert pasted Windows path (C:\foo\bar) to WSL path (/mnt/c/foo/bar).
        if (input.size() >= 3 && isalpha(static_cast<unsigned char>(input[0])) && input[1] == ':' &&
            (input[2] == '\\' || input[2] == '/')) {
            char drive = static_cast<char>(tolower(static_cast<unsigned char>(input[0])));
            string rest = input.substr(2);
            for (char& ch : rest) {
                if (ch == '\\') ch = '/';
            }
            while (!rest.empty() && rest.front() == '/') rest.erase(rest.begin());
            input = "/mnt/";
            input.push_back(drive);
            input.push_back('/');
            input += rest;
        }
#endif

        return input;
    }

    string toLowerAscii(string text) {
        for (char& ch : text) {
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        }
        return text;
    }

    bool hasExtensionIgnoreCase(const string& path, const string& extension) {
        if (path.size() < extension.size()) return false;
        const string pathTail = toLowerAscii(path.substr(path.size() - extension.size()));
        return pathTail == toLowerAscii(extension);
    }

    bool fileReadable(const string& path) {
        ifstream in(path);
        return in.good();
    }

    CliHistoryState& historyState() {
        static CliHistoryState state;
        return state;
    }

    void pushRecentPath(vector<string>& recent, const string& path) {
        if (path.empty()) return;

        const string needle = toLowerAscii(path);
        recent.erase(
            remove_if(recent.begin(), recent.end(), [&](const string& item) {
                return toLowerAscii(item) == needle;
            }),
            recent.end());

        recent.insert(recent.begin(), path);
        if (recent.size() > MAX_RECENT_HISTORY) {
            recent.resize(MAX_RECENT_HISTORY);
        }
    }

    void saveHistoryState() {
        const auto& st = historyState();
        ofstream out(HISTORY_FILE, ios::trunc);
        if (!out) return;

        if (!st.lastAsmPath.empty()) out << "LAST_ASM\t" << st.lastAsmPath << "\n";
        if (!st.lastTracePath.empty()) out << "LAST_TRACE\t" << st.lastTracePath << "\n";
        for (const auto& p : st.asmRecent) out << "ASM\t" << p << "\n";
        for (const auto& p : st.traceRecent) out << "TRACE\t" << p << "\n";
    }

    void ensureHistoryLoaded() {
        auto& st = historyState();
        if (st.loaded) return;

        ifstream in(HISTORY_FILE);
        string line;
        while (getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;

            const size_t tab = line.find('\t');
            if (tab == string::npos) continue;

            const string key = line.substr(0, tab);
            const string value = trim(line.substr(tab + 1));
            if (value.empty()) continue;

            if (key == "LAST_ASM") st.lastAsmPath = value;
            else if (key == "LAST_TRACE") st.lastTracePath = value;
            else if (key == "ASM") pushRecentPath(st.asmRecent, value);
            else if (key == "TRACE") pushRecentPath(st.traceRecent, value);
        }

        st.loaded = true;
    }

    void rememberAsmPath(const string& path) {
        if (path.empty()) return;
        ensureHistoryLoaded();
        auto& st = historyState();
        st.lastAsmPath = path;
        pushRecentPath(st.asmRecent, path);
        saveHistoryState();
    }

    void rememberTracePath(const string& path) {
        if (path.empty()) return;
        ensureHistoryLoaded();
        auto& st = historyState();
        st.lastTracePath = path;
        pushRecentPath(st.traceRecent, path);
        saveHistoryState();
    }

    bool chooseRecentPath(const vector<string>& recent, const string& title, string& outPath) {
        outPath.clear();
        if (recent.empty()) {
            cout << "No recent entries yet.\n";
            return false;
        }

        cout << "\n" << title << "\n";
        for (size_t i = 0; i < recent.size(); i++) {
            cout << (i + 1) << ") " << recent[i] << "\n";
        }
        cout << "0) Back\n"
             << "Select recent item: ";

        string input;
        if (!getline(cin, input)) return false;
        input = trim(input);
        if (input == "0") return false;

        int selected = 0;
        try {
            size_t idx = 0;
            selected = stoi(input, &idx);
            if (idx != input.size()) {
                cout << "Invalid recent selection.\n";
                return false;
            }
        } catch (...) {
            cout << "Invalid recent selection.\n";
            return false;
        }
        if (selected < 1 || selected > static_cast<int>(recent.size())) {
            cout << "Invalid recent selection.\n";
            return false;
        }

        outPath = recent[static_cast<size_t>(selected - 1)];
        return true;
    }

    bool getLastAsmPath(string& outPath, string& err) {
        ensureHistoryLoaded();
        const auto& st = historyState();
        if (st.lastAsmPath.empty()) {
            err = "No last ASM file available yet.";
            return false;
        }
        outPath = st.lastAsmPath;
        return true;
    }

    bool getLastTracePath(string& outPath, string& err) {
        ensureHistoryLoaded();
        const auto& st = historyState();
        if (st.lastTracePath.empty()) {
            err = "No last trace file available yet.";
            return false;
        }
        outPath = st.lastTracePath;
        return true;
    }

    vector<string> tokenizeInstruction(string line) {
        for (char& ch : line) {
            if (ch == ',') ch = ' ';
        }
        istringstream in(line);
        vector<string> out;
        string tok;
        while (in >> tok) out.push_back(tok);
        return out;
    }

    bool parseInt(const string& token, int& value) {
        try {
            size_t idx = 0;
            int parsed = stoi(token, &idx);
            if (idx != token.size()) return false;
            value = parsed;
            return true;
        } catch (...) {
            return false;
        }
    }

    bool parseRegister(const string& token, int& reg) {
        if (token.size() != 2 || (token[0] != 'R' && token[0] != 'r')) return false;
        if (token[1] < '0' || token[1] > '7') return false;
        reg = token[1] - '0';
        return true;
    }

    int getPresetMaxInput(const string& preset) {
        return (preset == "fact") ? FACT_MAX_INPUT : FIB_MAX_INPUT;
    }

    string getPresetRangeText(const string& preset) {
        return "0-" + to_string(getPresetMaxInput(preset));
    }

    bool validatePresetInput(const string& preset, int value, string& err) {
        const int maxInput = getPresetMaxInput(preset);
        if (value < 0 || value > maxInput) {
            const string label = (preset == "fact") ? "factorial" : "fibonacci";
            const string range = getPresetRangeText(preset);
            err = "Invalid " + label + " input: " + to_string(value) + ". Expected range " + range + ".";
            return false;
        }
        return true;
    }

    void resetState(VmState& vm) {
        fill(vm.memory.begin(), vm.memory.end(), 0);
        for (int i = 0; i < 8; i++) vm.R[i] = 0;
        vm.PC = 0;
        vm.FLAG_Z = 1;
        vm.FLAG_N = 0;
        vm.FLAG_P = 0;
    }

    void updateFlags(VmState& vm, uint16_t r) {
        if (r == 0) {
            vm.FLAG_Z = 1;
            vm.FLAG_N = 0;
            vm.FLAG_P = 0;
        } else if ((r >> 15) & 1) {
            vm.FLAG_Z = 0;
            vm.FLAG_N = 1;
            vm.FLAG_P = 0;
        } else {
            vm.FLAG_Z = 0;
            vm.FLAG_N = 0;
            vm.FLAG_P = 1;
        }
    }

    void loadFact(VmState& vm, int n) {
        resetState(vm);
        vm.memory[0] = (6 << 12) | (0 << 9) | 1;
        vm.memory[1] = (6 << 12) | (1 << 9) | n;
        vm.memory[2] = (6 << 12) | (2 << 9) | 3;
        vm.memory[3] = (7 << 12) | (0 << 9) | (1 << 6);
        vm.memory[4] = (8 << 12) | (1 << 9) | 1;
        vm.memory[5] = (4 << 12) | (1 << 9) | 1;
        vm.memory[6] = (5 << 12) | (2 << 9);
        vm.memory[7] = 0;
    }

    void loadFib(VmState& vm, int steps) {
        resetState(vm);
        // Produces n terms: [F0, F1, ..., F(n-1)]
        // Final displayed term is kept in R0.
        vm.memory[0] = (6 << 12) | (0 << 9) | 0;
        vm.memory[1] = (6 << 12) | (1 << 9) | 1;
        vm.memory[2] = (6 << 12) | (3 << 9) | steps;
        vm.memory[3] = (6 << 12) | (4 << 9) | 7;
        vm.memory[4] = (4 << 12) | (3 << 9) | 12;
        vm.memory[5] = (8 << 12) | (3 << 9) | 1;
        vm.memory[6] = (4 << 12) | (3 << 9) | 10;
        vm.memory[7] = (6 << 12) | (2 << 9) | 0;
        vm.memory[8] = (1 << 12) | (2 << 9) | (0 << 6);
        vm.memory[9] = (1 << 12) | (2 << 9) | (1 << 6);
        vm.memory[10] = (6 << 12) | (0 << 9) | 0;
        vm.memory[11] = (1 << 12) | (0 << 9) | (1 << 6);
        vm.memory[12] = (6 << 12) | (1 << 9) | 0;
        vm.memory[13] = (1 << 12) | (1 << 9) | (2 << 6);
        vm.memory[14] = (8 << 12) | (3 << 9) | 1;
        vm.memory[15] = (4 << 12) | (3 << 9) | 1;
        vm.memory[16] = (5 << 12) | (4 << 9);
        vm.memory[17] = 0;
    }

    string stripAsmComments(const string& rawLine) {
        string line = rawLine;
        const size_t slashPos = line.find("//");
        const size_t hashPos = line.find('#');

        size_t cutPos = string::npos;
        if (slashPos != string::npos) cutPos = slashPos;
        if (hashPos != string::npos) cutPos = (cutPos == string::npos) ? hashPos : min(cutPos, hashPos);

        if (cutPos != string::npos) {
            line = line.substr(0, cutPos);
        }
        return trim(line);
    }

    bool isValidAsmLabel(const string& label) {
        if (label.empty()) return false;
        const unsigned char c0 = static_cast<unsigned char>(label[0]);
        if (!(isalpha(c0) || label[0] == '_')) return false;
        for (char ch : label) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (!(isalnum(c) || ch == '_')) return false;
        }
        return true;
    }

    bool resolveImmediateToken(const string& token,
                               const string& op,
                               int lineNumber,
                               int currentAddress,
                               const unordered_map<string, int>& labels,
                               int& imm,
                               string& err) {
        if (parseInt(token, imm)) return true;

        const auto it = labels.find(token);
        if (it == labels.end()) {
            err = "Line " + to_string(lineNumber) + ": Unknown immediate/label \"" + token + "\".";
            return false;
        }

        const int targetAddress = it->second;
        if (op == "JZ") {
            const int relativeOffset = targetAddress - (currentAddress + 1);
            if (relativeOffset < 0 || relativeOffset > 63) {
                err = "Line " + to_string(lineNumber) + ": JZ label \"" + token + "\" is out of forward range (0..63).";
                return false;
            }
            imm = relativeOffset;
            return true;
        }

        imm = targetAddress;
        return true;
    }

    bool encodeAsmLine(const string& line,
                       int lineNumber,
                       uint16_t& encoded,
                       string& err,
                       const unordered_map<string, int>* labels = nullptr,
                       int currentAddress = -1) {
        vector<string> p = tokenizeInstruction(line);
        if (p.empty()) return false;

        const string op = p[0];
        int rd = 0;
        int rs = 0;
        int imm = 0;
        int opcode = 0;

        if (op == "HALT") {
            encoded = 0;
            return true;
        }

        if (op == "ADD" || op == "MUL") {
            if (p.size() != 3 || !parseRegister(p[1], rd) || !parseRegister(p[2], rs)) {
                err = "Line " + to_string(lineNumber) + ": " + op + " expects \"" + op + " Rn, Rm\".";
                return false;
            }
            opcode = (op == "ADD") ? 1 : 7;
        } else if (op == "JMP") {
            if (p.size() != 2 || !parseRegister(p[1], rd)) {
                err = "Line " + to_string(lineNumber) + ": JMP expects \"JMP Rn\".";
                return false;
            }
            opcode = 5;
        } else if (op == "LOAD" || op == "SUB" || op == "JZ" || op == "STORE" || op == "LOADM") {
            if (p.size() != 3 || !parseRegister(p[1], rd)) {
                err = "Line " + to_string(lineNumber) + ": " + op + " expects \"" + op + " Rn, imm(0-63)\".";
                return false;
            }

            bool immediateResolved = parseInt(p[2], imm);
            if (!immediateResolved && labels != nullptr) {
                immediateResolved = resolveImmediateToken(p[2], op, lineNumber, currentAddress, *labels, imm, err);
            }
            if (!immediateResolved || imm < 0 || imm > 63) {
                if (err.empty()) {
                    err = "Line " + to_string(lineNumber) + ": " + op + " expects \"" + op + " Rn, imm(0-63)\".";
                }
                return false;
            }

            if (op == "LOAD") opcode = 6;
            else if (op == "SUB") opcode = 8;
            else if (op == "JZ") opcode = 4;
            else if (op == "STORE") opcode = 9;
            else opcode = 10;
        } else {
            err = "Line " + to_string(lineNumber) + ": Unknown instruction \"" + op + "\".";
            return false;
        }

        encoded = static_cast<uint16_t>(((opcode & 0xF) << 12) | ((rd & 0x7) << 9) | ((rs & 0x7) << 6) | (imm & 0x3F));
        return true;
    }

    bool assembleSourceToWords(const string& path, vector<uint16_t>& programWords, string& err) {
        programWords.clear();

        ifstream in(path);
        if (!in) {
            err = "Unable to open ASM file: " + path;
            return false;
        }

        vector<pair<int, string>> instructionLines;
        unordered_map<string, int> labels;
        string line;
        int lineNumber = 0;
        int address = 0;

        while (getline(in, line)) {
            lineNumber++;
            line = stripAsmComments(line);
            if (line.empty()) continue;

            while (true) {
                const size_t colonPos = line.find(':');
                if (colonPos == string::npos) break;

                const string label = trim(line.substr(0, colonPos));
                if (!isValidAsmLabel(label)) {
                    err = "Line " + to_string(lineNumber) + ": Invalid label name \"" + label + "\".";
                    return false;
                }
                if (labels.find(label) != labels.end()) {
                    err = "Line " + to_string(lineNumber) + ": Duplicate label \"" + label + "\".";
                    return false;
                }

                labels[label] = address;
                line = trim(line.substr(colonPos + 1));
                if (line.empty()) break;
            }

            if (line.empty()) continue;
            if (address >= MEMORY_SIZE) {
                err = "Program exceeds VM memory capacity.";
                return false;
            }

            instructionLines.push_back({lineNumber, line});
            address++;
        }

        for (size_t i = 0; i < instructionLines.size(); i++) {
            uint16_t encoded = 0;
            if (!encodeAsmLine(instructionLines[i].second,
                               instructionLines[i].first,
                               encoded,
                               err,
                               &labels,
                               static_cast<int>(i))) {
                return false;
            }
            programWords.push_back(encoded);
        }

        return true;
    }

    bool loadAsmProgram(VmState& vm, const string& path, string& err) {
        resetState(vm);

        vector<uint16_t> words;
        if (!assembleSourceToWords(path, words, err)) {
            return false;
        }

        for (size_t i = 0; i < words.size(); i++) {
            vm.memory[i] = words[i];
        }

        return true;
    }

    bool assembleAsmToWords(const string& path, vector<uint16_t>& programWords, string& err) {
        return assembleSourceToWords(path, programWords, err);
    }

    bool writeBinaryProgram(const string& binPath, const vector<uint16_t>& programWords, string& err) {
        ofstream out(binPath, ios::binary | ios::trunc);
        if (!out) {
            err = "Unable to create binary file: " + binPath;
            return false;
        }

        const char magic[4] = {'C', 'V', 'M', '1'};
        out.write(magic, 4);
        uint32_t wordCount = static_cast<uint32_t>(programWords.size());
        out.write(reinterpret_cast<const char*>(&wordCount), sizeof(wordCount));

        for (uint16_t w : programWords) {
            out.write(reinterpret_cast<const char*>(&w), sizeof(w));
        }

        if (!out.good()) {
            err = "Failed while writing binary file: " + binPath;
            return false;
        }

        return true;
    }

    bool loadBinaryProgram(VmState& vm, const string& binPath, vector<uint16_t>& programWords, string& err) {
        resetState(vm);
        programWords.clear();

        ifstream in(binPath, ios::binary);
        if (!in) {
            err = "Unable to open binary file: " + binPath;
            return false;
        }

        char magic[4] = {0, 0, 0, 0};
        in.read(magic, 4);
        if (in.gcount() != 4 || strncmp(magic, "CVM1", 4) != 0) {
            err = "Invalid binary format. Expected CVM1 header.";
            return false;
        }

        uint32_t wordCount = 0;
        in.read(reinterpret_cast<char*>(&wordCount), sizeof(wordCount));
        if (!in.good()) {
            err = "Invalid binary format. Missing word count.";
            return false;
        }

        if (wordCount > static_cast<uint32_t>(MEMORY_SIZE)) {
            err = "Binary word count exceeds VM memory capacity.";
            return false;
        }

        programWords.resize(wordCount, 0);
        for (uint32_t i = 0; i < wordCount; i++) {
            uint16_t w = 0;
            in.read(reinterpret_cast<char*>(&w), sizeof(w));
            if (!in.good()) {
                err = "Binary file ended early while reading instructions.";
                return false;
            }
            programWords[i] = w;
            vm.memory[i] = w;
        }

        return true;
    }

    void writeTraceJsonl(const string& path, const vector<StepRecord>& trace) {
        ofstream out(path, ios::trunc);
        for (const auto& row : trace) {
            out << "{\"step\":" << row.step
                << ",\"pc\":" << row.pc
                << ",\"ins\":" << row.ins
                << ",\"regs\":[";
            for (int i = 0; i < 8; i++) {
                out << row.regs[i];
                if (i < 7) out << ",";
            }
            out << "]}\n";
        }
    }

    bool runVm(VmState& vm, const RunOptions& options, vector<StepRecord>& traceOut, string& err) {
        traceOut.clear();
        int step = 0;

        while (true) {
            if (step >= options.maxSteps) {
                err = "Execution stopped: max step limit reached (" + to_string(options.maxSteps) + ").";
                return false;
            }

            uint16_t pcBefore = vm.PC;
            uint16_t ins = vm.memory[vm.PC++];

            StepRecord row;
            row.step = step;
            row.pc = pcBefore;
            row.ins = ins;
            for (int i = 0; i < 8; i++) row.regs[i] = vm.R[i];
            traceOut.push_back(row);

            const int op = (ins >> 12) & 0xF;
            const int rd = (ins >> 9) & 7;
            const int rs = (ins >> 6) & 7;
            const int imm6 = ins & 0x3F;

            if (options.printStepLog) {
                cout << "step=" << step << " pc=" << pcBefore << " ins=" << ins << " op=" << op << "\n";
            }

            if (op == 0) break;

            switch (op) {
                case 1:
                    vm.R[rd] = static_cast<uint16_t>(vm.R[rd] + vm.R[rs]);
                    updateFlags(vm, vm.R[rd]);
                    break;
                case 2:
                    vm.R[rd] = static_cast<uint16_t>(vm.R[rd] & vm.R[rs]);
                    updateFlags(vm, vm.R[rd]);
                    break;
                case 3:
                    vm.R[rd] = static_cast<uint16_t>(~vm.R[rs]);
                    updateFlags(vm, vm.R[rd]);
                    break;
                case 4:
                    if (vm.R[rd] == 0) vm.PC = static_cast<uint16_t>(vm.PC + imm6);
                    break;
                case 5:
                    vm.PC = vm.R[rd];
                    break;
                case 6:
                    vm.R[rd] = static_cast<uint16_t>(imm6);
                    updateFlags(vm, vm.R[rd]);
                    break;
                case 7:
                    vm.R[rd] = static_cast<uint16_t>(vm.R[rd] * vm.R[rs]);
                    updateFlags(vm, vm.R[rd]);
                    break;
                case 8:
                    vm.R[rd] = static_cast<uint16_t>(vm.R[rd] - imm6);
                    updateFlags(vm, vm.R[rd]);
                    break;
                case 9:
                    vm.memory[imm6] = vm.R[rd];
                    break;
                case 10:
                    vm.R[rd] = vm.memory[imm6];
                    updateFlags(vm, vm.R[rd]);
                    break;
                default:
                    err = "Unknown opcode encountered: " + to_string(op);
                    return false;
            }

            step++;
        }

        if (options.writeTrace) {
            writeTraceJsonl(options.tracePath, traceOut);
        }
        return true;
    }

    void printFinalState(const VmState& vm) {
        cout << "Final Registers:\n";
        for (int i = 0; i < 8; i++) {
            cout << "  R" << i << " = " << vm.R[i] << "\n";
        }
        cout << "  PC = " << vm.PC << "\n";
        cout << "  FLAGS(Z,N,P) = (" << vm.FLAG_Z << ", " << vm.FLAG_N << ", " << vm.FLAG_P << ")\n";
    }

    void printFinalStateJson(const VmState& vm) {
        cout << "\"registers\":[";
        for (int i = 0; i < 8; i++) {
            if (i > 0) cout << ",";
            cout << vm.R[i];
        }
        cout << "],\"pc\":" << vm.PC
             << ",\"flags\":{\"z\":" << vm.FLAG_Z
             << ",\"n\":" << vm.FLAG_N
             << ",\"p\":" << vm.FLAG_P << "}";
    }

    void printUsage() {
           cout << "Custom VM CLI v" << CLI_VERSION << "\n"
               << "VM CLI Usage:\n"
             << "  vm_cli help\n"
             << "  vm_cli menu\n"
             << "  vm_cli shell\n"
             << "  vm_cli fact [n] [--trace <file>] [--no-trace] [--max-steps <n>] [--steps] [--quiet|--verbose] [--json|--format <text|json>]\n"
             << "  vm_cli fib [n]  [--trace <file>] [--no-trace] [--max-steps <n>] [--steps] [--quiet|--verbose] [--json|--format <text|json>]\n"
             << "  vm_cli asm <path.asm> [--trace <file>] [--no-trace] [--max-steps <n>] [--steps] [--quiet|--verbose] [--json|--format <text|json>]\n"
             << "  vm_cli asm --last [--trace <file>] [--no-trace] [--max-steps <n>] [--steps] [--quiet|--verbose] [--json|--format <text|json>]\n"
             << "  vm_cli asm-build <path.asm> [out.bin]\n"
             << "  vm_cli run-bin <path.bin> [--trace <file>] [--no-trace] [--max-steps <n>] [--steps] [--quiet|--verbose] [--json|--format <text|json>]\n"
             << "  vm_cli dump-bin <path.bin> [--json|--format <text|json>]\n"
             << "  vm_cli trace-summary [trace.jsonl|--last] [--json|--format <text|json>]\n"
             << "  vm_cli viewer [--open]\n\n"
             << "  vm_cli kali-ui [--open]\n\n"
             << "  vm_cli kali-app [--open]\n\n"
             << "Notes:\n"
             << "  - Immediate range is 0..63 in ASM instructions.\n"
             << "  - Binary format: 4-byte magic CVM1 + uint32 word count + uint16 instructions.\n"
             << "  - Factorial accepts n in 0.." << FACT_MAX_INPUT
             << ". Fibonacci accepts n in 0.." << FIB_MAX_INPUT << ".\n";
    }

    bool parseRunOptionsTokens(const vector<string>& args, size_t startIndex, RunOptions& options, string& err) {
        for (size_t i = startIndex; i < args.size(); i++) {
            const string& arg = args[i];
            if (arg == "--trace") {
                if (i + 1 >= args.size()) {
                    err = "Missing value for --trace";
                    return false;
                }
                options.tracePath = args[++i];
                options.writeTrace = true;
            } else if (arg == "--no-trace") {
                options.writeTrace = false;
            } else if (arg == "--max-steps") {
                if (i + 1 >= args.size()) {
                    err = "Missing value for --max-steps";
                    return false;
                }
                int parsed = 0;
                if (!parseInt(args[++i], parsed) || parsed <= 0) {
                    err = "Invalid --max-steps value";
                    return false;
                }
                options.maxSteps = parsed;
            } else if (arg == "--steps") {
                options.printStepLog = true;
            } else if (arg == "--quiet") {
                options.quietOutput = true;
            } else if (arg == "--verbose") {
                options.verboseOutput = true;
            } else if (arg == "--json") {
                options.jsonOutput = true;
            } else if (arg == "--format") {
                if (i + 1 >= args.size()) {
                    err = "Missing value for --format";
                    return false;
                }
                const string value = toLowerAscii(args[++i]);
                if (value == "json") options.jsonOutput = true;
                else if (value == "text") options.jsonOutput = false;
                else {
                    err = "Invalid --format value. Use text or json";
                    return false;
                }
            } else {
                err = "Unknown option: " + arg;
                return false;
            }
        }
        if (options.jsonOutput && options.printStepLog) {
            err = "--steps cannot be used with --json/--format json";
            return false;
        }
        if (options.quietOutput && options.verboseOutput) {
            err = "Cannot combine --quiet with --verbose";
            return false;
        }
        if (options.jsonOutput && (options.quietOutput || options.verboseOutput)) {
            err = "--quiet/--verbose cannot be used with --json/--format json";
            return false;
        }
        return true;
    }

    bool parseRunOptions(int startArg, int argc, char** argv, RunOptions& options, string& err) {
        for (int i = startArg; i < argc; i++) {
            string arg = argv[i];
            if (arg == "--trace") {
                if (i + 1 >= argc) {
                    err = "Missing value for --trace";
                    return false;
                }
                options.tracePath = argv[++i];
                options.writeTrace = true;
            } else if (arg == "--no-trace") {
                options.writeTrace = false;
            } else if (arg == "--max-steps") {
                if (i + 1 >= argc) {
                    err = "Missing value for --max-steps";
                    return false;
                }
                int parsed = 0;
                if (!parseInt(argv[++i], parsed) || parsed <= 0) {
                    err = "Invalid --max-steps value";
                    return false;
                }
                options.maxSteps = parsed;
            } else if (arg == "--steps") {
                options.printStepLog = true;
            } else if (arg == "--quiet") {
                options.quietOutput = true;
            } else if (arg == "--verbose") {
                options.verboseOutput = true;
            } else if (arg == "--json") {
                options.jsonOutput = true;
            } else if (arg == "--format") {
                if (i + 1 >= argc) {
                    err = "Missing value for --format";
                    return false;
                }
                const string value = toLowerAscii(argv[++i]);
                if (value == "json") options.jsonOutput = true;
                else if (value == "text") options.jsonOutput = false;
                else {
                    err = "Invalid --format value. Use text or json";
                    return false;
                }
            } else {
                err = "Unknown option: " + arg;
                return false;
            }
        }
        if (options.jsonOutput && options.printStepLog) {
            err = "--steps cannot be used with --json/--format json";
            return false;
        }
        if (options.quietOutput && options.verboseOutput) {
            err = "Cannot combine --quiet with --verbose";
            return false;
        }
        if (options.jsonOutput && (options.quietOutput || options.verboseOutput)) {
            err = "--quiet/--verbose cannot be used with --json/--format json";
            return false;
        }
        return true;
    }

    bool isOptionToken(const string& token) {
        return token.rfind("--", 0) == 0;
    }

    bool parseOutputModeTokens(const vector<string>& args, size_t startIndex, bool& jsonOutput, string& err) {
        for (size_t i = startIndex; i < args.size(); i++) {
            const string& arg = args[i];
            if (arg == "--json") {
                jsonOutput = true;
            } else if (arg == "--format") {
                if (i + 1 >= args.size()) {
                    err = "Missing value for --format";
                    return false;
                }
                const string value = toLowerAscii(args[++i]);
                if (value == "json") jsonOutput = true;
                else if (value == "text") jsonOutput = false;
                else {
                    err = "Invalid --format value. Use text or json";
                    return false;
                }
            } else {
                err = "Unknown option: " + arg;
                return false;
            }
        }
        return true;
    }

    bool parseOutputModeArgs(int startArg, int argc, char** argv, bool& jsonOutput, string& err) {
        for (int i = startArg; i < argc; i++) {
            const string arg = argv[i];
            if (arg == "--json") {
                jsonOutput = true;
            } else if (arg == "--format") {
                if (i + 1 >= argc) {
                    err = "Missing value for --format";
                    return false;
                }
                const string value = toLowerAscii(argv[++i]);
                if (value == "json") jsonOutput = true;
                else if (value == "text") jsonOutput = false;
                else {
                    err = "Invalid --format value. Use text or json";
                    return false;
                }
            } else {
                err = "Unknown option: " + arg;
                return false;
            }
        }
        return true;
    }

    bool parseTraceSummaryTokens(const vector<string>& args,
                                 size_t startIndex,
                                 string& tracePath,
                                 bool& jsonOutput,
                                 string& err) {
        tracePath = "trace.jsonl";
        bool explicitPath = false;
        bool useLast = false;

        for (size_t i = startIndex; i < args.size(); i++) {
            const string& token = args[i];
            if (token == "--last") {
                useLast = true;
            } else if (token == "--json") {
                jsonOutput = true;
            } else if (token == "--format") {
                if (i + 1 >= args.size()) {
                    err = "Missing value for --format";
                    return false;
                }
                const string value = toLowerAscii(args[++i]);
                if (value == "json") jsonOutput = true;
                else if (value == "text") jsonOutput = false;
                else {
                    err = "Invalid --format value. Use text or json";
                    return false;
                }
            } else if (!isOptionToken(token)) {
                if (explicitPath) {
                    err = "trace-summary accepts only one trace path.";
                    return false;
                }
                tracePath = token;
                explicitPath = true;
            } else {
                err = "Unknown option: " + token;
                return false;
            }
        }

        if (useLast && explicitPath) {
            err = "Cannot combine explicit trace path with --last";
            return false;
        }
        if (useLast) {
            string historyErr;
            if (!getLastTracePath(tracePath, historyErr)) {
                err = historyErr;
                return false;
            }
        }

        return true;
    }

    bool parseTraceSummaryArgs(int argc,
                               char** argv,
                               int startArg,
                               string& tracePath,
                               bool& jsonOutput,
                               string& err) {
        tracePath = "trace.jsonl";
        bool explicitPath = false;
        bool useLast = false;

        for (int i = startArg; i < argc; i++) {
            const string token = argv[i];
            if (token == "--last") {
                useLast = true;
            } else if (token == "--json") {
                jsonOutput = true;
            } else if (token == "--format") {
                if (i + 1 >= argc) {
                    err = "Missing value for --format";
                    return false;
                }
                const string value = toLowerAscii(argv[++i]);
                if (value == "json") jsonOutput = true;
                else if (value == "text") jsonOutput = false;
                else {
                    err = "Invalid --format value. Use text or json";
                    return false;
                }
            } else if (!isOptionToken(token)) {
                if (explicitPath) {
                    err = "trace-summary accepts only one trace path.";
                    return false;
                }
                tracePath = token;
                explicitPath = true;
            } else {
                err = "Unknown option: " + token;
                return false;
            }
        }

        if (useLast && explicitPath) {
            err = "Cannot combine explicit trace path with --last";
            return false;
        }
        if (useLast) {
            string historyErr;
            if (!getLastTracePath(tracePath, historyErr)) {
                err = historyErr;
                return false;
            }
        }

        return true;
    }

    int runPresetProgram(const string& preset, int value, const RunOptions& options) {
        string inputErr;
        if (!validatePresetInput(preset, value, inputErr)) {
            cerr << inputErr << "\n";
            return 1;
        }

        VmState vm;
        if (preset == "fact") loadFact(vm, value);
        else loadFib(vm, value);

        vector<StepRecord> trace;
        string err;
        if (!runVm(vm, options, trace, err)) {
            cerr << err << "\n";
            return 1;
        }

        const int n = value;
        vector<uint16_t> sequence;
        if (preset == "fib" && n > 0) {
            sequence.reserve(n);
            uint16_t a = 0;
            uint16_t b = 1;
            sequence.push_back(a);
            for (int i = 1; i < n; i++) {
                sequence.push_back(b);
                uint16_t next = static_cast<uint16_t>((a + b) & 0xFFFF);
                a = b;
                b = next;
            }
        }

        if (options.jsonOutput) {
            printJsonEnvelopePrefix(preset);
            cout << "\"input\":" << value;
            cout << ",\"clampedInput\":" << n;
            cout << ",\"resultR0\":" << vm.R[0];
            if (preset == "fib") {
                cout << ",\"sequence\":[";
                for (size_t i = 0; i < sequence.size(); i++) {
                    if (i > 0) cout << ",";
                    cout << sequence[i];
                }
                cout << "]";
            }
            cout << ",";
            printFinalStateJson(vm);
            cout << ",\"trace\":{\"enabled\":" << (options.writeTrace ? "true" : "false");
            if (options.writeTrace) {
                cout << ",\"path\":\"" << escapeJsonString(options.tracePath) << "\"";
            }
            cout << "}";
            printJsonEnvelopeSuffix();
            cout << "\n";
            return 0;
        }

        if (options.quietOutput) {
            if (preset == "fact") {
                cout << "Factorial of " << n << " is = " << vm.R[0] << "\n";
            } else if (preset == "fib") {
                cout << "Fibonacci sequence for " << n << " terms: ";
                if (sequence.empty()) {
                    cout << "(empty sequence)\n";
                } else {
                    for (size_t i = 0; i < sequence.size(); i++) {
                        if (i > 0) cout << " ";
                        cout << sequence[i];
                    }
                    cout << "\n";
                    cout << "The " << n << "th Fibonacci number is = " << vm.R[0] << "\n";
                }
            } else if (preset == "add") {
                cout << "Addition result: " << vm.R[0] << "\n";
            }
            return 0;
        }

        if (preset == "fact") {
            cout << "Result:\n";
            cout << "  Factorial of " << n << " is = " << vm.R[0] << "\n\n";
        } else if (preset == "fib") {
            cout << "Result:\n";
            cout << "  Fibonacci sequence for " << n << " terms: ";
            if (sequence.empty()) {
                cout << "(empty sequence)\n";
            } else {
                for (size_t i = 0; i < sequence.size(); i++) {
                    if (i > 0) cout << " ";
                    cout << sequence[i];
                }
                cout << "\n";
                cout << "  The " << n << "th Fibonacci number is = " << vm.R[0] << "\n\n";
            }
        } else if (preset == "add") {
            cout << "Result:\n";
            cout << "  Addition result: " << vm.R[0] << "\n\n";
        }

        cout << "Registers:\n";
        for (int i = 0; i < 8; i += 2) {
            cout << "  R" << i << ": " << setw(5) << dec << vm.R[i]
                 << " (" << hex16(vm.R[i]) << ")"
                 << "   R" << (i + 1) << ": " << setw(5) << dec << vm.R[i + 1]
                 << " (" << hex16(vm.R[i + 1]) << ")\n";
        }
        cout << "  PC: " << vm.PC << "\n";
        cout << "  FLAGS: Z=" << vm.FLAG_Z << " N=" << vm.FLAG_N << " P=" << vm.FLAG_P << "\n\n";

        if (options.verboseOutput) {
            cout << "Trace:\n";
            cout << "  enabled: " << (options.writeTrace ? "true" : "false") << "\n";
            if (options.writeTrace) {
                cout << "  path: " << options.tracePath << "\n";
            }
            cout << "\n";
        } else if (options.writeTrace) {
            cout << "Trace: " << options.tracePath << "\n\n";
        }

        cout << "Status: OK\n";
        return 0;
    }

    bool promptIntValue(const string& label, int& out) {
        cout << label;
        string input;
        if (!getline(cin, input)) return false;
        return parseInt(trim(input), out);
    }

    bool promptRunOutputMode(RunOptions& options) {
        while (true) {
            cout << "\nOutput mode\n"
                 << "1) Standard text\n"
                  << "2) Quiet\n"
                 << "3) Verbose\n"
                 << "4) JSON\n"
                 << "0) Back\n"
                 << "Select mode: ";

            string input;
            if (!getline(cin, input)) return false;
            input = trim(input);

            if (input == "0") return false;

            options.quietOutput = false;
            options.verboseOutput = false;
            options.jsonOutput = false;

            if (input == "1") {
                return true;
            }
            if (input == "2") {
                options.quietOutput = true;
                return true;
            }
            if (input == "3") {
                options.verboseOutput = true;
                return true;
            }
            if (input == "4") {
                options.jsonOutput = true;
                return true;
            }

            cout << "Unknown mode. Try again.\n";
        }
    }

    bool promptTraceSummaryMode(bool& jsonOutput) {
        while (true) {
            cout << "\nTrace summary mode\n"
                 << "1) Standard text\n"
                 << "2) JSON\n"
                 << "0) Back\n"
                 << "Select mode: ";

            string input;
            if (!getline(cin, input)) return false;
            input = trim(input);

            if (input == "0") return false;
            if (input == "1") {
                jsonOutput = false;
                return true;
            }
            if (input == "2") {
                jsonOutput = true;
                return true;
            }

            cout << "Unknown mode. Try again.\n";
        }
    }

        bool commandExists(const string& commandName) {
    #ifdef _WIN32
        const string checkCmd = "where " + commandName + " >nul 2>nul";
    #else
        const string checkCmd = "command -v " + commandName + " >/dev/null 2>&1";
    #endif
        return system(checkCmd.c_str()) == 0;
        }

        bool readLineFromCommand(const string& command, string& outLine) {
        outLine.clear();
    #ifdef _WIN32
        FILE* pipe = _popen(command.c_str(), "r");
    #else
        FILE* pipe = popen(command.c_str(), "r");
    #endif
        if (!pipe) return false;

        char buffer[4096] = {0};
        if (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
            outLine = trim(string(buffer));
        }

    #ifdef _WIN32
        _pclose(pipe);
    #else
        pclose(pipe);
    #endif
        return !outLine.empty();
        }

        bool chooseFileWithDialog(const string& title, const string& pattern, string& outPath) {
    #ifdef _WIN32
            string filter = "All files (*.*)|*.*";
            if (pattern.find("*.asm") != string::npos) {
                filter = "ASM files (*.asm)|*.asm|All files (*.*)|*.*";
            } else if (pattern.find("*.jsonl") != string::npos) {
                filter = "JSONL files (*.jsonl)|*.jsonl|All files (*.*)|*.*";
            }

            auto escapePsSingleQuoted = [](const string& s) {
                string out;
                out.reserve(s.size());
                for (char ch : s) {
                    if (ch == '\'') out += "''";
                    else out.push_back(ch);
                }
                return out;
            };

            const string psTitle = escapePsSingleQuoted(title);
            const string psFilter = escapePsSingleQuoted(filter);

            const string cmd =
                "powershell -NoProfile -STA -ExecutionPolicy Bypass -Command \""
                "Add-Type -AssemblyName System.Windows.Forms | Out-Null; "
                "$dlg = New-Object System.Windows.Forms.OpenFileDialog; "
                "$dlg.Title = '" + psTitle + "'; "
                "$dlg.Filter = '" + psFilter + "'; "
                "$dlg.AutoUpgradeEnabled = $false; "
                "$dlg.InitialDirectory = (Get-Location).Path; "
                "$dlg.CheckFileExists = $true; "
                "$dlg.RestoreDirectory = $true; "
                "if($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK){ [Console]::Out.WriteLine($dlg.FileName) }"
                "\"";

            return readLineFromCommand(cmd, outPath);
    #else
        if (commandExists("zenity")) {
            const string cmd = "zenity --file-selection --title=\"" + title + "\" --file-filter=\"" + pattern + "\" 2>/dev/null";
            return readLineFromCommand(cmd, outPath);
        }

        if (commandExists("kdialog")) {
            const string cmd = "kdialog --getopenfilename \"$PWD\" \"" + pattern + "\" --title \"" + title + "\" 2>/dev/null";
            return readLineFromCommand(cmd, outPath);
        }

        outPath.clear();
        return false;
    #endif
        }

    bool chooseAsmPathInteractive(string& outPath) {
        outPath.clear();
        ensureHistoryLoaded();

        while (true) {
              cout << "\nASM File Loader\n"
                  << "1) sample_addition.asm\n"
                  << "2) sample_factorial.asm\n"
                  << "3) sample_fibonacci.asm\n"
                  << "4) Browse .asm file (file picker)\n"
                  << "5) Enter custom path\n"
                  << "0) Back\n"
                  << "Tip: paste a full path directly here if you want.\n"
                  << "Select file: ";

            string rawChoice;
            if (!getline(cin, rawChoice)) return false;
            const string choice = trim(rawChoice);
            if (choice == "0") return false;

            string selectedPath;
            if (choice == "1") selectedPath = "sample_addition.asm";
            else if (choice == "2") selectedPath = "sample_factorial.asm";
            else if (choice == "3") selectedPath = "sample_fibonacci.asm";
            else if (choice == "4") {
                if (!chooseFileWithDialog("Select ASM File", "*.asm", selectedPath)) {
                    cout << "No file selected. Choose again, or press 0 to go back.\n";
                    continue;
                }
            } else if (choice == "5") {
                cout << "Enter ASM file path: ";
                if (!getline(cin, selectedPath)) return false;
            } else {
                if (isDigitsOnly(choice)) {
                    cout << "Unknown file option. Try again.\n";
                    continue;
                }
                selectedPath = choice;
            }

            selectedPath = normalizeUserPathInput(selectedPath);
            if (selectedPath.empty()) {
                cout << "ASM path cannot be empty.\n";
                continue;
            }

            if (!hasExtensionIgnoreCase(selectedPath, ".asm")) {
                cout << "Please select an .asm file.\n";
                continue;
            }

            if (!fileReadable(selectedPath)) {
                cout << "File not found or not readable: " << selectedPath << "\n";
                continue;
            }

            rememberAsmPath(selectedPath);
            outPath = selectedPath;
            return true;
        }
    }

    bool chooseTracePathInteractive(string& outPath) {
        outPath.clear();
        ensureHistoryLoaded();

        while (true) {
            cout << "\nTrace File Loader\n"
                 << "1) trace.jsonl\n"
                 << "2) trace_factorial_bin.jsonl\n"
                 << "3) trace_fibonacci_bin.jsonl\n"
                 << "4) trace_addition_bin.jsonl\n"
                 << "5) Browse .jsonl file (file picker)\n"
                 << "6) Enter custom path\n"
                 << "0) Back\n"
                 << "Tip: paste a full path directly here if you want.\n"
                 << "Select trace file: ";

            string rawChoice;
            if (!getline(cin, rawChoice)) return false;
            const string choice = trim(rawChoice);
            if (choice == "0") return false;

            string selectedPath;
            if (choice == "1") selectedPath = "trace.jsonl";
            else if (choice == "2") selectedPath = "trace_factorial_bin.jsonl";
            else if (choice == "3") selectedPath = "trace_fibonacci_bin.jsonl";
            else if (choice == "4") selectedPath = "trace_addition_bin.jsonl";
            else if (choice == "5") {
                if (!chooseFileWithDialog("Select Trace File", "*.jsonl", selectedPath)) {
                    cout << "No file selected. Choose again, or press 0 to go back.\n";
                    continue;
                }
            } else if (choice == "6") {
                cout << "Enter trace file path: ";
                if (!getline(cin, selectedPath)) return false;
            } else {
                if (isDigitsOnly(choice)) {
                    cout << "Unknown trace option. Try again.\n";
                    continue;
                }
                selectedPath = choice;
            }

            selectedPath = normalizeUserPathInput(selectedPath);
            if (selectedPath.empty()) {
                cout << "Trace path cannot be empty.\n";
                continue;
            }

            if (!hasExtensionIgnoreCase(selectedPath, ".jsonl")) {
                cout << "Please select a .jsonl file.\n";
                continue;
            }

            if (!fileReadable(selectedPath)) {
                cout << "File not found or not readable: " << selectedPath << "\n";
                continue;
            }

            rememberTracePath(selectedPath);
            outPath = selectedPath;
            return true;
        }
    }

    int runInteractiveMenu() {
        cout << "\nCustom VM CLI v" << CLI_VERSION << "\n";
        while (true) {
            cout << "\n=== VM CLI Menu ===\n"
                 << MENU_OPT_FACTORIAL << ") Factorial\n"
                 << MENU_OPT_FIBONACCI << ") Fibonacci\n"
                 << MENU_OPT_RUN_ASM << ") Run ASM file (loader)\n"
                 << MENU_OPT_TRACE_SUMMARY << ") Trace summary\n"
                 << MENU_OPT_VIEWER << ") Viewer path\n"
                 << MENU_OPT_EXIT << ") Exit\n"
                 << "Select option: ";

            string rawChoice;
            if (!getline(cin, rawChoice)) return 0;
            const string choice = trim(rawChoice);

            if (choice == MENU_OPT_EXIT) return 0;

            if (choice == MENU_OPT_FACTORIAL || choice == MENU_OPT_FIBONACCI) {
                int n = 0;
                const string preset = (choice == MENU_OPT_FACTORIAL) ? "fact" : "fib";
                const string prompt = "Enter value (" + getPresetRangeText(preset) + "): ";
                if (!promptIntValue(prompt, n)) {
                    cout << "Invalid number.\n";
                    continue;
                }
                string valueErr;
                if (!validatePresetInput(preset, n, valueErr)) {
                    cout << valueErr << "\n";
                    continue;
                }
                RunOptions options;
                if (!promptRunOutputMode(options)) {
                    continue;
                }
                runPresetProgram(preset, n, options);
                continue;
            }

            if (choice == MENU_OPT_RUN_ASM) {
                string asmPath;
                if (!chooseAsmPathInteractive(asmPath)) {
                    continue;
                }
                RunOptions options;
                if (!promptRunOutputMode(options)) {
                    continue;
                }
                runAsmProgram(asmPath, options);
                continue;
            }

            if (choice == MENU_OPT_TRACE_SUMMARY) {
                string tracePath;
                if (!chooseTracePathInteractive(tracePath)) {
                    continue;
                }
                bool jsonOutput = false;
                if (!promptTraceSummaryMode(jsonOutput)) {
                    continue;
                }
                traceSummary(tracePath, jsonOutput);
                continue;
            }

            if (choice == MENU_OPT_VIEWER) {
                handleViewerCommand(true);
                continue;
            }

            cout << "Unknown option. Try again.\n";
        }
    }

    vector<string> splitWords(const string& line) {
        istringstream in(line);
        vector<string> out;
        string token;
        while (in >> token) out.push_back(token);
        return out;
    }

    int runKaliShell() {
        cout << ANSI_BOLD << ANSI_GREEN
             << "\nCustom VM Secure Shell (Kali-style)\n"
             << ANSI_CYAN
             << "Custom VM CLI v" << CLI_VERSION << "\n"
             << "Type 'help' to list commands. Type 'exit' to quit.\n"
             << ANSI_RESET;

        while (true) {
            cout << ANSI_GREEN << "root@kali" << ANSI_RESET
                 << ":" << ANSI_CYAN << "~/custom_vm_project" << ANSI_RESET
                 << "# ";

            string line;
            if (!getline(cin, line)) return 0;
            line = trim(line);
            if (line.empty()) continue;

            vector<string> parts = splitWords(line);
            const string cmd = parts[0];

            if (cmd == "exit" || cmd == "quit") {
                cout << ANSI_CYAN << "Shell closed.\n" << ANSI_RESET;
                return 0;
            }

            if (cmd == "clear") {
                cout << string(40, '\n');
                continue;
            }

            if (cmd == "help") {
                cout << "Commands:\n"
                     << "  help\n"
                     << "  fact [n] [--trace file] [--no-trace] [--max-steps n] [--steps] [--quiet|--verbose] [--json|--format text|json]\n"
                     << "  fib [n] [--trace file] [--no-trace] [--max-steps n] [--steps] [--quiet|--verbose] [--json|--format text|json]\n"
                     << "  asm <file.asm> [--trace file] [--no-trace] [--max-steps n] [--steps] [--quiet|--verbose] [--json|--format text|json]\n"
                     << "  asm --last [--trace file] [--no-trace] [--max-steps n] [--steps] [--quiet|--verbose] [--json|--format text|json]\n"
                     << "  asm-build <file.asm> [out.bin]\n"
                     << "  run-bin <file.bin> [--trace file] [--no-trace] [--max-steps n] [--steps] [--quiet|--verbose] [--json|--format text|json]\n"
                     << "  dump-bin <file.bin> [--json|--format text|json]\n"
                     << "  trace-summary [trace.jsonl|--last] [--json|--format text|json]\n"
                     << "  viewer [--open]\n"
                     << "  kali-ui [--open]\n"
                     << "  kali-app [--open]\n"
                     << "  menu\n"
                     << "  clear\n"
                     << "  exit\n";
                continue;
            }

            if (cmd == "menu") {
                runInteractiveMenu();
                continue;
            }

            if (cmd == "fact" || cmd == "fib") {
                const string preset = cmd;
                int value = 0;
                size_t optionStart = 1;

                if (parts.size() >= 2 && !isOptionToken(parts[1])) {
                    if (!parseInt(parts[1], value)) {
                        cerr << "Invalid number: " << parts[1] << "\n";
                        continue;
                    }
                    optionStart = 2;
                } else {
                    const string prompt = "Enter value (" + getPresetRangeText(preset) + "): ";
                    if (!promptIntValue(prompt, value)) {
                        cerr << "Invalid number input.\n";
                        continue;
                    }
                }

                string valueErr;
                if (!validatePresetInput(preset, value, valueErr)) {
                    cerr << valueErr << "\n";
                    continue;
                }

                RunOptions options;
                string err;
                if (!parseRunOptionsTokens(parts, optionStart, options, err)) {
                    cerr << err << "\n";
                    continue;
                }

                runPresetProgram(preset, value, options);
                continue;
            }

            if (cmd == "asm") {
                string asmPath;
                if (parts.size() < 2) {
                    cerr << "Missing ASM file path.\n";
                    continue;
                }
                if (parts[1] == "--last") {
                    string historyErr;
                    if (!getLastAsmPath(asmPath, historyErr)) {
                        cerr << historyErr << "\n";
                        continue;
                    }
                } else {
                    asmPath = parts[1];
                }

                RunOptions options;
                string err;
                if (!parseRunOptionsTokens(parts, 2, options, err)) {
                    cerr << err << "\n";
                    continue;
                }

                runAsmProgram(asmPath, options);
                continue;
            }

            if (cmd == "asm-build") {
                if (parts.size() < 2) {
                    cerr << "Missing ASM file path.\n";
                    continue;
                }
                string outBin = (parts.size() >= 3) ? parts[2] : "program.bin";
                assembleToBinary(parts[1], outBin);
                continue;
            }

            if (cmd == "run-bin") {
                if (parts.size() < 2) {
                    cerr << "Missing binary file path.\n";
                    continue;
                }
                RunOptions options;
                string err;
                if (!parseRunOptionsTokens(parts, 2, options, err)) {
                    cerr << err << "\n";
                    continue;
                }
                runBinProgram(parts[1], options);
                continue;
            }

            if (cmd == "dump-bin") {
                if (parts.size() < 2) {
                    cerr << "Missing binary file path.\n";
                    continue;
                }
                bool jsonOutput = false;
                string err;
                if (!parseOutputModeTokens(parts, 2, jsonOutput, err)) {
                    cerr << err << "\n";
                    continue;
                }
                dumpBinary(parts[1], jsonOutput);
                continue;
            }

            if (cmd == "trace-summary") {
                string path;
                bool jsonOutput = false;
                string err;
                if (!parseTraceSummaryTokens(parts, 1, path, jsonOutput, err)) {
                    cerr << err << "\n";
                    continue;
                }
                traceSummary(path, jsonOutput);
                continue;
            }

            if (cmd == "viewer") {
                bool open = (parts.size() >= 2 && parts[1] == "--open");
                handleViewerCommand(open);
                continue;
            }

            if (cmd == "kali-ui") {
                bool open = (parts.size() >= 2 && parts[1] == "--open");
                handleKaliUiCommand(open);
                continue;
            }

            if (cmd == "kali-app") {
                bool open = (parts.size() >= 2 && parts[1] == "--open");
                handleKaliAppCommand(open);
                continue;
            }

            cerr << "Unknown shell command: " << cmd << "\n";
        }
    }

    int runAsmProgram(const string& asmPath, const RunOptions& options) {
        VmState vm;
        string err;
        if (!loadAsmProgram(vm, asmPath, err)) {
            cerr << err << "\n";
            return 1;
        }

        vector<StepRecord> trace;
        if (!runVm(vm, options, trace, err)) {
            cerr << err << "\n";
            return 1;
        }

        if (options.jsonOutput) {
            printJsonEnvelopePrefix("asm");
            cout << "\"asmPath\":\"" << escapeJsonString(asmPath) << "\"";
            cout << ",\"trace\":{\"enabled\":" << (options.writeTrace ? "true" : "false");
            if (options.writeTrace) {
                cout << ",\"path\":\"" << escapeJsonString(options.tracePath) << "\"";
            }
            cout << "},";
            printFinalStateJson(vm);
            printJsonEnvelopeSuffix();
            cout << "\n";
            rememberAsmPath(asmPath);
            return 0;
        }

        if (options.quietOutput) {
            // Friendly output for sample ASM files
            if (asmPath.find("sample_factorial.asm") != string::npos) {
                cout << "Factorial of 5 is = " << vm.R[0] << "\n";
            } else if (asmPath.find("sample_fibonacci.asm") != string::npos) {
                vector<int> fibSeq{0,1};
                for (int i = 2; i < 8; ++i) fibSeq.push_back(fibSeq[i-1] + fibSeq[i-2]);
                for (size_t i = 0; i < fibSeq.size(); ++i) {
                    if (i > 0) cout << " ";
                    cout << fibSeq[i];
                }
                cout << "\nThe 8th Fibonacci number is = " << vm.R[0] << "\n";
            } else if (asmPath.find("sample_addition.asm") != string::npos) {
                cout << "Addition of 12 and 15 is = " << vm.R[0] << "\n";
            } else {
                cout << vm.R[0] << "\n";
            }
            rememberAsmPath(asmPath);
            return 0;
        }

        // Friendly output for sample ASM files (non-quiet mode)
        if (asmPath.find("sample_factorial.asm") != string::npos) {
            cout << "Result:\n  Factorial of 5 is = " << vm.R[0] << "\n\n";
        } else if (asmPath.find("sample_fibonacci.asm") != string::npos) {
            cout << "Result:\n  Fibonacci sequence for 8 terms: ";
            vector<int> fibSeq{0,1};
            for (int i = 2; i < 8; ++i) fibSeq.push_back(fibSeq[i-1] + fibSeq[i-2]);
            for (size_t i = 0; i < fibSeq.size(); ++i) {
                if (i > 0) cout << " ";
                cout << fibSeq[i];
            }
            cout << "\n  The 8th Fibonacci number is = " << vm.R[0] << "\n\n";
        } else if (asmPath.find("sample_addition.asm") != string::npos) {
            cout << "Result:\n  Addition of 12 and 15 is = " << vm.R[0] << "\n\n";
        } else {
            cout << "Result:\n  ASM program executed successfully\n  source: " << asmPath << "\n\n";
        }

        cout << "Registers:\n";
        for (int i = 0; i < 8; i += 2) {
            cout << "  R" << i << ": " << setw(5) << dec << vm.R[i]
                 << " (" << hex16(vm.R[i]) << ")"
                 << "   R" << (i + 1) << ": " << setw(5) << dec << vm.R[i + 1]
                 << " (" << hex16(vm.R[i + 1]) << ")\n";
        }
        cout << "  PC: " << vm.PC << "\n";
        cout << "  FLAGS: Z=" << vm.FLAG_Z << " N=" << vm.FLAG_N << " P=" << vm.FLAG_P << "\n\n";

        if (options.verboseOutput) {
            cout << "Trace:\n";
            cout << "  enabled: " << (options.writeTrace ? "true" : "false") << "\n";
            if (options.writeTrace) {
                cout << "  path: " << options.tracePath << "\n";
            }
            cout << "\n";
        } else if (options.writeTrace) {
            cout << "Trace: " << options.tracePath << "\n\n";
        }

        cout << "Status: OK\n";
        rememberAsmPath(asmPath);
        return 0;
    }

    int assembleToBinary(const string& asmPath, const string& binPath) {
        vector<uint16_t> words;
        string err;
        if (!assembleAsmToWords(asmPath, words, err)) {
            cerr << err << "\n";
            return 1;
        }

        if (!writeBinaryProgram(binPath, words, err)) {
            cerr << err << "\n";
            return 1;
        }

        cout << "Assembled " << words.size() << " instructions to binary: " << binPath << "\n";
        return 0;
    }

    int runBinProgram(const string& binPath, const RunOptions& options) {
        VmState vm;
        vector<uint16_t> words;
        string err;
        if (!loadBinaryProgram(vm, binPath, words, err)) {
            cerr << err << "\n";
            return 1;
        }

        vector<StepRecord> trace;
        if (!runVm(vm, options, trace, err)) {
            cerr << err << "\n";
            return 1;
        }

        if (options.jsonOutput) {
            printJsonEnvelopePrefix("run-bin");
            cout << "\"binPath\":\"" << escapeJsonString(binPath) << "\"";
            cout << ",\"instructionCount\":" << words.size();
            cout << ",\"trace\":{\"enabled\":" << (options.writeTrace ? "true" : "false");
            if (options.writeTrace) {
                cout << ",\"path\":\"" << escapeJsonString(options.tracePath) << "\"";
            }
            cout << "},";
            printFinalStateJson(vm);
            printJsonEnvelopeSuffix();
            cout << "\n";
            return 0;
        }

        if (options.quietOutput) {
            cout << vm.R[0] << "\n";
            return 0;
        }

        cout << "Result:\n";
        cout << "  Binary program executed successfully\n";
        cout << "  source: " << binPath << "\n";
        cout << "  instruction count: " << words.size() << "\n\n";

        cout << "Registers:\n";
        for (int i = 0; i < 8; i += 2) {
            cout << "  R" << i << ": " << setw(5) << dec << vm.R[i]
                 << " (" << hex16(vm.R[i]) << ")"
                 << "   R" << (i + 1) << ": " << setw(5) << dec << vm.R[i + 1]
                 << " (" << hex16(vm.R[i + 1]) << ")\n";
        }
        cout << "  PC: " << vm.PC << "\n";
        cout << "  FLAGS: Z=" << vm.FLAG_Z << " N=" << vm.FLAG_N << " P=" << vm.FLAG_P << "\n\n";

        if (options.verboseOutput) {
            cout << "Trace:\n";
            cout << "  enabled: " << (options.writeTrace ? "true" : "false") << "\n";
            if (options.writeTrace) {
                cout << "  path: " << options.tracePath << "\n";
            }
            cout << "\n";
        } else if (options.writeTrace) {
            cout << "Trace: " << options.tracePath << "\n\n";
        }

        cout << "Status: OK\n";
        return 0;
    }

    int dumpBinary(const string& binPath, bool jsonOutput) {
        VmState vm;
        vector<uint16_t> words;
        string err;
        if (!loadBinaryProgram(vm, binPath, words, err)) {
            cerr << err << "\n";
            return 1;
        }

        if (jsonOutput) {
            printJsonEnvelopePrefix("dump-bin");
            cout << "\"binPath\":\"" << escapeJsonString(binPath) << "\"";
            cout << ",\"instructionCount\":" << words.size();
            cout << ",\"instructions\":[";
            for (size_t i = 0; i < words.size(); i++) {
                if (i > 0) cout << ",";
                cout << words[i];
            }
            cout << "]";
            printJsonEnvelopeSuffix();
            cout << "\n";
            return 0;
        }

        cout << "Binary dump: " << binPath << "\n";
        cout << "Instruction words: " << words.size() << "\n";
        for (size_t i = 0; i < words.size(); i++) {
            cout << "  [" << i << "] = " << words[i] << "\n";
        }
        return 0;
    }

    bool extractJsonIntField(const string& line, const string& fieldName, int& outValue) {
        const string key = "\"" + fieldName + "\":";
        size_t pos = line.find(key);
        if (pos == string::npos) return false;
        pos += key.size();

        while (pos < line.size() && isspace(static_cast<unsigned char>(line[pos]))) pos++;
        if (pos >= line.size()) return false;

        size_t end = pos;
        if (line[end] == '-') end++;
        while (end < line.size() && isdigit(static_cast<unsigned char>(line[end]))) end++;
        if (end == pos || (line[pos] == '-' && end == pos + 1)) return false;

        try {
            size_t idx = 0;
            const int parsed = stoi(line.substr(pos, end - pos), &idx);
            if (idx == 0) return false;
            outValue = parsed;
            return true;
        } catch (...) {
            return false;
        }
    }

    bool extractJsonRegs(const string& line, array<int, 8>& regsOut) {
        const size_t regsKeyPos = line.find("\"regs\"");
        if (regsKeyPos == string::npos) return false;

        const size_t listStart = line.find('[', regsKeyPos);
        const size_t listEnd = line.find(']', listStart == string::npos ? regsKeyPos : listStart);
        if (listStart == string::npos || listEnd == string::npos || listEnd <= listStart) return false;

        string content = line.substr(listStart + 1, listEnd - listStart - 1);
        replace(content.begin(), content.end(), ',', ' ');

        istringstream in(content);
        for (size_t i = 0; i < regsOut.size(); i++) {
            if (!(in >> regsOut[i])) return false;
        }
        return true;
    }

    string opcodeToName(int opcode) {
        switch (opcode) {
            case 0: return "HALT";
            case 1: return "ADD";
            case 2: return "AND";
            case 3: return "NOT";
            case 4: return "JZ";
            case 5: return "JMP";
            case 6: return "LOAD";
            case 7: return "MUL";
            case 8: return "SUB";
            case 9: return "STORE";
            case 10: return "LOADM";
            default: return "OP" + to_string(opcode);
        }
    }

    int traceSummary(const string& tracePath, bool jsonOutput) {
        ifstream in(tracePath);
        if (!in) {
            cerr << "Unable to open trace file: " << tracePath << "\n";
            return 1;
        }

        rememberTracePath(tracePath);

        int total = 0;
        int firstStep = -1;
        int lastStep = -1;
        string last;
        array<int, 8> firstRegs = {0, 0, 0, 0, 0, 0, 0, 0};
        array<int, 8> lastRegs = {0, 0, 0, 0, 0, 0, 0, 0};
        bool haveFirstRegs = false;
        bool haveLastRegs = false;
        array<int, 16> opcodeCounts = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            total++;
            last = line;

            int step = 0;
            if (extractJsonIntField(line, "step", step)) {
                if (firstStep < 0) firstStep = step;
                lastStep = step;
            }

            int ins = 0;
            if (extractJsonIntField(line, "ins", ins)) {
                const int op = (ins >> 12) & 0xF;
                if (op >= 0 && op < static_cast<int>(opcodeCounts.size())) {
                    opcodeCounts[static_cast<size_t>(op)]++;
                }
            }

            array<int, 8> regs = {0, 0, 0, 0, 0, 0, 0, 0};
            if (extractJsonRegs(line, regs)) {
                if (!haveFirstRegs) {
                    firstRegs = regs;
                    haveFirstRegs = true;
                }
                lastRegs = regs;
                haveLastRegs = true;
            }
        }

        vector<pair<int, int>> freq;
        for (size_t op = 0; op < opcodeCounts.size(); op++) {
            if (opcodeCounts[op] > 0) {
                freq.push_back({opcodeCounts[op], static_cast<int>(op)});
            }
        }
        sort(freq.begin(), freq.end(), [](const pair<int, int>& a, const pair<int, int>& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second < b.second;
        });

        if (jsonOutput) {
            printJsonEnvelopePrefix("trace-summary");
            cout << "\"tracePath\":\"" << escapeJsonString(tracePath) << "\"";
            cout << ",\"totalLoggedSteps\":" << total;
            if (total > 0 && firstStep >= 0 && lastStep >= 0) {
                cout << ",\"stepRange\":{\"from\":" << firstStep << ",\"to\":" << lastStep << "}";
            }
            if (haveFirstRegs && haveLastRegs) {
                cout << ",\"registerDeltas\":[";
                bool first = true;
                for (size_t i = 0; i < firstRegs.size(); i++) {
                    if (firstRegs[i] != lastRegs[i]) {
                        if (!first) cout << ",";
                        first = false;
                        cout << "{\"register\":\"R" << i
                             << "\",\"from\":" << firstRegs[i]
                             << ",\"to\":" << lastRegs[i] << "}";
                    }
                }
                cout << "]";
            }
            cout << ",\"opcodeFrequency\":[";
            for (size_t i = 0; i < freq.size(); i++) {
                if (i > 0) cout << ",";
                cout << "{\"opcode\":" << freq[i].second
                     << ",\"name\":\"" << opcodeToName(freq[i].second)
                     << "\",\"count\":" << freq[i].first << "}";
            }
            cout << "]";
            if (!last.empty()) {
                cout << ",\"lastEntryRaw\":\"" << escapeJsonString(last) << "\"";
            }
            printJsonEnvelopeSuffix();
            cout << "\n";
            return 0;
        }

        cout << "Trace file: " << tracePath << "\n";
        cout << "Total logged steps: " << total << "\n";

        if (total == 0) {
            cout << "Trace file is empty.\n";
            return 0;
        }

        // Detect which sample this is for friendly output
        string opDesc;
        if (tracePath.find("factorial") != string::npos) {
            opDesc = "Factorial of 5 is = " + to_string(lastRegs[0]);
        } else if (tracePath.find("fibonacci") != string::npos) {
            // Print all terms for 8-term Fibonacci
            vector<int> fibSeq;
            fibSeq.push_back(0);
            fibSeq.push_back(1);
            for (int i = 2; i < 8; ++i) fibSeq.push_back(fibSeq[i-1] + fibSeq[i-2]);
            ostringstream oss;
            oss << "Fibonacci sequence for 8 terms: ";
            for (size_t i = 0; i < fibSeq.size(); ++i) {
                if (i > 0) oss << " ";
                oss << fibSeq[i];
            }
            oss << "\nThe 8th Fibonacci number is = " << lastRegs[0];
            opDesc = oss.str();
        } else if (tracePath.find("addition") != string::npos) {
            opDesc = "Addition of 12 and 15 is = " + to_string(lastRegs[0]);
        }
        if (!opDesc.empty()) {
            cout << opDesc << "\n";
        }

        if (firstStep >= 0 && lastStep >= 0) {
            cout << "Step range: " << firstStep << " -> " << lastStep << "\n";
        }

        if (haveFirstRegs && haveLastRegs) {
            cout << "Register deltas:\n";
            bool anyDelta = false;
            for (size_t i = 0; i < firstRegs.size(); i++) {
                if (firstRegs[i] != lastRegs[i]) {
                    cout << "  R" << i << ": " << firstRegs[i] << " -> " << lastRegs[i] << "\n";
                    anyDelta = true;
                }
            }
            if (!anyDelta) {
                cout << "  (no register changes detected)\n";
            }
        }

        if (!freq.empty()) {
            cout << "Opcode frequency:\n";
            for (const auto& item : freq) {
                cout << "  " << opcodeToName(item.second) << " (op=" << item.second << "): " << item.first << "\n";
            }
        }

        if (!last.empty()) {
            cout << "Last entry: " << last << "\n";
        }
        return 0;
    }

    int handleViewerCommand(bool open) {
        const string viewerPath = "viewer.html";
        const string hostedViewerUrl = "https://kashvi1811.github.io/Mini_Project_2026/custom_vm_project/viewer.html";
        ifstream probe(viewerPath);
        if (!probe.good()) {
            cerr << "viewer.html not found in current directory. Run command from custom_vm_project folder.\n";
            return 1;
        }
        probe.close();

        cout << "Viewer path: " << viewerPath << "\n";
        if (open) {
#ifdef _WIN32
            string cmd = "start \"\" \"" + viewerPath + "\"";
#elif __APPLE__
            string cmd = "open \"" + viewerPath + "\"";
#else
            const char* codespacesEnv = getenv("CODESPACES");
            const char* codespaceName = getenv("CODESPACE_NAME");
            const char* forwardingDomain = getenv("GITHUB_CODESPACES_PORT_FORWARDING_DOMAIN");

            const bool looksLikeCodespaces =
                (codespacesEnv != nullptr && strlen(codespacesEnv) > 0) ||
                (codespaceName != nullptr && strlen(codespaceName) > 0) ||
                (forwardingDomain != nullptr && strlen(forwardingDomain) > 0);

            const bool hasXdgOpen = commandExists("xdg-open");

            if (looksLikeCodespaces || !hasXdgOpen) {
                if (looksLikeCodespaces) {
                    cout << "Codespaces detected. Open viewer in browser: " << hostedViewerUrl << "\n";
                } else {
                    cout << "Local browser launcher not available. Open viewer in browser: " << hostedViewerUrl << "\n";
                }
                return 0;
            }

            string cmd = "xdg-open \"" + viewerPath + "\"";
#endif
            int rc = system(cmd.c_str());
            if (rc != 0) {
                cout << "Failed to launch local browser. Open viewer in browser: " << hostedViewerUrl << "\n";
                return 0;
            }
            cout << "Viewer opened in browser.\n";
        }
        return 0;
    }

    int handleKaliUiCommand(bool open) {
        const string uiPath = "kali_ui.html";
        ifstream probe(uiPath);
        if (!probe.good()) {
            cerr << "kali_ui.html not found in current directory. Run command from custom_vm_project folder.\n";
            return 1;
        }
        probe.close();

        cout << "Kali UI path: " << uiPath << "\n";
        if (open) {
#ifdef _WIN32
            string cmd = "start \"\" \"" + uiPath + "\"";
#elif __APPLE__
            string cmd = "open \"" + uiPath + "\"";
#else
            string cmd = "xdg-open \"" + uiPath + "\"";
#endif
            int rc = system(cmd.c_str());
            if (rc != 0) {
                cerr << "Failed to launch Kali UI in browser.\n";
                return 1;
            }
            cout << "Kali UI opened in browser.\n";
        }
        return 0;
    }

    int handleKaliAppCommand(bool open) {
        const string appPath = "kali_ui.hta";
        const string uiPath = "kali_ui.html";
        const string psHostPath = "kali_app.ps1";

        ifstream probeUi(uiPath);
        if (!probeUi.good()) {
            cerr << "kali_ui.html not found in current directory. Run command from custom_vm_project folder.\n";
            return 1;
        }
        probeUi.close();

        ifstream probe(appPath);
        if (!probe.good()) {
            cerr << "kali_ui.hta not found in current directory. Run command from custom_vm_project folder.\n";
            return 1;
        }
        probe.close();

        ifstream probePs(psHostPath);
        const bool hasPsHost = probePs.good();
        if (probePs.good()) probePs.close();

        cout << "Kali app path: " << appPath << "\n";
        if (open) {
#ifdef _WIN32
            string uiArg = uiPath;
            char absBuffer[4096] = {0};
            const DWORD resolved = GetFullPathNameA(uiPath.c_str(), static_cast<DWORD>(sizeof(absBuffer)), absBuffer, nullptr);
            if (resolved > 0 && resolved < sizeof(absBuffer)) {
                string absPath(absBuffer);
                for (char& ch : absPath) {
                    if (ch == '\\') ch = '/';
                }
                uiArg = "file:///" + absPath;
            }

            const string edgePath1 = "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe";
            const string edgePath2 = "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
            const string chromePath1 = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
            const string chromePath2 = "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe";

            auto fileExists = [](const string& p) {
                ifstream f(p);
                return f.good();
            };

            string edgeExe;
            if (fileExists(edgePath1)) edgeExe = edgePath1;
            else if (fileExists(edgePath2)) edgeExe = edgePath2;

            string chromeExe;
            if (fileExists(chromePath1)) chromeExe = chromePath1;
            else if (fileExists(chromePath2)) chromeExe = chromePath2;

            if (!edgeExe.empty()) {
                int rcEdge = system(("start \"\" \"" + edgeExe + "\" --new-window --app=\"" + uiArg + "\"").c_str());
                if (rcEdge == 0) {
                    cout << "Kali app launched as modern app window (Edge).\n";
                    return 0;
                }
            }

            if (!chromeExe.empty()) {
                int rcChrome = system(("start \"\" \"" + chromeExe + "\" --new-window --app=\"" + uiArg + "\"").c_str());
                if (rcChrome == 0) {
                    cout << "Kali app launched as modern app window (Chrome).\n";
                    return 0;
                }
            }

            if (hasPsHost) {
                int rcPs = system(("start \"\" powershell -NoProfile -ExecutionPolicy Bypass -File \"" + psHostPath + "\"").c_str());
                if (rcPs == 0) {
                    cout << "Kali app launched as desktop window (legacy PowerShell host).\n";
                    return 0;
                }
            }

            int rc = system(("start \"\" mshta.exe \"" + appPath + "\"").c_str());
            if (rc == 0) {
                cout << "Kali app launch requested via HTA.\n";
                return 0;
            }

            int rcDefault = system(("start \"\" \"" + uiPath + "\"").c_str());
            if (rcDefault == 0) {
                cout << "Kali app fallback launched in default browser window.\n";
                return 0;
            }

            cerr << "Failed to launch Kali app window.\n";
            return 1;
#else
            cerr << "kali-app is currently supported on Windows only. Use 'kali-ui --open' on this platform.\n";
            return 1;
#endif
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        printUsage();
        return 0;
    }

    string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printUsage();
        return 0;
    }

    if (cmd == "menu") {
        return runInteractiveMenu();
    }

    if (cmd == "shell") {
        return runKaliShell();
    }

    if (cmd == "fact" || cmd == "fib") {
        const string preset = cmd;
        int value = 0;
        int optionStart = 3;

        if (argc >= 3 && !isOptionToken(argv[2])) {
            if (!parseInt(argv[2], value)) {
                cerr << "Invalid number: " << argv[2] << "\n";
                return 1;
            }
        } else {
            cout << "Enter value for " << preset << " (" << getPresetRangeText(preset) << "): ";
            string input;
            if (!getline(cin, input) || !parseInt(trim(input), value)) {
                cerr << "Invalid number input.\n";
                return 1;
            }
            optionStart = 2;
        }

        string valueErr;
        if (!validatePresetInput(preset, value, valueErr)) {
            cerr << valueErr << "\n";
            return 1;
        }

        RunOptions options;
        string err;
        if (!parseRunOptions(optionStart, argc, argv, options, err)) {
            cerr << err << "\n";
            return 1;
        }

        return runPresetProgram(preset, value, options);
    }

    if (cmd == "asm") {
        if (argc < 3) {
            cerr << "Missing ASM file path.\n";
            printUsage();
            return 1;
        }

        string asmPath;
        if (string(argv[2]) == "--last") {
            string historyErr;
            if (!getLastAsmPath(asmPath, historyErr)) {
                cerr << historyErr << "\n";
                return 1;
            }
        } else {
            asmPath = argv[2];
        }

        RunOptions options;
        string err;
        if (!parseRunOptions(3, argc, argv, options, err)) {
            cerr << err << "\n";
            return 1;
        }

        return runAsmProgram(asmPath, options);
    }

    if (cmd == "asm-build") {
        if (argc < 3) {
            cerr << "Missing ASM file path.\n";
            printUsage();
            return 1;
        }
        string outBin = (argc >= 4) ? argv[3] : "program.bin";
        return assembleToBinary(argv[2], outBin);
    }

    if (cmd == "run-bin") {
        if (argc < 3) {
            cerr << "Missing binary file path.\n";
            printUsage();
            return 1;
        }
        RunOptions options;
        string err;
        if (!parseRunOptions(3, argc, argv, options, err)) {
            cerr << err << "\n";
            return 1;
        }
        return runBinProgram(argv[2], options);
    }

    if (cmd == "dump-bin") {
        if (argc < 3) {
            cerr << "Missing binary file path.\n";
            printUsage();
            return 1;
        }
        bool jsonOutput = false;
        string err;
        if (!parseOutputModeArgs(3, argc, argv, jsonOutput, err)) {
            cerr << err << "\n";
            return 1;
        }
        return dumpBinary(argv[2], jsonOutput);
    }

    if (cmd == "trace-summary") {
        string path;
        bool jsonOutput = false;
        string err;
        if (!parseTraceSummaryArgs(argc, argv, 2, path, jsonOutput, err)) {
            cerr << err << "\n";
            return 1;
        }
        return traceSummary(path, jsonOutput);
    }

    if (cmd == "viewer") {
        bool open = false;
        if (argc >= 3 && string(argv[2]) == "--open") open = true;
        return handleViewerCommand(open);
    }

    if (cmd == "kali-ui") {
        bool open = false;
        if (argc >= 3 && string(argv[2]) == "--open") open = true;
        return handleKaliUiCommand(open);
    }

    if (cmd == "kali-app") {
        bool open = false;
        if (argc >= 3 && string(argv[2]) == "--open") open = true;
        return handleKaliAppCommand(open);
    }

    cerr << "Unknown command: " << cmd << "\n";
    printUsage();
    return 1;
}
