# FPGA-Accelerated Histogram of Gradients for Human Detection

This is a project using the Histogram of Gradients algorithm for lightweight human detection on FPGA: [Read the full report here](HOG-for-Human-Detection.pdf)

## Overview

### Original Implementation
The original implementation, developed as a class project and described in the accompanying paper, targeted a Xilinx PYNQ board. The system accelerated HOG feature extraction in programmable logic using HLS, with the ARM processor handling system control and classification. The design focused on demonstrating the performance benefits of moving computationally intensive computer-vision operations from software into FPGA hardware.

### Extension

The FPGA acceleration kernel was placed into a custom wireless detection system with the following components:
- A Raspberry Pi zero 2w with a pi camera for live input and wireless broadcasting
- An ESP32 receiver to take the image data and send it into the FPGA with a custom SPI
- An FPGA with the original HOG algorithm as well as the SPI logic, and an HLS accelerated SVM for inference
