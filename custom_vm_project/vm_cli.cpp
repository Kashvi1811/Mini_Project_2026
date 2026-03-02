#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

    string trim(const string& text) {
        size_t left = 0;
        while (left < text.size() && isspace(static_cast<unsigned char>(text[left]))) left++;
        size_t right = text.size();
        while (right > left && isspace(static_cast<unsigned char>(text[right - 1]))) right--;
        return text.substr(left, right - left);
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
        vm.memory[0] = (6 << 12) | (0 << 9) | 0;
        vm.memory[1] = (6 << 12) | (1 << 9) | 1;
        vm.memory[2] = (6 << 12) | (4 << 9) | steps;
        vm.memory[3] = (6 << 12) | (5 << 9) | 4;
        vm.memory[4] = (6 << 12) | (2 << 9) | 0;
        vm.memory[5] = (1 << 12) | (2 << 9) | (0 << 6);
        vm.memory[6] = (1 << 12) | (2 << 9) | (1 << 6);
        vm.memory[7] = (6 << 12) | (0 << 9) | 0;
        vm.memory[8] = (1 << 12) | (0 << 9) | (1 << 6);
        vm.memory[9] = (6 << 12) | (1 << 9) | 0;
        vm.memory[10] = (1 << 12) | (1 << 9) | (2 << 6);
        vm.memory[11] = (8 << 12) | (4 << 9) | 1;
        vm.memory[12] = (4 << 12) | (4 << 9) | 1;
        vm.memory[13] = (5 << 12) | (5 << 9);
        vm.memory[14] = 0;
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
             << "  vm_cli fact <n> [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli fib <n>  [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli asm <path.asm> [--trace <file>] [--no-trace] [--max-steps <n>] [--steps]\n"
             << "  vm_cli trace-summary [trace.jsonl]\n"
             << "  vm_cli viewer [--open]\n\n"
             << "Notes:\n"
             << "  - Immediate range is 0..63 in ASM instructions.\n"
             << "  - Preset commands clamp n to 0..63.\n";
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

            cout << "VM final Fibonacci register (R1) = " << vm.R[1] << "\n";
        }
        printFinalState(vm);
        if (options.writeTrace) cout << "Trace written to: " << options.tracePath << "\n";
        return 0;
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

    if (cmd == "trace-summary") {
        string path = (argc >= 3) ? argv[2] : "trace.jsonl";
        return traceSummary(path);
    }

    if (cmd == "viewer") {
        bool open = false;
        if (argc >= 3 && string(argv[2]) == "--open") open = true;
        return handleViewerCommand(open);
    }

    cerr << "Unknown command: " << cmd << "\n";
    printUsage();
    return 1;
}
