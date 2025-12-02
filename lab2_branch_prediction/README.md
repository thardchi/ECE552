## Lab 2 — Dynamic Branch Prediction

### 📘 Overview

This lab focuses on implementing dynamic branch prediction schemes using the 4th Championship Branch Prediction framework and evaluating their accuracy.

### 🛠️ What We Implemented

- 2-bit Bimodal Predictor
  - 8192-entry table of 2-bit saturating counters
  - Indexed directly by lower bits of PC
- Two-Level Local Predictor (PAp)
  - 512-entry Branch History Table (BHT)
  - 6-bit per-branch local histories
  - 8 Pattern History Tables (PHTs), each with 2-bit counters
  - Local-history–based indexing (PC → BHT → PHT)
- Open-Ended Predictor: GShare Design
  - 15-bit global history register (GHR)
  - 1 PHT where each entry is a 3-bit saturating counter
- CACTI modeling (area, timing, leakage)

### 🧪 Experiments & Results

- Evaluated MPKI across 8 benchmarks: astar, bwaves, bzip2, gcc, gromacs, hmmer, mcf, soplex
- GShare predictor produced the lowest miss prediction per 1K instructions among all designs
- Explored budget/accuracy tradeoffs using table size and counter width variations
- Used CACTI 6.5 to analyze storage cost, access time, and leakage power

### 🧠 Skills Demonstrated

- Branch prediction architecture
- GShare, bimodal, and two-level local predictors
- Trace-driven simulation
- Hardware budgeting (size vs. performance)
- CACTI modeling for memory structures
