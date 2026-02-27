# 🧠 Custom Stack-Based Virtual Machine (VM) with Visualization

## 📖 Overview
This project implements a **custom stack-based virtual machine in C++** that simulates CPU components — **Registers, Program Counter (PC), Stack Pointer (SP), Memory, and Instruction Set** — and executes programs using the **Fetch–Decode–Execute cycle**.  

To make execution transparent and educational, the VM includes a **step-by-step visualization layer** that highlights instructions, shows arrows for data flow, updates registers/memory/stack in real time, and logs execution events.

---

## 🎯 Objectives
- Implement the **Fetch–Decode–Execute cycle** programmatically.  
- Encode instructions into binary opcodes (e.g., `0x01` for `PUSH`, `0x02` for `ADD`).  
- Manage **virtual memory** and **stack-based computation**.  
- Build a **simple assembler** to translate human-readable code into bytecode.  
- Provide **interactive visualization** with:  
  - Highlighted current instruction  
  - Arrows showing data movement  
  - Real-time updates to PC, SP, ACC, memory, and stack  
  - Execution logs and undo/redo capability  

---

## 🛠️ Architecture
- **VM Engine:** CLI-based runtime that loads and executes bytecode.  
- **Instruction Set:** Documented opcodes for stack and arithmetic operations.  
- **Assembler:** Converts `.asm` files into `.vm` bytecode.  
- **Visualization Layer:** Technology-agnostic (SFML, Qt, OpenGL, ncurses, or Web). Displays memory cells, registers, stack, and instruction flow.  

---

## 📂 Sample Programs
- `factorial.vm` — computes factorial using stack recursion  
- `fibonacci.vm` — iterative and recursive Fibonacci  
- `arithmetic.vm` — tests ADD, SUB, MUL, DIV, STORE  

---

## 💻 Technology Stack
| Component        | Technology Options |
|------------------|---------------------|
| Core VM Engine   | C++                 |
| Instruction Format | Custom binary opcodes |
| Visualization    | SFML / Qt / OpenGL / ncurses / Web |
| Assembler        | C++ (CLI)           |
| Data Structures  | Stack, Array, Registers |

---

## 🚀 Usage
```bash
# From project root
cd custom_vm_project

# Compile VM engine
g++ vm.cpp -std=c++17 -O2 -o vm

# Run built-in programs
./vm fact 5
./vm fib 8
```

On Windows PowerShell:
```powershell
cd .\custom_vm_project
g++ vm.cpp -std=c++17 -O2 -o vm.exe
.\vm.exe fact 5
.\vm.exe fib 8
```

### Web Visualizer
- Open `custom_vm_project/viewer.html` in a browser.
- Use **Compile & Flash** to load instructions into VM RAM.
- Use **Next Step** or **Auto Run** to execute.
- Supported C++ subset in the high-level editor:
  - `int a = 5;`
  - `int c = a + b + 3;`
  - `a = b + 2;`

---

## 📘 Future Enhancements
- Branching instructions (`JMP`, `CALL`, `RET`)  
- Debugger with breakpoints and watch variables  
- Web-based visualization with drag-and-drop bytecode editor  
- ISA extension for floating-point and bitwise operations  

---

## 🧪 Problem Type
**Deeptech & System-Based Project** — combining systems programming, compiler design, and interactive visualization for educational CPU design.

---

