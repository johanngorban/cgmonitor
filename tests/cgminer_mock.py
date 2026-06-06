#!/usr/bin/env python3
"""
cgminer_mock.py — a tiny TCP server that imitates cgminer's RPC.

Accepts one JSON request per connection (cgminer's classic 'request + close'
pattern) and responds with realistic-looking randomized data. Use for
end-to-end testing without a real miner.

Usage:
    python3 cgminer_mock.py [--port 4028] [--boards 3] [--fans 4] [--pools 2]
"""
import argparse
import json
import random
import socket
import threading
import time

START_TS = time.time()

def make_summary():
    hashrate_mhs = random.gauss(110_000_000, 1_500_000)   # ~110 TH/s
    accepted = random.randint(100_000, 200_000)
    rejected = int(accepted * random.uniform(0.001, 0.005))
    return {
        "STATUS": [{"STATUS": "S", "When": int(time.time()), "Code": 11,
                    "Msg": "Summary", "Description": "cgminer-mock 1.0"}],
        "SUMMARY": [{
            "Elapsed":           int(time.time() - START_TS),
            "MHS av":            hashrate_mhs,
            "MHS 5s":            hashrate_mhs * random.uniform(0.95, 1.05),
            "MHS 5m":            hashrate_mhs * random.uniform(0.98, 1.02),
            "MHS 1h":            hashrate_mhs * random.uniform(0.97, 1.03),
            "Accepted":          accepted,
            "Rejected":          rejected,
            "Hardware Errors":   random.randint(0, 30),
            "Utility":           random.uniform(40, 60),
            "Discarded":         random.randint(0, 500),
            "Stale":             random.randint(0, 5),
            "Get Failures":      0,
            "Local Work":        random.randint(100_000_000, 500_000_000),
            "Remote Failures":   0,
            "Network Blocks":    random.randint(100, 1000),
            "Total MH":          random.uniform(1e12, 1e13),
            "Work Utility":      random.uniform(40, 60),
            "Difficulty Accepted": accepted * 65536,
            "Difficulty Rejected": rejected * 65536,
            "Difficulty Stale":    0,
        }],
        "id": 1,
    }

def make_stats(n_boards, n_fans):
    out = {}
    out["STATS"] = 0
    out["ID"] = "BC50"
    out["Elapsed"] = int(time.time() - START_TS)
    out["Calls"] = 0
    out["Wait"] = 0.0
    out["Max"] = 0.0
    out["Min"] = 99999999.0
    out["GHS 5s"] = random.uniform(100_000, 120_000)
    out["GHS av"] = random.uniform(100_000, 120_000)
    out["Mode"] = "Normal"
    out["miner_version"] = "mock-1.0.0"
    out["Firmware"] = "cgmonitor-mock-fw"
    out["Power"] = random.uniform(3000, 3500)
    out["fan_num"] = n_fans
    out["temp_num"] = n_boards

    for i in range(1, n_fans + 1):
        out[f"fan{i}"] = random.randint(3500, 5500)

    chips_per_board = 76
    for i in range(1, n_boards + 1):
        out[f"chain_acn{i}"]     = chips_per_board
        out[f"chain_acs{i}"]     = "o" * chips_per_board
        out[f"temp{i}"]          = round(random.gauss(72, 4), 1)
        out[f"temp2_{i}"]        = round(random.gauss(58, 3), 1)
        out[f"freq_avg{i}"]      = 700 + random.randint(-20, 20)
        out[f"chain_rate{i}"]    = round(random.uniform(35_000, 40_000), 2)
        out[f"chain_hw{i}"]      = random.randint(0, 10)

    return {
        "STATUS": [{"STATUS": "S", "When": int(time.time()), "Code": 70,
                    "Msg": "STATS", "Description": "cgminer-mock 1.0"}],
        "STATS": [out],
        "id": 1,
    }

def make_pools(n_pools):
    pools = []
    for i in range(n_pools):
        active = (i == 0)
        pools.append({
            "POOL": i,
            "URL": f"stratum+tcp://pool{i + 1}.example.com:3333",
            "Status":           "Alive" if random.random() > 0.05 else "Dead",
            "Priority":         i,
            "Quota":            1,
            "Long Poll":        "N",
            "Getworks":         random.randint(100, 1000),
            "Accepted":         random.randint(10_000, 50_000) if active else 0,
            "Rejected":         random.randint(0, 100)         if active else 0,
            "Works":            random.randint(0, 10000),
            "Discarded":        0,
            "Stale":            random.randint(0, 10)          if active else 0,
            "Get Failures":     0,
            "Remote Failures":  0,
            "User":             f"worker{i + 1}.miner1",
            "Last Share Time":  int(time.time()) - random.randint(1, 60),
            "Diff":             "65.5K",
            "Diff1 Shares":     random.randint(1000, 10000),
            "Proxy Type":       "",
            "Proxy":            "",
            "Difficulty Accepted": random.uniform(1e9, 1e10),
            "Difficulty Rejected": random.uniform(0, 1e7),
            "Difficulty Stale":    0,
            "Last Share Difficulty": 65536,
            "Has Stratum":      True,
            "Stratum Active":   "true" if active else "false",
            "Stratum URL":      f"pool{i + 1}.example.com",
            "Stratum Latency":  random.randint(10, 80) if active else -1,
            "Has GBT":          False,
        })
    return {
        "STATUS": [{"STATUS": "S", "When": int(time.time()), "Code": 7,
                    "Msg": "Pools", "Description": "cgminer-mock 1.0"}],
        "POOLS": pools,
        "id": 1,
    }

