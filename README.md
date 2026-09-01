# FPGA-Accelerated Histogram of Gradients for Human Detection

This is a project using the Histogram of Gradients algorithm for lightweight human detection on FPGA: [Read the full report here](HOG-for-Human-Detection.pdf)

Contributors:
- John Rottinghaus
- Candice Liu
- Andrew Ghartey

## Overview

### Original Implementation
The original implementation, developed as a class project and described in the accompanying paper, targeted a Xilinx PYNQ board. The system accelerated HOG feature extraction in programmable logic using HLS, with the ARM processor handling system control and classification. The design focused on demonstrating the performance benefits of moving computationally intensive computer-vision operations from software into FPGA hardware.

