/* cgmonitor frontend — vanilla ES module, no deps.
 *
 * Polls /api/snapshot at user-configurable interval and renders the data.
 * Reads /api/config once on load to pick up the temperature thresholds.
 */

const $ = (id) => document.getElementById(id);

const els = {
  dot:        $("dot"),
  fwVersion:  $("fw-version"),
  period:     $("period"),
  pause:      $("pause"),
  last:       $("last"),

  hashrate:   $("kpi-hashrate"),
  h5m:        $("kpi-h5m"),
  h1h:        $("kpi-h1h"),
  acc:        $("kpi-acc"),
  rej:        $("kpi-rej"),
  rejPct:     $("kpi-rej-pct"),
  hwe:        $("kpi-hwe"),
  hwePct:     $("kpi-hwe-pct"),
  power:      $("kpi-power"),
  eff:        $("kpi-eff"),
  uptime:     $("kpi-uptime"),
  activePool: $("kpi-active-pool"),

  tBoards:    document.querySelector("#t-boards tbody"),
  emptyBoards:$("empty-boards"),
  fansWrap:   $("fans-wrap"),
  emptyFans:  $("empty-fans"),
  tPools:     document.querySelector("#t-pools tbody"),
  emptyPools: $("empty-pools"),
};

const state = {
  periodMs:   parseInt(localStorage.getItem("cgmon.period") || "1000", 10),
  paused:     false,
  thresholds: { warn: 70, crit: 85 },
  failures:   0,
  timer:      null,
};

/* ---- formatting helpers ---------------------------------------------- */

function fmtNum(v, frac = 0) {
  if (v == null || !isFinite(v)) return "—";
  return Number(v).toLocaleString(undefined, {
    minimumFractionDigits: frac,
    maximumFractionDigits: frac,
  });
}

function fmtHashrate(ghs) {
  if (!ghs || !isFinite(ghs)) return { value: "—", unit: "" };
  if (ghs >= 1_000_000) return { value: fmtNum(ghs / 1_000_000, 2), unit: "PH/s" };
  if (ghs >= 1_000)     return { value: fmtNum(ghs / 1_000, 2),     unit: "TH/s" };
  if (ghs >= 1)         return { value: fmtNum(ghs, 1),             unit: "GH/s" };
  return { value: fmtNum(ghs * 1000, 0), unit: "MH/s" };
}

function fmtUptime(sec) {
  if (!sec || sec < 0) return "—";
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
}

function tempClass(c) {
  if (c == null || c <= 0) return "muted";
  if (c >= state.thresholds.crit) return "t-crit";
  if (c >= state.thresholds.warn) return "t-warn";
  return "t-ok";
}

/* ---- DOM updates ----------------------------------------------------- */

function setDot(cls) {
  els.dot.classList.remove("live", "stale", "dead");
  if (cls) els.dot.classList.add(cls);
}

function setEmpty(el, isEmpty) {
  el.classList.toggle("show", isEmpty);
}

function render(snap) {
  els.fwVersion.textContent = snap.fw_version || "";

  /* main hashrate */
  const hr = fmtHashrate(snap.miner.hashrate_5s_ghs);
  els.hashrate.textContent = `${hr.value} ${hr.unit}`.trim();

  const hr5m = fmtHashrate(snap.miner.hashrate_5m_ghs);
  const hr1h = fmtHashrate(snap.miner.hashrate_1h_ghs);
  els.h5m.textContent = `${hr5m.value} ${hr5m.unit}`.trim();
  els.h1h.textContent = `${hr1h.value} ${hr1h.unit}`.trim();

  /* shares */
  els.acc.textContent = fmtNum(snap.miner.shares_accepted);
  els.rej.textContent = fmtNum(snap.miner.shares_rejected);
  els.rejPct.textContent = `(${(snap.miner.shares_rejected_pct ?? 0).toFixed(2)}%)`;

  /* hw err */
  els.hwe.textContent = fmtNum(snap.miner.hw_errors);
  els.hwePct.textContent = `${(snap.miner.hw_errors_pct ?? 0).toFixed(3)}%`;

  /* power + efficiency.
   * Backend stores J/GHs; for modern miners that's < 1, so we convert
   * to the more readable J/TH (= J/GHs × 1000) once hashrate crosses 1 TH/s. */
  els.power.textContent = snap.miner.power_w > 0 ? `${fmtNum(snap.miner.power_w, 0)} W` : "—";
  const eff = snap.miner.efficiency_j_per_ghs;
  if (eff > 0) {
    if (snap.miner.hashrate_5s_ghs >= 1000) {
      els.eff.textContent = `${(eff * 1000).toFixed(1)} J/TH`;
    } else {
      els.eff.textContent = `${eff.toFixed(2)} J/GH`;
    }
  } else {
    els.eff.textContent = "—";
  }

  /* uptime + active pool */
  els.uptime.textContent = fmtUptime(snap.miner.uptime_sec);
  const ap = snap.pools && snap.pools.find(p => p.active);
  els.activePool.textContent = ap
    ? `pool #${ap.index}: ${shortUrl(ap.url)}`
    : "no active pool";

  /* hashboards */
  renderBoards(snap.hashboards || []);
  /* fans */
  renderFans(snap.fans || []);
  /* pools */
  renderPools(snap.pools || []);
}

