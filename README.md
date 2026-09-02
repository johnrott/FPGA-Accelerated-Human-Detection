# FPGA-Accelerated Histogram of Gradients for Human Detection

This is a project using the Histogram of Gradients algorithm for lightweight human detection on FPGA: [Read the full report here](HOG-for-Human-Detection.pdf)

## Original Implementation

The original implementation, developed as a class project and described in the accompanying paper, targeted a Xilinx PYNQ board. The system accelerated HOG feature extraction in programmable logic using HLS, with the ARM processor handling system control and classification.

The design focused on demonstrating the performance benefits of moving computationally intensive computer-vision operations from software into FPGA hardware.

## Extension

The original FPGA accelerator was later incorporated into a complete wireless human-detection system consisting of:

- **Raspberry Pi Zero 2 W + Pi Camera** - captures live video and wirelessly transmits image frames.
- **ESP32 receiver** - receives image data over Wi-Fi and transfers it to the FPGA using a custom SPI communication interface.
- **Xilinx Zynq FPGA** - receives and stores incoming frames, executes the HOG feature-extraction accelerator, and performs classification using an HLS-accelerated SVM.

This extension transformed the original standalone accelerator into an end-to-end embedded vision system capable of processing live camera input.

## System Architecture

Raspberry Pi Camera - Raspberry Pi Zero 2 W - Wi-Fi - ESP32 - SPI - FPGA - HOG - SVM - Detection

## Results

- FPGA HOG processing: **~38 ms/frame (~26 FPS)**
- Complete wireless system: **~10–15 FPS**
- Hardware-accelerated HOG feature extraction
- FPGA-based SVM inference
- Custom wireless-to-SPI image-transfer pipeline
