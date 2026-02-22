// ═══════════════════════════════════════════════════
// AMG8833 Web Thermal Camera — Browser Application
// ═══════════════════════════════════════════════════

(function() {
'use strict';

// ── Ironbow Colormap ────────────────────────────────
// 256-entry LUT: each entry [R, G, B]
const IRONBOW = buildIronbow();

function buildIronbow() {
  // Key stops for ironbow palette (t: 0-1, r,g,b: 0-255)
  const stops = [
    { t: 0.00, r:   0, g:   0, b:   0 },
    { t: 0.10, r:  32, g:   0, b:  64 },
    { t: 0.25, r:  96, g:   0, b: 128 },
    { t: 0.40, r: 192, g:   0, b:  64 },
    { t: 0.50, r: 220, g:  40, b:   0 },
    { t: 0.60, r: 255, g: 100, b:   0 },
    { t: 0.70, r: 255, g: 160, b:   0 },
    { t: 0.80, r: 255, g: 220, b:  40 },
    { t: 0.90, r: 255, g: 255, b: 128 },
    { t: 1.00, r: 255, g: 255, b: 255 },
  ];
  const lut = new Array(256);
  for (let i = 0; i < 256; i++) {
    const t = i / 255;
    let lo = stops[0], hi = stops[stops.length - 1];
    for (let s = 0; s < stops.length - 1; s++) {
      if (t >= stops[s].t && t <= stops[s + 1].t) {
        lo = stops[s];
        hi = stops[s + 1];
        break;
      }
    }
    const f = (hi.t === lo.t) ? 0 : (t - lo.t) / (hi.t - lo.t);
    lut[i] = [
      Math.round(lo.r + f * (hi.r - lo.r)),
      Math.round(lo.g + f * (hi.g - lo.g)),
      Math.round(lo.b + f * (hi.b - lo.b)),
    ];
  }
  return lut;
}

// ── Interpolation ───────────────────────────────────

function interpNearest(src, srcW, srcH, dstW, dstH) {
  const dst = new Float32Array(dstW * dstH);
  const sx = srcW / dstW, sy = srcH / dstH;
  for (let y = 0; y < dstH; y++) {
    const iy = Math.min(Math.floor(y * sy), srcH - 1);
    for (let x = 0; x < dstW; x++) {
      const ix = Math.min(Math.floor(x * sx), srcW - 1);
      dst[y * dstW + x] = src[iy * srcW + ix];
    }
  }
  return dst;
}

function interpBilinear(src, srcW, srcH, dstW, dstH) {
  const dst = new Float32Array(dstW * dstH);
  const sx = (srcW - 1) / (dstW - 1);
  const sy = (srcH - 1) / (dstH - 1);
  for (let y = 0; y < dstH; y++) {
    const fy = y * sy;
    const iy = Math.floor(fy);
    const dy = fy - iy;
    const y0 = Math.min(iy, srcH - 1);
    const y1 = Math.min(iy + 1, srcH - 1);
    for (let x = 0; x < dstW; x++) {
      const fx = x * sx;
      const ix = Math.floor(fx);
      const dx = fx - ix;
      const x0 = Math.min(ix, srcW - 1);
      const x1 = Math.min(ix + 1, srcW - 1);
      const v =
        src[y0 * srcW + x0] * (1 - dx) * (1 - dy) +
        src[y0 * srcW + x1] * dx * (1 - dy) +
        src[y1 * srcW + x0] * (1 - dx) * dy +
        src[y1 * srcW + x1] * dx * dy;
      dst[y * dstW + x] = v;
    }
  }
  return dst;
}

function cubicWeight(t) {
  // Catmull-Rom
  const at = Math.abs(t);
  if (at <= 1) return 1.5 * at * at * at - 2.5 * at * at + 1;
  if (at <= 2) return -0.5 * at * at * at + 2.5 * at * at - 4 * at + 2;
  return 0;
}

function interpBicubic(src, srcW, srcH, dstW, dstH) {
  const dst = new Float32Array(dstW * dstH);
  const sx = (srcW - 1) / (dstW - 1);
  const sy = (srcH - 1) / (dstH - 1);

  for (let y = 0; y < dstH; y++) {
    const fy = y * sy;
    const iy = Math.floor(fy);
    const dy = fy - iy;

    for (let x = 0; x < dstW; x++) {
      const fx = x * sx;
      const ix = Math.floor(fx);
      const dx = fx - ix;

      let val = 0;
      for (let m = -1; m <= 2; m++) {
        const cy = Math.min(Math.max(iy + m, 0), srcH - 1);
        const wy = cubicWeight(m - dy);
        for (let n = -1; n <= 2; n++) {
          const cx = Math.min(Math.max(ix + n, 0), srcW - 1);
          val += src[cy * srcW + cx] * wy * cubicWeight(n - dx);
        }
      }
      dst[y * dstW + x] = val;
    }
  }
  return dst;
}

const INTERP_FN = {
  nearest:  interpNearest,
  bilinear: interpBilinear,
  bicubic:  interpBicubic,
};

// ── Canvas rendering ────────────────────────────────

const heatmapCanvas = document.getElementById('heatmap');
const heatmapCtx    = heatmapCanvas.getContext('2d');
const legendCanvas  = document.getElementById('legend');
const legendCtx     = legendCanvas.getContext('2d');

let currentResolution = 24;

function renderHeatmap(pixels8x8, scaleMin, scaleMax, hotspotX, hotspotY) {
  const res = currentResolution;
  const interpFn = INTERP_FN[document.getElementById('ctrl-interp').value] || interpBilinear;
  const grid = interpFn(pixels8x8, 8, 8, res, res);

  heatmapCanvas.width  = res;
  heatmapCanvas.height = res;

  const imgData = heatmapCtx.createImageData(res, res);
  const d = imgData.data;
  const range = scaleMax - scaleMin;
  const invRange = range > 0.01 ? 1.0 / range : 1.0;

  for (let i = 0; i < res * res; i++) {
    const norm = Math.max(0, Math.min(1, (grid[i] - scaleMin) * invRange));
    const idx = Math.round(norm * 255);
    const c = IRONBOW[idx];
    const p = i * 4;
    d[p]     = c[0];
    d[p + 1] = c[1];
    d[p + 2] = c[2];
    d[p + 3] = 255;
  }
  heatmapCtx.putImageData(imgData, 0, 0);

  // Hotspot marker
  if (document.getElementById('ctrl-hotspot').checked) {
    const sx = res / 8;
    const cx = (hotspotX + 0.5) * sx;
    const cy = (hotspotY + 0.5) * sx;
    const r  = sx * 0.4;
    // Pick black or white based on hotspot pixel brightness
    var pIdx = (Math.round(cy) * res + Math.round(cx)) * 4;
    var lum = imgData.data[pIdx] * 0.299 + imgData.data[pIdx+1] * 0.587 + imgData.data[pIdx+2] * 0.114;
    heatmapCtx.strokeStyle = lum > 128 ? '#000' : '#fff';
    heatmapCtx.lineWidth = Math.max(1, res / 48);
    heatmapCtx.beginPath();
    // Crosshair
    heatmapCtx.moveTo(cx - r, cy); heatmapCtx.lineTo(cx + r, cy);
    heatmapCtx.moveTo(cx, cy - r); heatmapCtx.lineTo(cx, cy + r);
    heatmapCtx.stroke();
  }
}

function renderLegend(scaleMin, scaleMax) {
  const w = 30, h = 200;
  legendCanvas.width  = w;
  legendCanvas.height = h;

  const barW = 14;
  const imgData = legendCtx.createImageData(barW, h);
  const d = imgData.data;

  for (let y = 0; y < h; y++) {
    const norm = 1.0 - y / (h - 1);  // top=hot, bottom=cold
    const idx = Math.round(norm * 255);
    const c = IRONBOW[idx];
    for (let x = 0; x < barW; x++) {
      const p = (y * barW + x) * 4;
      d[p]     = c[0];
      d[p + 1] = c[1];
      d[p + 2] = c[2];
      d[p + 3] = 255;
    }
  }
  legendCtx.putImageData(imgData, 0, 0);

  // Labels
  legendCtx.fillStyle = '#eee';
  legendCtx.font = '10px sans-serif';
  legendCtx.textAlign = 'left';
  legendCtx.fillText(scaleMax.toFixed(1), barW + 2, 10);
  legendCtx.fillText(((scaleMax + scaleMin) / 2).toFixed(1), barW + 2, h / 2 + 4);
  legendCtx.fillText(scaleMin.toFixed(1), barW + 2, h - 2);
}

// ── Autoscale ───────────────────────────────────────

let scaleMin = 15, scaleMax = 40;
let prevScaleMin = 15, prevScaleMax = 40;

function updateScale(tmin, tmax) {
  if (document.getElementById('ctrl-autoscale').checked) {
    const beta = parseFloat(document.getElementById('ctrl-smoothing').value);
    scaleMin = beta * tmin + (1 - beta) * prevScaleMin;
    scaleMax = beta * tmax + (1 - beta) * prevScaleMax;
    prevScaleMin = scaleMin;
    prevScaleMax = scaleMax;
  } else {
    scaleMin = parseFloat(document.getElementById('ctrl-scale-min').value);
    scaleMax = parseFloat(document.getElementById('ctrl-scale-max').value);
  }
}

// ── WebSocket ───────────────────────────────────────

let ws = null;
let reconnectTimer = null;
const RECONNECT_DELAY = 2000;

function wsConnect() {
  const banner = document.getElementById('conn-banner');
  banner.textContent = 'Connecting...';
  banner.className = 'banner connecting';

  const host = window.location.hostname || '192.168.4.1';
  const url = 'ws://' + host + '/ws';

  ws = new WebSocket(url);
  ws.binaryType = 'arraybuffer';

  ws.onopen = function() {
    banner.textContent = 'Connected';
    banner.className = 'banner connected';
    clearTimeout(reconnectTimer);
    // Load current config from server
    loadConfig();
  };

  ws.onmessage = function(evt) {
    if (!(evt.data instanceof ArrayBuffer)) return;
    parseFrame(evt.data);
  };

  ws.onclose = function() {
    banner.textContent = 'Disconnected';
    banner.className = 'banner disconnected';
    scheduleReconnect();
  };

  ws.onerror = function() {
    ws.close();
  };
}

function scheduleReconnect() {
  clearTimeout(reconnectTimer);
  reconnectTimer = setTimeout(wsConnect, RECONNECT_DELAY);
}

// ── Binary frame parser ─────────────────────────────

// Payload layout (packed, little-endian):
// offset  size  field
// 0       4     timestamp_ms   (uint32)
// 4       1     current_fps    (uint8)
// 5       1     flags          (uint8)
// 6       4     calibration_offset (float32)
// 10      4     tmin           (float32)
// 14      4     tmax           (float32)
// 18      4     tmean          (float32)
// 22      1     hotspot_x      (uint8)
// 23      1     hotspot_y      (uint8)
// 24      256   pixels[64]     (float32 x 64)
// Total: 280 bytes

const PAYLOAD_SIZE = 280;

function parseFrame(buf) {
  if (buf.byteLength !== PAYLOAD_SIZE) return;

  const dv = new DataView(buf);
  let off = 0;

  const timestamp = dv.getUint32(off, true);   off += 4;
  const fps       = dv.getUint8(off);           off += 1;
  const flags     = dv.getUint8(off);           off += 1;
  const calOffset = dv.getFloat32(off, true);   off += 4;
  const tmin      = dv.getFloat32(off, true);   off += 4;
  const tmax      = dv.getFloat32(off, true);   off += 4;
  const tmean     = dv.getFloat32(off, true);   off += 4;
  const hotX      = dv.getUint8(off);           off += 1;
  const hotY      = dv.getUint8(off);           off += 1;

  const pixels = new Float32Array(64);
  for (let i = 0; i < 64; i++) {
    pixels[i] = dv.getFloat32(off, true);
    off += 4;
  }

  const temporalOn = !!(flags & 0x01);
  const idleActive = !!(flags & 0x02);

  // Update stats display
  document.getElementById('stat-min').textContent = tmin.toFixed(1) + '\u00b0C';
  document.getElementById('stat-mean').textContent = tmean.toFixed(1) + '\u00b0C';
  document.getElementById('stat-max').textContent = tmax.toFixed(1) + '\u00b0C';
  document.getElementById('stat-hotspot').textContent = '(' + hotX + ',' + hotY + ')';
  document.getElementById('stat-fps').textContent = fps;
  document.getElementById('stat-status').textContent = idleActive ? 'Idle' : 'Active';

  // Update scale and render
  updateScale(tmin, tmax);
  renderHeatmap(pixels, scaleMin, scaleMax, hotX, hotY);
  renderLegend(scaleMin, scaleMax);
}

// ── Config API ──────────────────────────────────────

function loadConfig() {
  fetch('/api/config')
    .then(function(r) { return r.json(); })
    .then(function(cfg) {
      document.getElementById('ctrl-fps').value         = cfg.normal_fps;
      document.getElementById('ctrl-idle-timeout').value = cfg.idle_timeout_sec;
      document.getElementById('ctrl-offset').value       = cfg.calibration_offset;
      document.getElementById('val-offset').textContent  = cfg.calibration_offset.toFixed(1);
      document.getElementById('ctrl-temporal').checked   = cfg.temporal_enabled;
      document.getElementById('ctrl-alpha').value        = cfg.alpha;
      document.getElementById('val-alpha').textContent   = cfg.alpha.toFixed(2);
      document.getElementById('ctrl-sta-enabled').checked = cfg.sta_enabled;
      document.getElementById('ctrl-sta-ssid').value     = cfg.sta_ssid || '';
      document.getElementById('sta-ip').textContent      = cfg.sta_ip || '--';

      toggleAlphaRow();
      toggleStaFields();
    })
    .catch(function(e) {
      console.error('Config load failed:', e);
    });
}

function sendConfig(obj, callback) {
  fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(obj),
  }).then(function(r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  }).then(function(data) {
    console.log('Config saved:', data);
    if (callback) callback(true);
  }).catch(function(e) {
    console.error('Config send failed:', e);
    if (callback) callback(false);
  });
}

