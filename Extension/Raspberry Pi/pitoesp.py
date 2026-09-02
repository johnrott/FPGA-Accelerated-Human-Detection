from picamera2 import Picamera2
import numpy as np
import socket
import struct
import time

CAM_W = 384
CAM_H = 768

OUT_W = 128
OUT_H = 256
OUT_BYTES = OUT_W * OUT_H

ESP32_IP = "192.168.1.165"
ESP32_PORT = 5000

MAGIC = b"\xAA\x55"


def checksum_u8(data: bytes) -> int:
    return sum(data) & 0xFF


def make_packet(gray_img: np.ndarray, frame_id: int) -> bytes:
    payload = gray_img.tobytes()

    if len(payload) != OUT_BYTES:
        raise ValueError(f"Bad payload size: expected {OUT_BYTES}, got {len(payload)}")

    header = MAGIC
    header += struct.pack(">H", OUT_W)
    header += struct.pack(">H", OUT_H)
    header += struct.pack("B", frame_id & 0xFF)
    header += struct.pack(">I", len(payload))

    checksum = checksum_u8(payload)

    return header + payload + struct.pack("B", checksum)


def connect_to_esp32():
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            print(f"Connecting to ESP32 at {ESP32_IP}:{ESP32_PORT}...")
            sock.connect((ESP32_IP, ESP32_PORT))
            sock.settimeout(None)
            print("Connected to ESP32.")
            return sock
        except OSError as e:
            print(f"Connection failed: {e}")
            print("Retrying in 2 seconds...")
            time.sleep(2)


def main():
    picam2 = Picamera2()

    config = picam2.create_video_configuration(
        main={"size": (CAM_W, CAM_H), "format": "RGB888"}
    )

    picam2.configure(config)
    picam2.start()
    time.sleep(1)

    frame = picam2.capture_array()
    src_h, src_w = frame.shape[:2]

    print("Actual camera frame shape:", frame.shape)

    y_idx = np.linspace(0, src_h - 1, OUT_H).astype(np.int32)
    x_idx = np.linspace(0, src_w - 1, OUT_W).astype(np.int32)

    sock = connect_to_esp32()

    frame_id = 0
    frames = 0
    t0 = time.time()

    try:
        while True:
            frame = picam2.capture_array()

            gray = (
                0.299 * frame[:, :, 0] +
                0.587 * frame[:, :, 1] +
                0.114 * frame[:, :, 2]
            ).astype(np.uint8)

            gray_128x256 = gray[y_idx[:, None], x_idx[None, :]]
            gray_128x256 = np.ascontiguousarray(gray_128x256, dtype=np.uint8)

            packet = make_packet(gray_128x256, frame_id)

            try:
                sock.sendall(packet)
            except OSError as e:
                print(f"Send failed: {e}")
                sock.close()
                sock = connect_to_esp32()
                continue

            frame_id = (frame_id + 1) & 0xFF
            frames += 1

            elapsed = time.time() - t0

            if elapsed >= 1.0:
                fps = frames / elapsed
                print(
                    f"Send FPS: {fps:.2f} | "
                    f"packet bytes: {len(packet)} | "
                    f"image shape: {gray_128x256.shape} | "
                    f"min: {gray_128x256.min()} | "
                    f"max: {gray_128x256.max()}"
                )

                frames = 0
                t0 = time.time()

    except KeyboardInterrupt:
        print("Stopped by user.")

    finally:
        sock.close()
        picam2.stop()


if __name__ == "__main__":
    main()