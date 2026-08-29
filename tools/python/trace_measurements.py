#!/usr/bin/env python3
"""Read response curves out of a published measurement plot.

The branch constants in src/dsp/ModelTables.h are fitted to measurements of an
assembled board, published as PNG plots with the Nyan-1073-EQ hardware project
(CC BY-SA 4.0):

    https://github.com/ravettel/Nyan-1073-EQ

This is how the numbers in those comments were obtained, so that they can be
checked and redone rather than taken on trust. It calibrates against the plot
frame and axis labels, then follows each curve by its colour: reading a plot by
eye is guesswork, and the first pass at these constants was wrong by up to
15.8 % in centre frequency because of it.

    python3 trace_measurements.py mid  nyan1073_mid.png
    python3 trace_measurements.py lf   nyan1073_lf.png
    python3 trace_measurements.py hpf  nyan1073_hpf.png

Needs pillow and numpy:

    uv venv && uv pip install pillow numpy
"""

import math
import sys
from collections import Counter

import numpy as np
from PIL import Image

# Plot frame, in pixels, found by locating the longest runs of grey. Every plot
# in that project shares one template, so these are constant across them.
X0, X1 = 47, 2137          # 20 Hz .. 20 kHz, logarithmic
Y0, Y1 = 15, 891           # -8 dB .. -54 dB, linear
FLAT = -30.9               # where the unprocessed reference trace sits

# Curve colours, sampled from the plots.
CURVES = {
    "mid": [
        ("360 Hz",  (0, 153, 0),     360.0),
        ("700 Hz",  (0, 102, 204),   700.0),
        ("1.6 kHz", (153, 77, 0),    1600.0),
        ("3.2 kHz", (0, 128, 127),   3200.0),
        ("4.8 kHz", (108, 13, 242),  4800.0),
        ("7.2 kHz", (163, 10, 194),  7200.0),
    ],
    "lf": [
        ("35 Hz",   (153, 77, 0),    35.0),
        ("60 Hz",   (0, 128, 127),   60.0),
        ("110 Hz",  (108, 13, 242),  110.0),
        ("220 Hz",  (163, 10, 194),  220.0),
    ],
}


def x_to_hz(x):
    return 20.0 * 10 ** (3.0 * (x - X0) / (X1 - X0))


def y_to_db(y):
    return -8.0 - 46.0 * (y - Y0) / (Y1 - Y0)


def db_to_y(db):
    return Y0 + (-8.0 - db) * (Y1 - Y0) / 46.0


def check_calibration(image):
    """The 10 kHz gridline should fall where the mapping says it does."""
    expected = X0 + (X1 - X0) * math.log10(10000.0 / 20.0) / 3.0
    grey = (
        (abs(image[:, :, 0] - image[:, :, 1]) < 12)
        & (abs(image[:, :, 1] - image[:, :, 2]) < 12)
        & (image[:, :, 0] < 200)
    )
    columns = [x for x in range(image.shape[1]) if grey[:, x].sum() > image.shape[0] * 0.5]
    nearest = min(columns, key=lambda x: abs(x - expected))
    print(f"calibration: 10 kHz predicted at x={expected:.0f}, "
          f"nearest gridline at x={nearest} ({abs(nearest - expected):.0f} px off)\n")


def trace(image, rgb, top_only=False, tolerance=45):
    """Follow one curve, returning {Hz: dB}.

    top_only takes the highest matching run in each column, which separates a
    boost curve from a cut curve drawn in the same colour.
    """
    target = np.array(rgb)
    bottom = int(db_to_y(FLAT + 1.0)) if top_only else Y1
    out = {}

    for x in range(X0, X1):
        column = image[Y0:bottom, x]
        hits = np.where(np.abs(column - target).sum(axis=1) < tolerance)[0]
        if len(hits):
            row = hits.min() if top_only else hits.mean()
            out[x_to_hz(x)] = y_to_db(Y0 + row)

    return out