// ── UI event handlers ───────────────────────────────

// Visualization (local only)
document.getElementById('ctrl-interp').addEventListener('change', function() {});

document.getElementById('ctrl-resolution').addEventListener('change', function() {
  currentResolution = parseInt(this.value);
});

document.getElementById('ctrl-autoscale').addEventListener('change', function() {
  document.getElementById('row-smoothing').style.display   = this.checked ? '' : 'none';
  document.getElementById('row-manual-scale').style.display = this.checked ? 'none' : '';
  if (this.checked) {
    prevScaleMin = scaleMin;
    prevScaleMax = scaleMax;
  }
});

document.getElementById('ctrl-smoothing').addEventListener('input', function() {
  document.getElementById('val-smoothing').textContent = parseFloat(this.value).toFixed(2);
});

// Sensor controls (send to ESP)
document.getElementById('ctrl-fps').addEventListener('change', function() {
  sendConfig({ normal_fps: parseInt(this.value) });
});

document.getElementById('ctrl-idle-timeout').addEventListener('change', function() {
  sendConfig({ idle_timeout_sec: parseInt(this.value) });
});

document.getElementById('ctrl-offset').addEventListener('input', function() {
  document.getElementById('val-offset').textContent = parseFloat(this.value).toFixed(1);
});
document.getElementById('ctrl-offset').addEventListener('change', function() {
  sendConfig({ calibration_offset: parseFloat(this.value) });
});

