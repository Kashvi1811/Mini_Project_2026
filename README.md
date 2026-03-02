# 🧠 Custom Register VM + Interactive Visualizer

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/Frontend-HTML%20%7C%20CSS%20%7C%20JS-E34F26?logo=html5&logoColor=white" />
  <img src="https://img.shields.io/badge/Architecture-16--bit%20VM-6C63FF" />
  <img src="https://img.shields.io/badge/Status-Active-success" />
  <img src="https://img.shields.io/badge/Purpose-Systems%20Education-0EA5E9" />
</p>

<p align="center">
  <b>Learn CPU execution visually</b> through a custom 16-bit virtual machine and step-by-step browser visualization.
</p>

---

## 📚 Table of Contents
- [✨ Why This Project](#-why-this-project)
- [🧩 What’s Included](#-whats-included)
- [⚙️ Quick Start (Windows)](#️-quick-start-windows)
- [🧠 Instruction Set](#-instruction-set)
- [🧪 Demo Programs](#-demo-programs)
- [📊 Trace & Debugging](#-trace--debugging)
- [🗂️ Project Structure](#️-project-structure)
- [⚠️ Limitations](#️-limitations)
- [🛣️ Roadmap](#️-roadmap)
- [📌 Project Note](#-project-note)

---

## ✨ Why This Project

✅ Understand **Fetch → Decode → Execute** clearly  
✅ Connect **high-level logic** to **assembly + machine encoding**  
✅ Explore register/memory state at each step  
✅ Great for **COA/OS mini-project demos** and interviews

---

## 🧩 What’s Included

| Component | Icon | Description |
|---|---:|---|
| `vm.cpp` | ⚙️ | Core VM engine (instruction execution + trace output) |
| `vm_cli.cpp` | 🖥️ | Command-line runner for presets, custom ASM, and trace tools |
| `viewer.html` | 🌐 | Interactive visualizer with step controls and narration |
| Demo programs | 🧠 | Factorial and Fibonacci examples |

---

## ⚙️ Quick Start (Windows)

### 1) Build and run VM
```powershell
cd .\custom_vm_project
g++ vm.cpp -std=c++17 -O2 -o vm.exe
.\vm.exe fact 5
.\vm.exe fib 8
```

### 2) Build and run CLI
```powershell
cd .\custom_vm_project
g++ vm_cli.cpp -std=c++17 -O2 -o vm_cli.exe

# Presets
.\vm_cli.exe fact 5
.\vm_cli.exe fib 8

# Custom ASM + trace
.\vm_cli.exe asm .\my_program.asm --trace .\trace.jsonl

# Trace summary
.\vm_cli.exe trace-summary .\trace.jsonl

# Visualizer path / open
.\vm_cli.exe viewer
.\vm_cli.exe viewer --open
```

### 3) Open visualizer
- Open `custom_vm_project/viewer.html` in browser
- Choose **Factorial / Fibonacci / Basic Addition**
- Click **Compile & Flash**
- Use **Next Step** ▶️ or **Auto Run** ⏩

---

## 🧠 Instruction Set

| Opcode | Mnemonic | Operation |
|------:|---|---|
| `0` | `HALT` | Stop execution |
| `1` | `ADD rd, rs` | `R[rd] = R[rd] + R[rs]` |
| `4` | `JZ rd, imm` | If `R[rd] == 0`, jump by immediate offset |
| `5` | `JMP rd` | `PC = R[rd]` |
| `6` | `LOAD rd, imm` | `R[rd] = imm` |
| `7` | `MUL rd, rs` | `R[rd] = R[rd] * R[rs]` |
| `8` | `SUB rd, imm` | `R[rd] = R[rd] - imm` |
| `9` | `STORE rd, imm` | `MEM[imm] = R[rd]` |

> ℹ️ Immediate field is 6-bit (`0..63`).

---

## 🧪 Demo Programs

### 🔢 Factorial (`fact n`)
- Loop-based multiplication
- Decrement-to-zero control
- Final answer in `R0`

### 🌱 Fibonacci (`fib n`)
- Iterative `prev/curr/temp` updates
- Demonstrates branching + loop mechanics

---

## 📊 Trace & Debugging

Trace file format: **JSONL** (`trace.jsonl`)  
Each line includes:
- `step`
- `pc`
- `raw_instruction`
- `register_snapshot`

Useful for:
- ✅ verification
- ✅ debugging
- ✅ project presentation screenshots

---

## 🗂️ Project Structure

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

## ⚠️ Limitations

- Immediate values limited to `0..63`
- Compact RAM UI for readability
- C++ subset supports basic arithmetic assignments only

---

## 🛣️ Roadmap

- [ ] Add more opcodes (compare, call/return, memory load)
- [ ] Improve instruction explanation panel
- [ ] Expand C++ subset (conditions + loops)
- [ ] Add automated tests for presets and encodings

---

## 📌 Project Note

Built as a systems-focused academic mini project to connect theory with practical CPU simulation through an interactive UX.