function shortUrl(url) {
  if (!url) return "";
  return url.replace(/^stratum\+tcp:\/\//, "").replace(/^stratum:\/\//, "");
}

function renderBoards(boards) {
  setEmpty(els.emptyBoards, boards.length === 0);
  els.tBoards.innerHTML = boards.map(b => {
    const chipCls = tempClass(b.chip_temp_c);
    const pcbCls  = tempClass(b.pcb_temp_c);
    const status = b.status || "unknown";
    const chips  = b.chips_total > 0
      ? `${b.chips_active}/${b.chips_total}`
      : (b.chips_active > 0 ? String(b.chips_active) : "—");
    return `<tr>
      <td>${b.index}</td>
      <td><span class="pill ${status}">${status}</span></td>
      <td class="${chipCls}">${b.chip_temp_c ? b.chip_temp_c.toFixed(1) : "—"}</td>
      <td class="${pcbCls}">${b.pcb_temp_c ? b.pcb_temp_c.toFixed(1) : "—"}</td>
      <td>${b.chip_frequency_mhz || "—"}</td>
      <td>${chips}</td>
    </tr>`;
  }).join("");
}

function renderFans(fans) {
  setEmpty(els.emptyFans, fans.length === 0);
  els.fansWrap.innerHTML = fans.map(f => `
    <div class="fan">
      <div class="num">FAN ${f.index}</div>
      <div class="rpm">${fmtNum(f.rpm)}</div>
      <div class="lbl">RPM</div>
    </div>
  `).join("");
}

function renderPools(pools) {
  setEmpty(els.emptyPools, pools.length === 0);
  els.tPools.innerHTML = pools.map(p => {
    const status = p.status || "unknown";
    return `<tr>
      <td>${p.index}</td>
      <td title="${escapeHtml(p.url)}">${escapeHtml(shortUrl(p.url))}</td>
      <td><span class="pill ${status}">${status}</span></td>
      <td>${p.active ? "<span class='pill alive'>yes</span>" : "<span class='pill off'>no</span>"}</td>
      <td>${p.latency_ms >= 0 ? p.latency_ms + " ms" : "—"}</td>
      <td>${fmtNum(p.accepted)}</td>
      <td>${fmtNum(p.rejected)}</td>
      <td>${fmtNum(p.stale)}</td>
    </tr>`;
  }).join("");
}

function escapeHtml(s) {
  if (!s) return "";
  return s.replace(/[&<>"']/g, ch => ({
    "&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#39;"
  }[ch]));
}

/* ---- polling -------------------------------------------------------- */

async function loadConfig() {
  try {
    const r = await fetch("/api/config", { cache: "no-store" });
    if (!r.ok) return;
    const c = await r.json();
    if (typeof c.temp_warn_c === "number") state.thresholds.warn = c.temp_warn_c;
    if (typeof c.temp_crit_c === "number") state.thresholds.crit = c.temp_crit_c;
  } catch (e) { /* keep defaults */ }
}

async function tick() {
  if (state.paused) return;
  try {
    const r = await fetch("/api/snapshot", { cache: "no-store" });
    if (!r.ok) throw new Error("HTTP " + r.status);
    const snap = await r.json();
    state.failures = 0;
    setDot(snap.has_data ? "live" : "stale");
    if (snap.has_data) {
      render(snap);
      const t = new Date(snap.timestamp * 1000);
      els.last.textContent = t.toLocaleTimeString();
    } else {
      els.last.textContent = "waiting…";
    }
  } catch (e) {
    state.failures++;
    if (state.failures >= 3)      setDot("dead");
    else if (state.failures >= 1) setDot("stale");
    els.last.textContent = "err";
  }
}

function restartTimer() {
  if (state.timer) clearInterval(state.timer);
  if (state.paused) return;
  /* clamp to a sensible range — sub-50ms polling makes browsers cry */
  const p = Math.max(50, Math.min(60000, state.periodMs));
  state.timer = setInterval(tick, p);
  tick();
}

els.period.addEventListener("change", () => {
  state.periodMs = parseInt(els.period.value, 10);
  localStorage.setItem("cgmon.period", String(state.periodMs));
  restartTimer();
});
els.pause.addEventListener("click", () => {
  state.paused = !state.paused;
  els.pause.textContent = state.paused ? "resume" : "pause";
  if (state.paused) {
    if (state.timer) { clearInterval(state.timer); state.timer = null; }
    setDot("");
  } else {
    restartTimer();
  }
});

/* init */
els.period.value = String(state.periodMs);
loadConfig().then(restartTimer);