document.getElementById('ctrl-temporal').addEventListener('change', function() {
  sendConfig({ temporal_enabled: this.checked });
  toggleAlphaRow();
});

function toggleAlphaRow() {
  document.getElementById('row-alpha').style.display =
    document.getElementById('ctrl-temporal').checked ? '' : 'none';
}

document.getElementById('ctrl-alpha').addEventListener('input', function() {
  document.getElementById('val-alpha').textContent = parseFloat(this.value).toFixed(2);
});
document.getElementById('ctrl-alpha').addEventListener('change', function() {
  sendConfig({ alpha: parseFloat(this.value) });
});

// WiFi STA
document.getElementById('ctrl-sta-enabled').addEventListener('change', function() {
  toggleStaFields();
});

function toggleStaFields() {
  document.getElementById('sta-fields').style.display =
    document.getElementById('ctrl-sta-enabled').checked ? '' : 'none';
}

document.getElementById('btn-sta-save').addEventListener('click', function() {
  var btn = this;
  var origText = btn.textContent;
  btn.textContent = 'Saving...';
  btn.disabled = true;

  sendConfig({
    sta_enabled:  document.getElementById('ctrl-sta-enabled').checked,
    sta_ssid:     document.getElementById('ctrl-sta-ssid').value,
    sta_password: document.getElementById('ctrl-sta-pass').value,
  }, function(ok) {
    if (ok) {
      btn.textContent = 'Saved! Connecting...';
      // Poll config after a few seconds to get the STA IP
      setTimeout(function() { loadConfig(); btn.textContent = origText; btn.disabled = false; }, 5000);
      setTimeout(function() { loadConfig(); }, 10000);
      setTimeout(function() { loadConfig(); }, 15000);
    } else {
      btn.textContent = 'Error! Try again';
      setTimeout(function() { btn.textContent = origText; btn.disabled = false; }, 2000);
    }
  });
});

// ── Init ────────────────────────────────────────────

wsConnect();

})();
