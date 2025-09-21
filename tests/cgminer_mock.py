#!/usr/bin/env python3
"""
Mock cgminer TCP API server.

Listens on localhost:<port>. Responds to:
  '{"command": "devs"}'   -> full JSON reply (STATUS + DEVS array)
  'devs' (plain text)     -> legacy text reply with '|' blocks

Generates realistic random data for ASIC devices with fields:
    id, Name, MHS av, Temperature, Utility, Accepted, Rejected, Hardware Errors
and some extra helpful fields (MHS 10s/1m/5m/15m, Total MH, Last Share Time).

Arguments:
  --host HOST   (default: 127.0.0.1)
  --port PORT   (default: 4028)
  --devices N   (default: 2)
  --min-temp T  (default: 40.0)
  --max-temp T  (default: 90.0)
  --seed S      (optional random seed)
"""
import argparse
import json
import random
import socket
import threading
import time
from socketserver import ThreadingMixIn, TCPServer, StreamRequestHandler
from datetime import datetime

# --- helpers to generate device data ---

ASIC_MODELS = [
    "Antminer S19", "Antminer S17", "AvalonMiner 1246", "Whatsminer M30S",
    "Whatsminer M21S", "Bitmain S9", "Innosilicon T3", "MicroBT M20S"
]

def gen_asic(i, min_temp, max_temp):
    """
    Generate a realistic asic device dictionary.
    """
    model = random.choice(ASIC_MODELS)
    dev_id = i
    name = f"{model}"
    # MHS in TH/s or GH/s? cgminer uses MHS (MH/s). For ASICs we keep MHS large.
    # Simulate as megahashes per second (MH/s). Example: 1000000 MH/s = 1 TH/s
    base_hash = random.uniform(100000.0, 1200000.0)  # MH/s range
    # small instantaneous variations for 10s/1m/5m/15m
    mhs_10s = base_hash * random.uniform(0.95, 1.05)
    mhs_1m  = base_hash * random.uniform(0.96, 1.04)
    mhs_5m  = base_hash * random.uniform(0.97, 1.03)
    mhs_15m = base_hash * random.uniform(0.98, 1.02)
    mhs_av  = base_hash

    temperature = round(random.uniform(min_temp, max_temp), 2)
    utility = round(random.uniform(0.5, 200.0), 3)  # arbitrary "utility" metric
    accepted = random.randint(0, 200000)
    rejected = random.randint(0, max(1, accepted // 500))
    hw_errors = random.randint(0, max(1, accepted // 20000))
    total_mh = int(base_hash * random.uniform(1000, 20000))  # total MH hashed

    last_share_time = int(time.time() - random.randint(0, 3600))
    last_valid_work = last_share_time + random.randint(0, 60)

    # Build dict following cgminer JSON convention keys
    dev = {
        "ASC": dev_id,
        "Name": name,
        "ID": dev_id,
        "MHS av": round(mhs_av, 2),
        "MHS 10s": round(mhs_10s, 2),
        "MHS 1m": round(mhs_1m, 2),
        "MHS 5m": round(mhs_5m, 2),
        "MHS 15m": round(mhs_15m, 2),
        "Temperature": temperature,
        "Utility": utility,
        "Accepted": accepted,
        "Rejected": rejected,
        "Hardware Errors": hw_errors,
        "Total MH": total_mh,
        "Last Share Time": last_share_time,
        "Last Valid Work": last_valid_work,
        # For compatibility include some cgminer-like fields:
        "Diff1 Work": random.randint(0, accepted + 1000),
        "Difficulty Accepted": random.uniform(0.0, float(max(1, accepted))),
        "Difficulty Rejected": random.uniform(0.0, float(max(1, rejected))),
        "Last Share Difficulty": random.uniform(1.0, 10000.0)
    }
    return dev

# --- server implementation ---

class ThreadedTCPServer(ThreadingMixIn, TCPServer):
    allow_reuse_address = True

class CGMinerMockHandler(StreamRequestHandler):
    def handle(self):
        # read request (client may not send newline)
        self.request.settimeout(1.0)
        try:
            raw = b''
            # read until socket closed by client or timeout
            while True:
                chunk = self.request.recv(4096)
                if not chunk:
                    break
                raw += chunk
                # if looks like complete JSON or short text guess we can break early
                if raw.strip().startswith(b'{') and raw.strip().endswith(b'}'):
                    break
                if b'\n' in raw:
                    break
            if not raw:
                return
            try:
                txt = raw.decode('utf-8').strip()
            except Exception:
                txt = raw.decode('latin1').strip()
        except socket.timeout:
            # nothing received
            return

        # detect json or plain
        is_json = txt.startswith('{')
        # parse if json to get command
        cmd = None
        param = None
        if is_json:
            try:
                j = json.loads(txt)
                cmd = j.get("command")
                param = j.get("parameter")
            except Exception:
                # bad json -> reply with error STATUS
                self.wfile.write(self._make_status_error("Invalid JSON").encode('utf-8'))
                return
        else:
            # plain text, commands may be joined with '|' e.g. "summary|devs"
            cmd = txt.split('|')[0].strip().lower()

        # Only handle 'devs' here; otherwise reply basic STATUS
        if cmd and cmd.lower() == "devs":
            self._handle_devs(is_json)
        else:
            # not implemented: reply an INFO STATUS
            if is_json:
                resp = {
                    "STATUS": [
                        {"STATUS": "E",
                         "When": int(time.time()),
                         "Code": 1,
                         "Msg": "Unknown command",
                         "Description": "mock-cgminer"}
                    ],
                    "id": 1
                }
                self.wfile.write((json.dumps(resp) + "\n").encode('utf-8'))
            else:
                self.wfile.write(self._make_status_error("Unknown command").encode('utf-8'))

    def _make_status_ok(self, msg="DEVS"):
        return "STATUS=S,When=%d,Code=9,Msg=%s,Description=mock-cgminer|" % (int(time.time()), msg)

    def _make_status_error(self, msg="Error"):
        return "STATUS=E,When=%d,Code=1,Msg=%s,Description=mock-cgminer|" % (int(time.time()), msg)

    def _handle_devs(self, is_json):
        # build few devices
        devs = []
        for i in range(self.server.num_devices):
            devs.append(gen_asic(i, self.server.min_temp, self.server.max_temp))

        if is_json:
            resp = {
                "STATUS": [
                    {"STATUS": "S",
                     "When": int(time.time()),
                     "Code": 9,
                     "Msg": "DEVS",
                     "Description": "mock-cgminer"}
                ],
                "DEVS": devs,
                "id": 1
            }
            payload = json.dumps(resp) + "\n"
            self.wfile.write(payload.encode('utf-8'))
        else:
            # text format: STATUS...|DEVS=...,field=val,...|DEVS=...|
            out = self._make_status_ok("DEVS")
            for d in devs:
                # flatten dictionary into key=val pairs (string values are not quoted)
                parts = []
                # try to include as many fields as possible in natural order
                order = ["ASC", "Name", "ID", "MHS av", "MHS 10s", "MHS 1m", "MHS 5m", "MHS 15m",
                         "Temperature", "Utility", "Accepted", "Rejected", "Hardware Errors",
                         "Total MH", "Last Share Time", "Last Valid Work"]
                for k in order:
                    if k in d:
                        v = d[k]
                        # format floats with reasonable precision
                        if isinstance(v, float):
                            parts.append(f"{k}={v:.2f}")
                        else:
                            parts.append(f"{k}={v}")
                out += "DEVS=" + ",".join(parts) + "|"
            self.wfile.write(out.encode('utf-8'))

# --- main / CLI ---

def run_server(host, port, num_devices, min_temp, max_temp):
    random.seed()  # system seed
    server = ThreadedTCPServer((host, port), CGMinerMockHandler)
    server.num_devices = num_devices
    server.min_temp = min_temp
    server.max_temp = max_temp

    print(f"mock-cgminer listening on {host}:{port} (devices={num_devices})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("shutting down mock-cgminer")
        server.shutdown()
        server.server_close()

def parse_args():
    p = argparse.ArgumentParser(description="Mock cgminer API server (devs)")
    p.add_argument("--host", default="127.0.0.1", help="listen address")
    p.add_argument("--port", type=int, default=4028, help="listen port")
    p.add_argument("--devices", type=int, default=2, help="number of ASIC devices to simulate")
    p.add_argument("--min-temp", type=float, default=40.0, help="minimum device temperature")
    p.add_argument("--max-temp", type=float, default=85.0, help="maximum device temperature")
    p.add_argument("--seed", type=int, default=None, help="random seed (optional)")
    return p.parse_args()

if __name__ == "__main__":
    args = parse_args()
    if args.seed is not None:
        random.seed(args.seed)
    run_server(args.host, args.port, args.devices, args.min_temp, args.max_temp)
