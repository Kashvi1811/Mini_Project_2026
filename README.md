# 🧠 Custom Register VM + Interactive Visualizer

<p align="left">
  <img alt="Language" src="https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus&logoColor=white">
  <img alt="Frontend" src="https://img.shields.io/badge/Visualizer-HTML%2FCSS%2FJS-E34F26?logo=html5&logoColor=white">
  <img alt="Architecture" src="https://img.shields.io/badge/Architecture-16--bit%20Custom%20VM-6C63FF">
  <img alt="Type" src="https://img.shields.io/badge/Project-Systems%20%2B%20Education-0EA5E9">
</p>

A compact educational project that demonstrates how a CPU-like virtual machine executes instructions through the **Fetch → Decode → Execute** cycle.

This repository contains:

- A **C++ VM engine** (`vm.cpp`) for instruction execution and trace logging
- A **CLI runner** (`vm_cli.cpp`) for presets, custom ASM, trace tools, and viewer launch
- A **browser visualizer** (`viewer.html`) for step-by-step simulation
- Built-in demo programs for **Factorial** and **Fibonacci**

---

## 🎯 Why this project

This project is designed to make low-level execution easy to understand by connecting:

- high-level logic,
- assembly-style instructions,
- encoded machine instructions,
- and live register/memory updates.

It is ideal for mini-project demos, OS/COA learning, and systems programming practice.

---

## ✨ Key Features

- 🔢 16-bit custom instruction format (opcode + register fields + immediate)
- 🧮 VM with 8 registers (`R0` to `R7`) and program counter (`PC`)
- 📝 JSONL trace output for execution debugging (`trace.jsonl`)
- 🖥️ Visual RAM grid, register file, IR bit view, and execution narration
- ⏯️ Step execution, auto-run mode, back/undo, and reset
- 🔁 Simple C++ subset to ASM translator inside the visualizer

---

## 📁 Project Structure

```text
.
├─ README.md
├─ custom_vm_project/
│  ├─ vm.cpp
│  ├─ vm_cli.cpp
│  ├─ viewer.html
│  └─ trace.jsonl
└─ test-sfml.cpp
```

---

## 🧩 Instruction Set (Current)

| Opcode | Mnemonic | Operation |
|-------:|----------|-----------|
| 0 | `HALT` | Stop program |
| 1 | `ADD rd, rs` | `R[rd] = R[rd] + R[rs]` |
| 4 | `JZ rd, imm` | If `R[rd] == 0`, jump forward by immediate offset |
| 5 | `JMP rd` | `PC = R[rd]` |
| 6 | `LOAD rd, imm` | `R[rd] = imm` |
| 7 | `MUL rd, rs` | `R[rd] = R[rd] * R[rs]` |
| 8 | `SUB rd, imm` | `R[rd] = R[rd] - imm` |
| 9 | `STORE rd, imm` | `MEM[imm] = R[rd]` (used in viewer simulation) |

> ℹ️ Immediate values use 6 bits, so valid range is `0..63`.

---

## 🚀 Quick Start

### 1) ⚙️ Run C++ VM (PowerShell / Windows)

```powershell
cd .\custom_vm_project
g++ vm.cpp -std=c++17 -O2 -o vm.exe
.\vm.exe fact 5
.\vm.exe fib 8
```

### 1b) 🧭 Run the new CLI (all core project workflows)

```powershell
cd .\custom_vm_project
g++ vm_cli.cpp -std=c++17 -O2 -o vm_cli.exe

# Presets
.\vm_cli.exe fact 5
.\vm_cli.exe fib 8

# Custom ASM program
.\vm_cli.exe asm .\my_program.asm --trace .\trace.jsonl

# Trace quick summary
.\vm_cli.exe trace-summary .\trace.jsonl

# Visualizer path / open in browser
.\vm_cli.exe viewer
.\vm_cli.exe viewer --open
```

### 2) 🌐 Open Visualizer

- Open `custom_vm_project/viewer.html` in your browser.
- Choose a preset (**Factorial**, **Fibonacci**, or **Basic Addition**) or type code.
- Click **Compile & Flash**.
- Use **Next Step** (manual) or **Auto Run**.

---

## 🧪 Supported C++ Subset (in viewer)

The high-level editor currently supports simple assignment-based expressions, for example:

```cpp
int a = 5;
int b = 9;
int c = a + b + 3;
c = a + 2;
```

This subset is intentionally small to keep the instruction mapping clear and educational.

---

## 🧠 Demo Programs

- **Factorial (`fact n`)**
  - Uses looped multiplication and decrement-until-zero logic
  - Final result is stored in `R0`

- **Fibonacci (`fib n`)**
  - Iterative register updates (`prev`, `curr`, `temp`)
  - Demonstrates jump-based loop control

---

## 📊 Trace Output

Running `vm.cpp` generates `trace.jsonl` with per-step machine state:

- step index
- program counter
- raw instruction value
- register snapshot

This is useful for debugging, validation, and project presentation.

---

## ⚠️ Current Limitations

- Immediate values limited to `0..63`
- Visualizer RAM view is compact (for UI clarity)
- C++ translator supports basic arithmetic-style assignments only

---

## 🛣️ Roadmap

- Add more opcodes (compare, call/return, memory load)
- Improve instruction-level explanation panel (`op-name`, `op-math`)
- Expand C++ subset support (conditions/loops)
- Add automated tests for encoding and preset correctness

---

## 📌 Project Note

Built as a systems-focused academic mini project to bridge theory and practical CPU simulation with an interactive UX.

---


