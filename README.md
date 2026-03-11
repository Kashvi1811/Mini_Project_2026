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
│  ├─ demo_cli.asm
│  ├─ trace.jsonl
│  ├─ trace_cli_asm.jsonl
│  ├─ trace_cli_fact.jsonl
│  ├─ trace_cli_fact6.jsonl
│  └─ trace_cli_fib7.jsonl

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

### 1b) 🧭 Run the CLI (all core project workflows)

```powershell
cd .\custom_vm_project
g++ vm_cli.cpp -std=c++17 -O2 -o vm_cli.exe

# Presets
.\vm_cli.exe fact 5
.\vm_cli.exe fib 8

# Prompt mode (asks for number)
.\vm_cli.exe fact
.\vm_cli.exe fib

# Guided menu mode
.\vm_cli.exe menu

# Kali-style terminal shell mode
.\vm_cli.exe shell

# Kali-style app window (desktop-like UI)
.\vm_cli.exe kali-ui --open

# Native app-like window on Windows (no browser chrome)
.\vm_cli.exe kali-app --open

# Custom ASM program
.\vm_cli.exe asm .\my_program.asm --trace .\trace.jsonl

# Assemble ASM -> binary and execute binary
.\vm_cli.exe asm-build .\sample_factorial.asm .\factorial.bin
.\vm_cli.exe run-bin .\factorial.bin --trace .\trace_factorial_bin.jsonl
.\vm_cli.exe dump-bin .\factorial.bin

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

### 2b) 🖥️ Open Browser CLI (No Setup)

- Open `custom_vm_project/kali_ui.html` in your browser.
- Run `help` to list commands.
- Try:
  - `fact 5`
  - `fib 8`
  - `asm-list`
  - `asm fact`
  - `asm-build fib fib.bin`
  - `run-bin fib.bin`
  - `trace-summary`

On GitHub Pages, this is directly accessible at:

- `https://<your-username>.github.io/<repo-name>/custom_vm_project/kali_ui.html`

### 2c) 📺 Open Classic Menu CLI (Screenshot-style)

- Open `custom_vm_project/menu_cli.html` in your browser.
- This page is designed to look and behave like the native VM CLI menu screen.
- Choose menu options `1..5` directly in terminal-style input.

On GitHub Pages, this is directly accessible at:

- `https://<your-username>.github.io/<repo-name>/custom_vm_project/menu_cli.html`

### 3) 🐧 WSL Support

- WSL-specific scripts and commands are in `custom_vm_project/wsl/`
- Start here: `custom_vm_project/wsl/README_WSL.md`
- Includes:
  - `run_demo_wsl.sh` (full CLI demo)
  - `open_ui_wsl.sh` (open `kali_ui.html` / `viewer.html`)
  - `wsl_shell.sh` (build + open themed shell)
  - `Makefile` shortcuts for build/test run

### 4) 🖥️ Full Kali Desktop VM Pack

- Separate folder for OS-level/Kali-desktop style packaging:
  - `custom_vm_project/kali_desktop_vm/`
- Start with:
  - `custom_vm_project/kali_desktop_vm/README.md`

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

- **ASM samples (submission-ready)**
  - `custom_vm_project/sample_factorial.asm`
  - `custom_vm_project/sample_fibonacci.asm`
  - `custom_vm_project/sample_addition.asm`

---

## 📚 Documentation Deliverables

- Instruction Set Manual: `custom_vm_project/INSTRUCTION_SET.md`
- Architecture/Design Notes: `custom_vm_project/ARCHITECTURE_NOTES.md`

> For final submission where PDF is required, export these markdown files to PDF.

---

## 📊 Trace Output

## 🌍 Publish for Everyone (GitHub Pages)

This repo includes an auto-deploy workflow in `.github/workflows/` (currently `jekyll-gh-pages.yml`).

### One-time setup

1. Push your code to GitHub on the `main` branch.
2. In GitHub: **Settings → Pages**
3. Under **Build and deployment**, set **Source** to **GitHub Actions**.
4. Wait for the workflow **Deploy GitHub Pages** to finish.

### Public links

- Landing page: `https://<your-username>.github.io/<repo-name>/`
- Browser CLI: `https://<your-username>.github.io/<repo-name>/custom_vm_project/kali_ui.html`
- Classic Menu CLI: `https://<your-username>.github.io/<repo-name>/custom_vm_project/menu_cli.html`
- Viewer: `https://<your-username>.github.io/<repo-name>/custom_vm_project/viewer.html`

## 📦 Best Way to Share Native CLI (No Clone/Build)

This repo now includes a release workflow at `.github/workflows/release-cli.yml`.

What it does:

- On every pushed tag like `v1.0.0`, GitHub Actions compiles `vm_cli.cpp` for:
  - Windows
  - Linux
  - macOS
- It auto-creates/updates a GitHub Release for that tag.
- It auto-attaches downloadable binaries.

### Publish a new version

```bash
git tag v1.0.0
git push origin v1.0.0
```

### Share one stable link

Use:

- `https://github.com/<your-username>/<repo-name>/releases/latest`

Users can open that link, download their OS binary, and run directly (no clone/build/setup).

### What users can do (no clone, no setup)

On the Browser CLI page, they can run:

- `help`
- `fact 5`
- `fib 8`
- `asm-list`
- `asm fact`
- `trace-summary`

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

- 🧩 Add more opcodes (compare, call/return, memory load)
- 🧠 Improve instruction-level explanation panel (`op-name`, `op-math`)
- 🔄 Expand C++ subset support (conditions/loops)
- ✅ Add automated tests for encoding and preset correctness

---

## 📌 Project Note

Built as a systems-focused academic mini project to connect theory with practical CPU simulation through an interactive UX.

---