def analyse_bell(curve, nominal):
    hz = np.array(sorted(curve))
    db = np.array([curve[k] for k in hz]) - FLAT

    i = int(np.argmax(db))
    peak, gain = hz[i], db[i]
    threshold = gain - 3.0

    lo = next((hz[j] for j in range(i, 0, -1) if db[j] < threshold), None)
    hi = next((hz[j] for j in range(i, len(hz)) if db[j] < threshold), None)

    if lo is None or hi is None or hi <= lo:
        return None

    octaves = math.log2(hi / lo)
    q = 1.0 / (2 ** (octaves / 2) - 2 ** (-octaves / 2))
    return peak, 100.0 * (peak - nominal) / nominal, gain, octaves, q


def analyse_shelf(curve):
    hz = np.array(sorted(curve))
    db = np.array([curve[k] for k in hz]) - FLAT

    i = int(np.argmax(db))
    boost = db[i]

    corner = next((hz[j] for j in range(i, len(hz)) if db[j] < boost - 3.0), None)
    at15 = next((hz[j] for j in range(i, len(hz)) if db[j] < boost - 15.0), None)
    slope = 12.0 / math.log2(at15 / corner) if corner and at15 and at15 > corner else float("nan")

    above = hz > 300
    dip = db[above].min()
    dip_hz = hz[above][int(np.argmin(db[above]))]

    return boost, corner, slope, dip, dip_hz, 100.0 * abs(dip) / boost


def analyse_highpass(curve):
    hz = np.array(sorted(curve))
    db = np.array([curve[k] for k in hz]) - FLAT

    peak = db.max()
    corner = next((hz[j] for j in range(len(hz) - 1, -1, -1) if db[j] < -3.0), None)
    return corner, peak


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1

    mode, path = sys.argv[1], sys.argv[2]
    image = np.array(Image.open(path).convert("RGB")).astype(int)
    check_calibration(image)

    if mode == "mid":
        print(f"{'band':>8} {'peak Hz':>9} {'err %':>7} {'gain dB':>8} {'octaves':>8} {'Q':>6}")
        for name, rgb, nominal in CURVES["mid"]:
            result = analyse_bell(trace(image, rgb, top_only=True), nominal)
            if result:
                peak, err, gain, octaves, q = result
                print(f"{name:>8} {peak:9.0f} {err:+7.1f} {gain:+8.1f} {octaves:8.2f} {q:6.2f}")

    elif mode == "lf":
        print(f"{'shelf':>8} {'boost':>7} {'corner':>8} {'dB/oct':>8} "
              f"{'dip dB':>8} {'at Hz':>8} {'dip/boost':>10}")
        for name, rgb, _ in CURVES["lf"]:
            boost, corner, slope, dip, dip_hz, ratio = analyse_shelf(trace(image, rgb))
            print(f"{name:>8} {boost:+7.1f} {corner:8.0f} {slope:8.1f} "
                  f"{dip:+8.2f} {dip_hz:8.0f} {ratio:9.1f}%")

    elif mode == "hpf":
        # The high-pass curves are not individually labelled here, so report
        # every trace that is flat at the top and cut at the bottom.
        sub = image[Y0:Y1, X0:X1]
        saturated = (sub.max(axis=2) - sub.min(axis=2)) > 60
        print(f"{'-3 dB Hz':>10} {'peak dB':>9}")
        for rgb, _ in Counter(map(tuple, sub[saturated])).most_common(8):
            curve = trace(image, rgb)
            if len(curve) < 400:
                continue
            hz = np.array(sorted(curve))
            db = np.array([curve[k] for k in hz]) - FLAT
            if db[-1] < -1.0 or db[0] > -3.0:
                continue
            corner, peak = analyse_highpass(curve)
            print(f"{corner:10.0f} {peak:+9.2f}")

    else:
        print(__doc__)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
