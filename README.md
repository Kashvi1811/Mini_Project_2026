# 🧠 Custom Register VM + Interactive Visualizer

<p align="left">
  <img alt="Language" src="https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus&logoColor=white">
  <img alt="Frontend" src="https://img.shields.io/badge/Visualizer-HTML%2FCSS%2FJS-E34F26?logo=html5&logoColor=white">
  <img alt="Architecture" src="https://img.shields.io/badge/Architecture-16--bit%20Custom%20VM-6C63FF">
  <img alt="Focus" src="https://img.shields.io/badge/Focus-Systems%20Education-0EA5E9">
</p>

A compact educational system that demonstrates how a CPU-like virtual machine executes instructions through the **Fetch → Decode → Execute** pipeline.

---

## ✨ Overview

This repository combines a backend VM engine with an interactive browser visualizer to make low-level execution clear and inspectable.

### Included Components
- **C++ VM engine** (`vm.cpp`) for instruction execution + trace logging  
- **CLI runner** (`vm_cli.cpp`) for presets, custom ASM, trace tools, and viewer launch  
- **Browser visualizer** (`viewer.html`) for step-by-step execution  
- Built-in demo programs: **Factorial** and **Fibonacci**

---

## 🎯 Project Goals

This project is built to bridge:
- high-level logic,
- assembly-like instructions,
- machine encoding,
- and runtime register/memory behavior.

It is well-suited for mini-project demonstrations, COA/OS learning, and systems programming practice.

---

## 🚀 Key Features

- 16-bit custom instruction format (**opcode + register fields + immediate**)
- 8-register VM (`R0`–`R7`) with program counter (`PC`)
- JSONL trace generation (`trace.jsonl`) for debugging and analysis
- Visual register file, RAM grid, instruction bit view, and execution narration
- Manual step mode, auto-run, undo/back, and reset controls
- Lightweight C++-subset to ASM translator inside the visualizer

---

## 📁 Project Structure

```text
.
├─ README.md
├─ custom_vm_project/
│  ├─ vm.cpp
│  ├─ vm_cli.cpp
│  ├─ viewer.html
│  ├─ demo_cli.asm
│  └─ trace.jsonl
└─ test-sfml.cpp
```

---

## 🧩 Instruction Set

| Opcode | Mnemonic        | Description |
|------:|------------------|-------------|
| 0     | `HALT`           | Stop execution |
| 1     | `ADD rd, rs`     | `R[rd] = R[rd] + R[rs]` |
| 4     | `JZ rd, imm`     | If `R[rd] == 0`, jump by immediate offset |
| 5     | `JMP rd`         | `PC = R[rd]` |
| 6     | `LOAD rd, imm`   | `R[rd] = imm` |
| 7     | `MUL rd, rs`     | `R[rd] = R[rd] * R[rs]` |
| 8     | `SUB rd, imm`    | `R[rd] = R[rd] - imm` |
| 9     | `STORE rd, imm`  | `MEM[imm] = R[rd]` |

> Immediate values use 6 bits (`0..63`).

---

## ⚙️ Quick Start (Windows / PowerShell)

### 1) Build and Run VM
```powershell
cd .\custom_vm_project
g++ vm.cpp -std=c++17 -O2 -o vm.exe
.\vm.exe fact 5
.\vm.exe fib 8
```

### 2) Build and Use CLI
```powershell
cd .\custom_vm_project
g++ vm_cli.cpp -std=c++17 -O2 -o vm_cli.exe

# Presets
.\vm_cli.exe fact 5
.\vm_cli.exe fib 8

# Run custom ASM
.\vm_cli.exe asm .\my_program.asm --trace .\trace.jsonl

# Trace summary
.\vm_cli.exe trace-summary .\trace.jsonl

# Visualizer path / open in browser
.\vm_cli.exe viewer
.\vm_cli.exe viewer --open
```

### 3) Open Visualizer
- Open `custom_vm_project/viewer.html` in a browser
- Select **Factorial**, **Fibonacci**, or **Basic Addition** (or type code)
- Click **Compile & Flash**
- Run with **Next Step** or **Auto Run**

---

## 🧪 Supported C++ Subset (Visualizer)

The high-level editor currently supports simple assignment-style expressions such as:

```cpp
int a = 5;
int b = 9;
int c = a + b + 3;
c = a + 2;
```

This subset is intentionally small to keep instruction mapping transparent and educational.

---

## 🧠 Demo Programs

- **Factorial (`fact n`)**
  - Loop-based multiplication with decrement-to-zero control
  - Final result stored in `R0`

- **Fibonacci (`fib n`)**
  - Iterative register updates (`prev`, `curr`, `temp`)
  - Demonstrates jump-based loop mechanics

---

## 📊 Trace Output

Execution traces are written in JSONL format and include:
- step index
- program counter
- raw instruction value
- register snapshot

Useful for debugging, validation, and project presentation.

---

## ⚠️ Current Limitations

- Immediate range limited to `0..63`
- Compact RAM UI for readability
- C++ translator currently supports basic arithmetic assignments only

---

## 🛣️ Roadmap

- Add more opcodes (compare, call/return, memory load)
- Improve instruction explanation panel (`op-name`, `op-math`)
- Expand C++ subset (conditions and loops)
- Add automated tests for encoding and preset correctness

---

## 📌 Project Note

Built as a systems-focused academic mini project to connect theory with practical CPU simulation through an interactive UX.