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
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace {
    constexpr int MEMORY_SIZE = 1 << 16;

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
    };

    int runPresetProgram(const string& preset, int value, const RunOptions& options);
    int runAsmProgram(const string& asmPath, const RunOptions& options);
    int runBinProgram(const string& binPath, const RunOptions& options);
    int assembleToBinary(const string& asmPath, const string& binPath);
    int dumpBinary(const string& binPath);
    int traceSummary(const string& tracePath);
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
        n = max(0, min(n, 63));
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
        steps = max(0, min(steps, 63));
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

    bool encodeAsmLine(const string& line, int lineNumber, uint16_t& encoded, string& err) {
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
            if (p.size() != 3 || !parseRegister(p[1], rd) || !parseInt(p[2], imm) || imm < 0 || imm > 63) {
                err = "Line " + to_string(lineNumber) + ": " + op + " expects \"" + op + " Rn, imm(0-63)\".";
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

    bool loadAsmProgram(VmState& vm, const string& path, string& err) {
        resetState(vm);

        ifstream in(path);
        if (!in) {
            err = "Unable to open ASM file: " + path;
            return false;
        }

        string line;
        int memIndex = 0;
        int lineNumber = 0;
        while (getline(in, line)) {
            lineNumber++;
            size_t commentPos = line.find("//");
            if (commentPos != string::npos) line = line.substr(0, commentPos);
            commentPos = line.find('#');
            if (commentPos != string::npos) line = line.substr(0, commentPos);
            line = trim(line);
            if (line.empty()) continue;

            if (memIndex >= MEMORY_SIZE) {
                err = "Program exceeds VM memory capacity.";
                return false;
            }

            uint16_t encoded = 0;
            if (!encodeAsmLine(line, lineNumber, encoded, err)) {
                return false;
            }

            vm.memory[memIndex++] = encoded;
        }

        return true;
    }

    bool assembleAsmToWords(const string& path, vector<uint16_t>& programWords, string& err) {
        programWords.clear();
        ifstream in(path);
        if (!in) {
            err = "Unable to open ASM file: " + path;
            return false;
        }

        string line;
        int lineNumber = 0;
        while (getline(in, line)) {
            lineNumber++;
            size_t commentPos = line.find("//");
            if (commentPos != string::npos) line = line.substr(0, commentPos);
            commentPos = line.find('#');
            if (commentPos != string::npos) line = line.substr(0, commentPos);
            line = trim(line);
            if (line.empty()) continue;

            if (programWords.size() >= static_cast<size_t>(MEMORY_SIZE)) {
                err = "Program exceeds VM memory capacity.";
                return false;
            }

            uint16_t encoded = 0;
            if (!encodeAsmLine(line, lineNumber, encoded, err)) {
                return false;
            }

            programWords.push_back(encoded);
        }

        return true;
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

    void printUsage() {
        cout << "VM CLI Usage:\n"
             << "  vm_cli help\n"
             << "  vm_cli menu\n"
             << "  vm_cli shell\n"
             << "  vm_cli fact [n] [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli fib [n]  [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli asm <path.asm> [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli asm-build <path.asm> [out.bin]\n"
             << "  vm_cli run-bin <path.bin> [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli dump-bin <path.bin>\n"
             << "  vm_cli trace-summary [trace.jsonl]\n"
             << "  vm_cli viewer [--open]\n\n"
             << "  vm_cli kali-ui [--open]\n\n"
             << "  vm_cli kali-app [--open]\n\n"
             << "Notes:\n"
             << "  - Immediate range is 0..63 in ASM instructions.\n"
             << "  - Binary format: 4-byte magic CVM1 + uint32 word count + uint16 instructions.\n"
             << "  - Preset commands clamp n to 0..63.\n";
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
            } else {
                err = "Unknown option: " + arg;
                return false;
            }
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
            } else {
                err = "Unknown option: " + arg;
                return false;
            }
        }
        return true;
    }

    bool isOptionToken(const string& token) {
        return token.rfind("--", 0) == 0;
    }

    int runPresetProgram(const string& preset, int value, const RunOptions& options) {
        VmState vm;
        if (preset == "fact") loadFact(vm, value);
        else loadFib(vm, value);

        vector<StepRecord> trace;
        string err;
        if (!runVm(vm, options, trace, err)) {
            cerr << err << "\n";
            return 1;
        }

        if (preset == "fact") {
            cout << "Factorial(" << max(0, min(value, 63)) << ") result in R0 = " << vm.R[0] << "\n";
        } else {
            const int n = max(0, min(value, 63));
            vector<uint16_t> sequence;
            if (n > 0) {
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

            cout << "Fibonacci sequence (" << n << " terms): ";
            if (sequence.empty()) {
                cout << "[]\n";
            } else {
                cout << "[";
                for (size_t i = 0; i < sequence.size(); i++) {
                    cout << sequence[i];
                    if (i + 1 < sequence.size()) cout << ", ";
                }
                cout << "]\n";
            }

            cout << "VM final Fibonacci term (R0) = " << vm.R[0] << "\n";
        }
        printFinalState(vm);
        if (options.writeTrace) cout << "Trace written to: " << options.tracePath << "\n";
        return 0;
    }

    bool promptIntValue(const string& label, int& out) {
        cout << label;
        string input;
        if (!getline(cin, input)) return false;
        return parseInt(trim(input), out);
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
        (void)title;
        (void)pattern;
        outPath.clear();
        return false;
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

        if (choice == "1") outPath = "sample_addition.asm";
        else if (choice == "2") outPath = "sample_factorial.asm";
        else if (choice == "3") outPath = "sample_fibonacci.asm";
        else if (choice == "4") {
            if (!chooseFileWithDialog("Select ASM File", "*.asm", outPath)) {
                cout << "File picker unavailable/cancelled. Install zenity or kdialog, or choose custom path.\n";
                return false;
            }
        } else if (choice == "5") {
            cout << "Enter ASM file path: ";
            if (!getline(cin, outPath)) return false;
            outPath = normalizeUserPathInput(outPath);
            if (outPath.empty()) {
                cout << "ASM path cannot be empty.\n";
                return false;
            }
        } else {
            if (isDigitsOnly(choice)) {
                cout << "Unknown file option. Try again.\n";
                return false;
            }
            outPath = normalizeUserPathInput(choice);
        }

        ifstream probe(outPath);
        if (!probe.good()) {
            cout << "Warning: file not found in current directory: " << outPath << "\n";
        }
        return true;
    }

    bool chooseTracePathInteractive(string& outPath) {
        outPath.clear();

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

        if (choice == "1") outPath = "trace.jsonl";
        else if (choice == "2") outPath = "trace_factorial_bin.jsonl";
        else if (choice == "3") outPath = "trace_fibonacci_bin.jsonl";
        else if (choice == "4") outPath = "trace_addition_bin.jsonl";
        else if (choice == "5") {
            if (!chooseFileWithDialog("Select Trace File", "*.jsonl", outPath)) {
                cout << "File picker unavailable/cancelled. Install zenity or kdialog, or choose custom path.\n";
                return false;
            }
        } else if (choice == "6") {
            cout << "Enter trace file path: ";
            if (!getline(cin, outPath)) return false;
            outPath = normalizeUserPathInput(outPath);
            if (outPath.empty()) {
                cout << "Trace path cannot be empty.\n";
                return false;
            }
        } else {
            if (isDigitsOnly(choice)) {
                cout << "Unknown trace option. Try again.\n";
                return false;
            }
            outPath = normalizeUserPathInput(choice);
        }

        ifstream probe(outPath);
        if (!probe.good()) {
            cout << "Warning: file not found in current directory: " << outPath << "\n";
        }
        return true;
    }

    int runInteractiveMenu() {
        while (true) {
            cout << "\n=== VM CLI Menu ===\n"
                 << "1) Factorial\n"
                 << "2) Fibonacci\n"
                 << "3) Run ASM file (loader)\n"
                 << "4) Trace summary\n"
                  << "5) Open viewer\n"
                  << "6) Viewer path\n"
                 << "0) Exit\n"
                 << "Select option: ";

            string rawChoice;
            if (!getline(cin, rawChoice)) return 0;
            const string choice = trim(rawChoice);

            if (choice == "0") return 0;

            if (choice == "1" || choice == "2") {
                int n = 0;
                if (!promptIntValue("Enter value (0-63): ", n)) {
                    cout << "Invalid number.\n";
                    continue;
                }
                RunOptions options;
                const string preset = (choice == "1") ? "fact" : "fib";
                runPresetProgram(preset, n, options);
                continue;
            }

            if (choice == "3") {
                string asmPath;
                if (!chooseAsmPathInteractive(asmPath)) {
                    continue;
                }
                RunOptions options;
                runAsmProgram(asmPath, options);
                continue;
            }

            if (choice == "4") {
                string tracePath;
                if (!chooseTracePathInteractive(tracePath)) {
                    continue;
                }
                traceSummary(tracePath);
                continue;
            }

            if (choice == "5") {
                handleViewerCommand(true);
                continue;
            }

            if (choice == "6") {
                handleViewerCommand(false);
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
                     << "  fact [n] [--trace file] [--no-trace] [--max-steps n] [--steps]\n"
                     << "  fib [n] [--trace file] [--no-trace] [--max-steps n] [--steps]\n"
                     << "  asm <file.asm> [--trace file] [--no-trace] [--max-steps n] [--steps]\n"
                     << "  asm-build <file.asm> [out.bin]\n"
                     << "  run-bin <file.bin> [--trace file] [--no-trace] [--max-steps n] [--steps]\n"
                     << "  dump-bin <file.bin>\n"
                     << "  trace-summary [trace.jsonl]\n"
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
                int value = 0;
                size_t optionStart = 1;

                if (parts.size() >= 2 && !isOptionToken(parts[1])) {
                    if (!parseInt(parts[1], value)) {
                        cerr << "Invalid number: " << parts[1] << "\n";
                        continue;
                    }
                    optionStart = 2;
                } else {
                    if (!promptIntValue("Enter value (0-63): ", value)) {
                        cerr << "Invalid number input.\n";
                        continue;
                    }
                }

                RunOptions options;
                string err;
                if (!parseRunOptionsTokens(parts, optionStart, options, err)) {
                    cerr << err << "\n";
                    continue;
                }

                runPresetProgram(cmd, value, options);
                continue;
            }

            if (cmd == "asm") {
                if (parts.size() < 2) {
                    cerr << "Missing ASM file path.\n";
                    continue;
                }

                const string asmPath = parts[1];
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
                dumpBinary(parts[1]);
                continue;
            }

            if (cmd == "trace-summary") {
                const string path = (parts.size() >= 2) ? parts[1] : "trace.jsonl";
                traceSummary(path);
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

        cout << "ASM program executed successfully.\n";
        printFinalState(vm);
        if (options.writeTrace) cout << "Trace written to: " << options.tracePath << "\n";
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

        cout << "Binary program executed successfully (" << words.size() << " instructions loaded).\n";
        printFinalState(vm);
        if (options.writeTrace) cout << "Trace written to: " << options.tracePath << "\n";
        return 0;
    }

    int dumpBinary(const string& binPath) {
        VmState vm;
        vector<uint16_t> words;
        string err;
        if (!loadBinaryProgram(vm, binPath, words, err)) {
            cerr << err << "\n";
            return 1;
        }

        cout << "Binary dump: " << binPath << "\n";
        cout << "Instruction words: " << words.size() << "\n";
        for (size_t i = 0; i < words.size(); i++) {
            cout << "  [" << i << "] = " << words[i] << "\n";
        }
        return 0;
    }

    int traceSummary(const string& tracePath) {
        ifstream in(tracePath);
        if (!in) {
            cerr << "Unable to open trace file: " << tracePath << "\n";
            return 1;
        }

        int total = 0;
        string last;
        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            total++;
            last = line;
        }

        cout << "Trace file: " << tracePath << "\n";
        cout << "Total logged steps: " << total << "\n";
        if (!last.empty()) {
            cout << "Last entry: " << last << "\n";
        }
        return 0;
    }

    int handleViewerCommand(bool open) {
        const string viewerPath = "viewer.html";
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
            string cmd = "xdg-open \"" + viewerPath + "\"";
#endif
            int rc = system(cmd.c_str());
            if (rc != 0) {
                cerr << "Failed to launch viewer in browser.\n";
                return 1;
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
        int value = 0;
        int optionStart = 3;

        if (argc >= 3 && !isOptionToken(argv[2])) {
            if (!parseInt(argv[2], value)) {
                cerr << "Invalid number: " << argv[2] << "\n";
                return 1;
            }
        } else {
            cout << "Enter value for " << cmd << " (0-63): ";
            string input;
            if (!getline(cin, input) || !parseInt(trim(input), value)) {
                cerr << "Invalid number input.\n";
                return 1;
            }
            optionStart = 2;
        }

        RunOptions options;
        string err;
        if (!parseRunOptions(optionStart, argc, argv, options, err)) {
            cerr << err << "\n";
            return 1;
        }

        return runPresetProgram(cmd, value, options);
    }

    if (cmd == "asm") {
        if (argc < 3) {
            cerr << "Missing ASM file path.\n";
            printUsage();
            return 1;
        }

        string asmPath = argv[2];
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
        return dumpBinary(argv[2]);
    }

    if (cmd == "trace-summary") {
        string path = (argc >= 3) ? argv[2] : "trace.jsonl";
        return traceSummary(path);
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