def make_devs(n_boards):
    devs = []
    for i in range(n_boards):
        alive = random.random() > 0.02
        devs.append({
            "ASC":              i,
            "ID":               i,
            "Name":             "BC50",
            "Slot":             i,
            "Enabled":          "Y",
            "Status":           "Alive" if alive else "Sick",
            "Temperature":      round(random.gauss(72, 4), 1),
            "MHS av":           random.uniform(35_000_000, 40_000_000),
            "MHS 5s":           random.uniform(35_000_000, 40_000_000),
            "MHS 1m":           random.uniform(35_000_000, 40_000_000),
            "MHS 5m":           random.uniform(35_000_000, 40_000_000),
            "MHS 15m":          random.uniform(35_000_000, 40_000_000),
            "Accepted":         random.randint(1000, 10000),
            "Rejected":         random.randint(0, 50),
            "Hardware Errors":  random.randint(0, 10),
            "Utility":          random.uniform(10, 20),
            "Last Share Pool":  0,
            "Last Share Time":  int(time.time()) - random.randint(1, 60),
            "Total MH":         random.uniform(1e10, 1e11),
            "Diff1 Work":       random.uniform(1e8, 1e9),
            "Difficulty Accepted": random.uniform(1e8, 1e9),
            "Difficulty Rejected": random.uniform(0, 1e6),
            "Last Share Difficulty": 65536,
            "Last Valid Work":  int(time.time()),
            "Device Hardware%": round(random.uniform(0.001, 0.01), 4),
            "Device Rejected%": round(random.uniform(0.01, 0.1), 4),
            "Device Elapsed":   int(time.time() - START_TS),
        })
    return {
        "STATUS": [{"STATUS": "S", "When": int(time.time()), "Code": 9,
                    "Msg": "Devs", "Description": "cgminer-mock 1.0"}],
        "DEVS": devs,
        "id": 1,
    }

def handle(conn, n_boards, n_fans, n_pools):
    try:
        data = b""
        conn.settimeout(2.0)
        while True:
            chunk = conn.recv(1024)
            if not chunk:
                break
            data += chunk
            if data.endswith(b"\n") or len(data) > 2:
                break

        try:
            req = json.loads(data.decode("utf-8", errors="ignore"))
            cmd = req.get("command", "").lower()
        except Exception:
            cmd = ""

        if cmd == "summary":
            resp = make_summary()
        elif cmd == "stats":
            resp = make_stats(n_boards, n_fans)
        elif cmd == "pools":
            resp = make_pools(n_pools)
        elif cmd == "devs":
            resp = make_devs(n_boards)
        elif cmd == "version":
            resp = {
                "STATUS": [{"STATUS": "S", "Code": 22, "Msg": "CGMiner versions",
                            "When": int(time.time())}],
                "VERSION": [{"CGMiner": "mock-1.0.0", "API": "3.7"}],
                "id": 1,
            }
        else:
            resp = {"STATUS": [{"STATUS": "E", "Code": 14, "Msg": f"unknown cmd '{cmd}'",
                                 "When": int(time.time())}], "id": 1}

        body = json.dumps(resp).encode("utf-8")
        conn.sendall(body)
    finally:
        try: conn.shutdown(socket.SHUT_WR)
        except Exception: pass
        conn.close()

def serve(port, n_boards, n_fans, n_pools):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", port))
    s.listen(16)
    print(f"cgminer-mock listening on 0.0.0.0:{port} "
          f"(boards={n_boards} fans={n_fans} pools={n_pools})")
    try:
        while True:
            conn, _ = s.accept()
            t = threading.Thread(target=handle, args=(conn, n_boards, n_fans, n_pools),
                                 daemon=True)
            t.start()
    except KeyboardInterrupt:
        print("\nshutting down")
    finally:
        s.close()

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port",  type=int, default=4028)
    p.add_argument("--boards", type=int, default=3)
    p.add_argument("--fans",   type=int, default=4)
    p.add_argument("--pools",  type=int, default=2)
    a = p.parse_args()
    serve(a.port, a.boards, a.fans, a.pools)

if __name__ == "__main__":
    main()
