## Lab 3 — Tomasulo Dynamic Scheduling

### 📘 Overview

This lab implements a cycle-accurate simulation of Tomasulo’s dynamic scheduling algorithm. The goal is to compute the total number of cycles required to execute the first 1,000,000 instructions from the gcc, go, and compress EIO traces.

### 🛠️ What We Implemented

- Instruction Fetch Queue (IFQ)
- Integer + FP reservation stations
- Register renaming (tag-based dependencies)
- Functional units (INT/FP) with precise timing
- CDB broadcast + arbitration for oldest completing instruction

### 🧪 Experiments & Results

- Simulated gcc, go, compress for 1,000,000 instructions
- Recorded dispatch/issue/execute/CDB cycles per instruction

### 🧠 Skills Demonstrated

- Tomasulo OOO scheduling
- Reservation stations & dependency resolution
- Register renaming (map table + tags)
- FU allocation and latency modeling
- CDB arbitration & in-order retirement
- Cycle-accurate architectural simulation
