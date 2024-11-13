# RISC-V CPU Design with Pipeline Implementation and Assembler

## Project Overview
This project implements a 5-stage pipelined CPU for the RISC-V architecture in C++. It includes an assembler that translates RISC-V assembly instructions into binary machine code, supporting an efficient and realistic simulation of instruction execution. The pipelined CPU stages—Fetch, Decode, Execute, Memory, and Write Back—are carefully designed to model dependencies and data flow.

## Features
- **5-Stage Pipelined CPU**: Simulates the standard 5-stage RISC-V pipeline to handle instruction execution, including hazard management and data flow across stages.
- **Assembler**: Converts assembly code to machine code, mapping each instruction to its respective type (R, I, J, etc.) and encoding it for execution.
- **Reverse-Order Execution in Simulation**: Pipeline stages are processed in reverse order within a loop to better demonstrate data flow and interdependencies.
- **C++ Implementation**: Built from scratch in C++ with modular code structure for easy understanding and future extension.

## Pipeline Stages
1. **Fetch**: Retrieves the next instruction from instruction memory.
2. **Decode**: Parses the instruction and identifies its type.
3. **Execute**: Carries out arithmetic or logical operations.
4. **Memory Access**: Interacts with data memory for load/store operations.
5. **Write Back**: Writes the result back to the register file.

## Getting Started

### Prerequisites
- C++ compiler (e.g., `g++`)
- Git for version control

### Cloning the Repository
```bash
git clone https://github.com/nitin1942003/RISC-V_cpu_design.git
cd RISC-V_cpu_design
