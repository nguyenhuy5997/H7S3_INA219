"""Real-time plot of the INA219_H7S3 UART log.

    py -3 ina219_plot.py                     auto-detect the ST-Link VCP
    py -3 ina219_plot.py --port COM16 --window 120 --csv log.csv
"""
import argparse
import collections
import csv
import re
import sys
import threading
import time

import matplotlib.pyplot as plt
import serial
from matplotlib.animation import FuncAnimation
from serial.tools import list_ports

LINE_RE = re.compile(
    r"Vbus=(?P<vbus>-?[\d.]+) V\s+Vshunt=(?P<vshunt>-?[\d.]+) mV\s+I=(?P<i>-?[\d.]+) mA\s+"
    r"P=(?P<p>-?[\d.]+) mW\s+err=(?P<err>\d+)(?P<ovf>\s+OVF)?"
)

CHANNELS = [
    ("vbus", "Vbus", "V", "%.3f"),
    ("vshunt", "Vshunt", "mV", "%.2f"),
    ("i", "I", "mA", "%.3f"),
    ("p", "P", "mW", "%.2f"),
]


def find_port():
    for p in list_ports.comports():
        if p.vid == 0x0483 and ("STLink" in (p.description or "") or "STLINK" in (p.description or "")):
            return p.device
    for p in list_ports.comports():
        if p.vid == 0x0483:
            return p.device
    return None


class Reader(threading.Thread):
    def __init__(self, ser, window_s, csv_path):
        super().__init__(daemon=True)
        self.ser = ser
        self.window_s = window_s
        self.lock = threading.Lock()
        self.t = collections.deque()
        self.data = {key: collections.deque() for key, *_ in CHANNELS}
        self.err = 0
        self.ovf = False
        self.samples = 0
        self.t0 = time.monotonic()
        self.stop = threading.Event()
        self.csv_file = open(csv_path, "w", newline="") if csv_path else None
        self.csv = csv.writer(self.csv_file) if self.csv_file else None
        if self.csv:
            self.csv.writerow(["time_s", "vbus_v", "vshunt_mv", "i_ma", "p_mw", "err", "ovf"])

    def run(self):
        buf = b""
        while not self.stop.is_set():
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                self.handle(raw.decode("ascii", "replace").strip("\r\x00 "))

    def handle(self, line):
        if not line:
            return
        m = LINE_RE.search(line)
        if not m:
            print(line, flush=True)
            return
        now = time.monotonic() - self.t0
        values = {key: float(m.group(key)) for key, *_ in CHANNELS}
        with self.lock:
            self.t.append(now)
            for key, v in values.items():
                self.data[key].append(v)
            self.err = int(m.group("err"))
            self.ovf = bool(m.group("ovf"))
            self.samples += 1
            while self.t and now - self.t[0] > self.window_s:
                self.t.popleft()
                for d in self.data.values():
                    d.popleft()
        if self.csv:
            self.csv.writerow(["%.3f" % now] + [m.group(key) for key, *_ in CHANNELS] + [self.err, int(self.ovf)])
            self.csv_file.flush()

    def snapshot(self):
        with self.lock:
            return list(self.t), {k: list(v) for k, v in self.data.items()}, self.err, self.ovf, self.samples

    def close(self):
        self.stop.set()
        if self.csv_file:
            self.csv_file.close()


def main():
    ap = argparse.ArgumentParser(description="Real-time plot of the INA219 UART log (Vbus, Vshunt, I, P).")
    ap.add_argument("--port", help="serial port, e.g. COM16 (default: auto-detect the ST-Link VCP)")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--window", type=float, default=60.0, help="seconds shown on the plot (default 60)")
    ap.add_argument("--csv", help="also write every sample to this CSV file")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        sys.exit("No ST-Link VCP found; pass --port COMx")
    ser = serial.Serial(port, args.baud, timeout=0.2)
    print("Reading %s at %d baud" % (port, args.baud), flush=True)

    reader = Reader(ser, args.window, args.csv)
    reader.start()

    fig, axes = plt.subplots(len(CHANNELS), 1, sharex=True, figsize=(10, 8))
    fig.canvas.manager.set_window_title("INA219 - %s" % port)
    lines = {}
    for ax, (key, name, unit, _) in zip(axes, CHANNELS):
        (lines[key],) = ax.plot([], [], lw=1.2)
        ax.set_ylabel("%s (%s)" % (name, unit))
        ax.grid(True, alpha=0.3)
    axes[-1].set_xlabel("time (s)")
    fig.tight_layout(rect=(0, 0, 1, 0.96))

    def update(_):
        t, data, err, ovf, samples = reader.snapshot()
        if not t:
            fig.suptitle("waiting for data on %s ..." % port)
            return list(lines.values())
        for ax, (key, name, unit, fmt) in zip(axes, CHANNELS):
            y = data[key]
            lines[key].set_data(t, y)
            lo, hi = min(y), max(y)
            pad = max((hi - lo) * 0.1, abs(hi) * 0.002, 1e-3)
            ax.set_ylim(lo - pad, hi + pad)
            avg = sum(y) / len(y)
            ax.set_title(("%s = " + fmt + " %s    min " + fmt + "  avg " + fmt + "  max " + fmt)
                         % (name, y[-1], unit, lo, avg, hi), fontsize=9, loc="left")
        axes[-1].set_xlim(max(0.0, t[-1] - args.window), max(t[-1], args.window * 0.05))
        status = "samples=%d  err=%d" % (samples, err)
        if ovf:
            status += "  OVERFLOW"
        fig.suptitle(status, color="red" if err or ovf else "black")
        return list(lines.values())

    anim = FuncAnimation(fig, update, interval=50, cache_frame_data=False)
    try:
        plt.show()
    finally:
        reader.close()
        ser.close()
    del anim


if __name__ == "__main__":
    main()
