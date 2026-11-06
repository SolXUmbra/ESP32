#!/usr/bin/env python3
"""Upload a photo into the SUNISLE frame's SD card over the serial port.

Why this exists: the microSD sits inside the frame, and the build PC has no
card reader. The firmware therefore exposes a small PUT command and writes the
bytes itself, then rescans /SUNISLE/photos and decodes the new file at once.

Usage:
    python tools/upload_photo.py <image> [--name 01_together.jpg]
                                        [--port COM5] [--frame 216x164]

Any input format Pillow can read is accepted (PNG/JPG/HEIC-converted...).
The image is cropped "cover" to the frame ratio and saved as a 2x JPEG, so
TJpgD decodes it with a 1/2 IDCT scale straight into the 216x164 viewport -
no full-resolution frame is ever held in RAM.
"""

import argparse
import os
import pathlib
import sys
import time

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow required: pip install pillow")

try:
    import serial
except ImportError:
    sys.exit("pyserial required: pip install pyserial")

FRAME_W, FRAME_H = 216, 164      # must match ui_sunisle.cpp PHOTO_W/PHOTO_H
SD_DIR = "/SUNISLE/photos"


def prepare(src_path, out_path, focus=0.55, quality=88):
    """Cover-crop to the frame ratio, then 2x for a crisp 1/2-scale decode."""
    img = Image.open(src_path).convert("RGB")
    scale = max(FRAME_W / img.width, FRAME_H / img.height)
    nw, nh = round(img.width * scale), round(img.height * scale)
    img = img.resize((nw, nh), Image.LANCZOS)
    left = (nw - FRAME_W) // 2
    top = int((nh - FRAME_H) * focus)      # bias up: faces sit above centre
    img = img.crop((left, top, left + FRAME_W, top + FRAME_H))
    img = img.resize((FRAME_W * 2, FRAME_H * 2), Image.LANCZOS)
    img.save(out_path, "JPEG", quality=quality, optimize=True,
             progressive=False, subsampling=0)
    return img.size, os.path.getsize(out_path)


def upload(port, local_jpeg, remote_name, baud=115200, chunk=1024,
           gap=0.004, verbose=True):
    data = pathlib.Path(local_jpeg).read_bytes()
    target = "%s/%s" % (SD_DIR, remote_name)

    ser = serial.Serial(port, baud, timeout=0.2, write_timeout=10)
    log = bytearray()

    def pump(seconds):
        t0 = time.time()
        while time.time() - t0 < seconds:
            d = ser.read(4096)
            if d:
                log.extend(d)

    try:
        pump(2.0)
        ser.reset_input_buffer()
        ser.write(("PUT %s %d\n" % (target, len(data))).encode())
        ser.flush()

        t0 = time.time()
        while time.time() - t0 < 8:
            d = ser.read(4096)
            if d:
                log.extend(d)
                if b"[put] READY" in log:
                    break
        else:
            raise RuntimeError("device never acknowledged PUT (is it booted?)")

        # Chunked on purpose: the ESP32 drains 8 KB, but a single blast can
        # still overrun it while the network task logs.
        for i in range(0, len(data), chunk):
            ser.write(data[i:i + chunk])
            ser.flush()
            time.sleep(gap)

        t0 = time.time()
        result = None
        while time.time() - t0 < 40:
            d = ser.read(4096)
            if d:
                log.extend(d)
                if b"[put] OK" in log:
                    result = "OK"
                    break
                if b"[put] FAIL" in log:
                    result = "FAIL"
                    break
        pump(1.0)
    finally:
        ser.close()

    if verbose:
        for line in log.decode("utf-8", "replace").splitlines():
            s = line.strip()
            if s.startswith("[put]") or s.startswith("[sd] uploaded") \
                    or "photo scan" in s or "uibridge" in s:
                print(s)
    if result != "OK":
        raise RuntimeError("upload failed (result=%s)" % result)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--name", default=None, help="file name inside /SUNISLE/photos")
    ap.add_argument("--port", default="COM5")
    ap.add_argument("--focus", type=float, default=0.55,
                    help="0=top, 1=bottom crop bias for cover fit")
    ap.add_argument("--keep", action="store_true", help="keep the temp JPEG")
    args = ap.parse_args()

    name = args.name or pathlib.Path(args.image).stem
    if not name.lower().endswith((".jpg", ".jpeg")):
        name += ".jpg"
    name = name.replace(" ", "_")

    root = pathlib.Path(__file__).resolve().parent.parent
    tmp_dir = root / ".local"
    tmp_dir.mkdir(exist_ok=True)
    tmp = tmp_dir / name

    size, nbytes = prepare(args.image, tmp, focus=args.focus)
    print("prepared %s -> %s (%d B)" % (size, tmp.name, nbytes))
    upload(args.port, tmp, name)
    print("done: %s/%s" % (SD_DIR, name))
    if not args.keep:
        pass   # keep in .local for reproducibility; harmless


if __name__ == "__main__":
    main()
