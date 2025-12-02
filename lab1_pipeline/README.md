## Lab 1 — Pipeline Performance & RAW Hazard Analysis

### 📘 Overview

This lab investigates how RAW data hazards affect pipeline performance.
We extended the SimpleScalar `sim-safe` simulator to measure stalls and CPI degradation under different pipeline forwarding configurations.

### 🛠️ What We Implemented

- RAW hazard detection with stall-cycle counting
- Support for:
  - 5-stage pipeline (no forwarding)
  - 6-stage pipeline (with EX1/EX2 forwarding)
- C + PISA microbenchmarks verifying hazard correctness
- CPI computation relative to the ideal pipeline

### 🧠 Skills Demonstrated

- Pipeline hazard modeling
- Simulator instrumentation
- Microbenchmark creation
- Performance evaluation
