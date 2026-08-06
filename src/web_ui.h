#pragma once

#include <Arduino.h>

const char WEB_UI_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="th">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#101820">
  <title>6DOF Robot Arm</title>
  <style>
    :root {
      color-scheme: light;
      --ink: #14202b;
      --muted: #66727d;
      --line: #dce2e6;
      --panel: #ffffff;
      --canvas: #f3f5f6;
      --nav: #101820;
      --nav-soft: #1d2933;
      --accent: #00a67e;
      --accent-dark: #007e61;
      --warning: #f2a93b;
      --danger: #d53c3c;
      --danger-dark: #ad2929;
      --shadow: 0 12px 28px rgba(20, 32, 43, .09);
    }
    * { box-sizing: border-box; }
    html, body { margin: 0; min-height: 100%; }
    body {
      background: var(--canvas);
      color: var(--ink);
      font-family: system-ui, -apple-system, "Segoe UI", Tahoma, sans-serif;
      letter-spacing: 0;
    }
    button, input { font: inherit; letter-spacing: 0; }
    button { cursor: pointer; }
    button:focus-visible, input:focus-visible {
      outline: 3px solid rgba(0, 166, 126, .28);
      outline-offset: 2px;
    }
    .app { min-height: 100vh; display: grid; grid-template-columns: 280px minmax(0, 1fr); }
    .sidebar {
      position: sticky;
      top: 0;
      height: 100vh;
      overflow-y: auto;
      background: var(--nav);
      color: #fff;
      padding: 22px 18px;
      z-index: 20;
    }
    .brand { display: flex; align-items: center; gap: 12px; margin: 0 4px 22px; }
    .brand-mark {
      width: 42px;
      height: 42px;
      display: grid;
      place-items: center;
      border: 1px solid rgba(255,255,255,.2);
      background: var(--nav-soft);
      color: #6ce1c2;
      font-weight: 800;
      font-size: 13px;
    }
    .brand strong { display: block; font-size: 16px; }
    .brand small { display: block; margin-top: 2px; color: #aeb8bf; font-size: 12px; }
    .connection {
      display: flex;
      align-items: center;
      gap: 9px;
      padding: 10px 12px;
      margin-bottom: 18px;
      border: 1px solid rgba(255,255,255,.1);
      background: rgba(255,255,255,.04);
      font-size: 13px;
    }
    .dot { width: 9px; height: 9px; border-radius: 50%; background: #7d8992; flex: none; }
    .dot.online { background: #3bd3a9; box-shadow: 0 0 0 4px rgba(59,211,169,.12); }
    .nav-label { margin: 0 8px 8px; color: #84919b; font-size: 11px; font-weight: 700; text-transform: uppercase; }
    .joint-list { display: grid; gap: 7px; }
    .joint-tab {
      width: 100%;
      min-height: 59px;
      display: grid;
      grid-template-columns: 34px minmax(0, 1fr) auto;
      align-items: center;
      gap: 10px;
      padding: 9px 11px;
      border: 1px solid transparent;
      background: transparent;
      color: #c8d0d5;
      text-align: left;
    }
    .joint-tab:hover { background: rgba(255,255,255,.06); }
    .joint-tab.active { background: #22313b; border-color: #3c505d; color: #fff; }
    .joint-index { width: 32px; height: 32px; display: grid; place-items: center; background: #2a3943; color: #91a0aa; font-size: 12px; font-weight: 800; }
    .joint-tab.active .joint-index { background: var(--accent); color: #fff; }
    .joint-name { min-width: 0; font-size: 14px; font-weight: 650; overflow-wrap: anywhere; }
    .joint-name small { display: block; margin-top: 3px; color: #84919b; font-size: 11px; font-weight: 400; }
    .joint-angle { color: #dfe6e9; font-size: 12px; font-variant-numeric: tabular-nums; }
    .sidebar-foot { margin: 20px 6px 0; color: #76848e; font-size: 11px; line-height: 1.5; }
    .main { min-width: 0; }
    .topbar {
      min-height: 78px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      padding: 15px 28px;
      border-bottom: 1px solid var(--line);
      background: rgba(255,255,255,.92);
    }
    .title-row { display: flex; align-items: center; gap: 12px; min-width: 0; }
    .menu-btn { display: none; width: 42px; height: 42px; border: 1px solid var(--line); background: #fff; font-size: 20px; }
    h1 { margin: 0; font-size: 20px; line-height: 1.25; }
    .subtitle { margin: 3px 0 0; color: var(--muted); font-size: 12px; }
    .top-actions { display: flex; align-items: center; gap: 8px; }
    .btn {
      min-height: 42px;
      border: 1px solid var(--line);
      background: #fff;
      color: var(--ink);
      padding: 0 14px;
      font-weight: 700;
      font-size: 13px;
    }
    .btn:hover { border-color: #aeb9c0; }
    .btn.primary { border-color: var(--accent); background: var(--accent); color: #fff; }
    .btn.primary:hover { background: var(--accent-dark); }
    .btn.danger { border-color: var(--danger); background: var(--danger); color: #fff; }
    .btn.danger:hover { background: var(--danger-dark); }
    .btn:disabled { cursor: not-allowed; opacity: .48; }
    .content { width: min(1180px, 100%); margin: 0 auto; padding: 28px; }
    .status-strip {
      display: grid;
      grid-template-columns: repeat(6, minmax(0, 1fr));
      margin-bottom: 18px;
      border: 1px solid var(--line);
      background: var(--panel);
    }
    .metric { min-width: 0; padding: 14px 17px; border-right: 1px solid var(--line); }
    .metric:nth-child(6n) { border-right: 0; }
    .metric-label { display: block; color: var(--muted); font-size: 11px; }
    .metric-value { display: block; margin-top: 4px; overflow-wrap: anywhere; font-size: 14px; font-weight: 750; font-variant-numeric: tabular-nums; }
    .workspace { display: grid; grid-template-columns: minmax(0, 1.55fr) minmax(280px, .75fr); gap: 18px; align-items: start; }
    .panel { border: 1px solid var(--line); background: var(--panel); box-shadow: var(--shadow); }
    .panel-head { display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; padding: 20px 22px; border-bottom: 1px solid var(--line); }
    .panel-head h2 { margin: 0; font-size: 18px; }
    .panel-head p { margin: 5px 0 0; color: var(--muted); font-size: 12px; }
    .state-pill { padding: 6px 9px; background: #edf1f3; color: #58656f; font-size: 11px; font-weight: 800; white-space: nowrap; }
    .state-pill.moving { background: #e4f7f1; color: #08785d; }
    .control-body { padding: 26px 22px 22px; }
    .angle-readout { display: flex; align-items: baseline; justify-content: center; min-height: 92px; margin-bottom: 12px; }
    .angle-readout strong { font-size: 62px; line-height: 1; font-variant-numeric: tabular-nums; }
    .angle-readout span { margin-left: 5px; color: var(--muted); font-size: 20px; }
    .range-row { display: grid; grid-template-columns: 34px minmax(0, 1fr) 40px; gap: 12px; align-items: center; }
    .range-row > span { color: var(--muted); font-size: 12px; text-align: center; }
    input[type="range"] { width: 100%; accent-color: var(--accent); }
    .stepper { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 8px; margin: 22px 0; }
    .stepper button { min-height: 44px; border: 1px solid var(--line); background: #f8f9fa; color: var(--ink); font-weight: 750; }
    .stepper button:hover { border-color: var(--accent); color: var(--accent-dark); }
    .direct-control { display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: 9px; }
    .input-wrap { display: grid; grid-template-columns: minmax(0, 1fr) auto; align-items: center; border: 1px solid var(--line); background: #fff; }
    .input-wrap input { width: 100%; min-height: 44px; border: 0; padding: 0 12px; color: var(--ink); font-weight: 700; }
    .input-wrap span { padding: 0 12px; color: var(--muted); font-size: 12px; }
    .control-foot { display: flex; justify-content: space-between; gap: 10px; margin-top: 17px; padding-top: 16px; border-top: 1px solid var(--line); }
    .range-note { color: var(--muted); font-size: 12px; line-height: 1.5; }
    .side-stack { display: grid; gap: 18px; }
    .quick-actions { display: grid; gap: 9px; padding: 18px; }
    .quick-actions .btn { width: 100%; display: flex; align-items: center; justify-content: space-between; text-align: left; }
    .safety { padding: 18px; border-left: 4px solid var(--warning); background: #fffaf0; }
    .safety h3 { margin: 0 0 7px; font-size: 14px; }
    .safety p { margin: 0; color: #6d6250; font-size: 12px; line-height: 1.6; }
    .emergency { width: 100%; min-height: 54px; margin-top: 13px; font-size: 14px; }
    .toast {
      position: fixed;
      right: 20px;
      bottom: 20px;
      max-width: min(360px, calc(100vw - 40px));
      padding: 12px 15px;
      background: #14202b;
      color: #fff;
      box-shadow: var(--shadow);
      font-size: 13px;
      transform: translateY(120px);
      opacity: 0;
      transition: .2s ease;
      z-index: 50;
    }
    .toast.show { transform: translateY(0); opacity: 1; }
    .toast.error { background: var(--danger-dark); }
    .overlay { display: none; }
    @media (max-width: 920px) {
      .app { grid-template-columns: 238px minmax(0, 1fr); }
      .workspace { grid-template-columns: 1fr; }
      .side-stack { grid-template-columns: 1fr 1fr; }
      .top-actions .label-wide { display: none; }
    }
    @media (max-width: 720px) {
      .app { display: block; }
      .sidebar { position: fixed; left: 0; transform: translateX(-105%); width: min(310px, 88vw); transition: transform .2s ease; box-shadow: 12px 0 30px rgba(0,0,0,.25); }
      .sidebar.open { transform: translateX(0); }
      .overlay { position: fixed; inset: 0; display: block; pointer-events: none; opacity: 0; background: rgba(8,15,20,.55); transition: opacity .2s; z-index: 15; }
      .overlay.show { opacity: 1; pointer-events: auto; }
      .menu-btn { display: inline-grid; place-items: center; flex: none; }
      .topbar { position: sticky; top: 0; z-index: 10; min-height: 68px; padding: 11px 14px; }
      .top-actions #homeTopBtn { display: none; }
      .top-actions .btn { min-width: 42px; padding: 0 10px; }
      h1 { font-size: 15px; }
      .subtitle { display: none; }
      .content { padding: 16px 12px 28px; }
      .status-strip { grid-template-columns: 1fr 1fr; }
      .metric:nth-child(2n) { border-right: 0; }
      .metric:nth-child(-n+4) { border-bottom: 1px solid var(--line); }
      .workspace, .side-stack { grid-template-columns: 1fr; }
      .panel-head, .control-body { padding-left: 17px; padding-right: 17px; }
      .angle-readout strong { font-size: 52px; }
    }
  </style>
</head>
<body>
  <div class="app">
    <aside class="sidebar" id="sidebar">
      <div class="brand">
        <div class="brand-mark">6X</div>
        <div><strong>Robot Arm</strong><small>ESP8266 Control</small></div>
      </div>
      <div class="connection"><span class="dot" id="connectionDot"></span><span id="connectionText">กำลังเชื่อมต่อ...</span></div>
      <p class="nav-label">ข้อต่อทั้งหมด</p>
      <nav class="joint-list" id="jointList" aria-label="เลือกข้อต่อ"></nav>
      <p class="sidebar-foot">การเคลื่อนที่ถูกจำกัดตามช่วงที่ตั้งไว้ในเฟิร์มแวร์ ปลดล็อกก่อนส่งคำสั่ง</p>
    </aside>
    <div class="overlay" id="overlay"></div>

    <main class="main">
      <header class="topbar">
        <div class="title-row">
          <button class="menu-btn" id="menuBtn" aria-label="เปิดเมนู">☰</button>
          <div><h1>ระบบควบคุมแขนกล 6 แกน</h1><p class="subtitle" id="headerStatus">รอข้อมูลจากตัวควบคุม</p></div>
        </div>
        <div class="top-actions">
          <button class="btn" id="lockBtn" type="button">ปลดล็อก</button>
          <button class="btn primary" id="homeTopBtn" type="button"><span class="label-wide">กลับตำแหน่ง </span>Home</button>
          <button class="btn danger" id="stopTopBtn" type="button" aria-label="หยุดฉุกเฉิน">STOP</button>
        </div>
      </header>

      <div class="content">
        <section class="status-strip" aria-label="สถานะระบบ">
          <div class="metric"><span class="metric-label">เครือข่าย</span><strong class="metric-value" id="ssidValue">-</strong></div>
          <div class="metric"><span class="metric-label">IP Address</span><strong class="metric-value" id="ipValue">-</strong></div>
          <div class="metric"><span class="metric-label">ความปลอดภัย</span><strong class="metric-value" id="lockValue">-</strong></div>
          <div class="metric"><span class="metric-label">กำลังเคลื่อนที่</span><strong class="metric-value" id="movingValue">-</strong></div>
          <div class="metric"><span class="metric-label">PCA9685</span><strong class="metric-value" id="pwmValue">-</strong></div>
          <div class="metric"><span class="metric-label">I2C Address</span><strong class="metric-value" id="pwmAddressValue">-</strong></div>

        </section>

        <div class="workspace">
          <section class="panel">
            <div class="panel-head">
              <div><h2 id="jointTitle">ฐานหมุน</h2><p id="jointMeta">Joint 0 · Channel 0</p></div>
              <span class="state-pill" id="jointState">OFF</span>
            </div>
            <div class="control-body">
              <div class="angle-readout"><strong id="angleReadout">90</strong><span>°</span></div>
              <div class="range-row"><span id="minLabel">10°</span><input id="angleSlider" type="range" min="10" max="170" value="90" aria-label="มุมข้อต่อ"><span id="maxLabel">170°</span></div>
              <div class="stepper" aria-label="ปรับมุมแบบละเอียด">
                <button type="button" data-step="-10">−10°</button>
                <button type="button" data-step="-5">−5°</button>
                <button type="button" data-step="5">+5°</button>
                <button type="button" data-step="10">+10°</button>
              </div>
              <div class="direct-control">
                <label class="input-wrap"><input id="angleInput" type="number" min="10" max="170" value="90" inputmode="numeric" aria-label="ระบุมุม"><span>องศา</span></label>
                <button class="btn primary" id="moveBtn" type="button">สั่งเคลื่อนที่</button>
              </div>
              <div class="control-foot">
                <div class="range-note">ช่วงปลอดภัย <strong id="safeRange">10° – 170°</strong><br>Home <strong id="homeAngle">90°</strong></div>
                <button class="btn" id="offJointBtn" type="button">ปิดเซอร์โวข้อนี้</button>
              </div>
            </div>
          </section>

          <div class="side-stack">
            <section class="panel">
              <div class="panel-head"><div><h2>คำสั่งด่วน</h2><p>ควบคุมทุกข้อต่อพร้อมกัน</p></div></div>
              <div class="quick-actions">
                <button class="btn" id="homeBtn" type="button"><span>กลับตำแหน่ง Home</span><span>⌂</span></button>
                <button class="btn" id="lockSideBtn" type="button"><span>ปลดล็อกการควบคุม</span><span>○</span></button>
              </div>
            </section>
            <section class="panel safety">
              <h3>ความปลอดภัย</h3>
              <p>ปุ่มฉุกเฉินจะหยุดสัญญาณ PWM ทุกช่องและล็อกคำสั่งทันที แขนกลอาจตกลงตามแรงโน้มถ่วงเมื่อเซอร์โวถูกปิด</p>
              <button class="btn danger emergency" id="stopBtn" type="button">หยุดฉุกเฉิน</button>
            </section>
          </div>
        </div>
      </div>
    </main>
  </div>
  <div class="toast" id="toast" role="status" aria-live="polite"></div>

  <script>
    const displayNames = ['ฐานหมุน', 'หัวไหล่', 'ข้อศอก', 'ข้อมือก้ม-เงย', 'ข้อมือหมุน', 'กริปเปอร์'];
    let state = null;
    let selected = 0;
    let previewAngle = 90;
    let toastTimer = 0;

    const el = id => document.getElementById(id);
    const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

    function showToast(message, error = false) {
      const node = el('toast');
      node.textContent = message;
      node.className = 'toast show' + (error ? ' error' : '');
      clearTimeout(toastTimer);
      toastTimer = setTimeout(() => node.className = 'toast', 2600);
    }

    async function request(path, params = {}) {
      const body = new URLSearchParams(params);
      const response = await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
      let data = {};
      try { data = await response.json(); } catch (_) {}
      if (!response.ok || data.ok === false) throw new Error(data.error || 'คำสั่งไม่สำเร็จ');
      return data;
    }

    function renderSidebar() {
      if (!state) return;
      el('jointList').innerHTML = state.joints.map((joint, index) => {
        const angle = joint.enabled ? joint.current + '°' : 'OFF';
        const sub = joint.moving ? 'กำลังเคลื่อนที่' : 'CH' + joint.channel;
        return `<button class="joint-tab ${index === selected ? 'active' : ''}" data-joint="${index}" type="button"><span class="joint-index">J${index}</span><span class="joint-name">${displayNames[index]}<small>${sub}</small></span><span class="joint-angle">${angle}</span></button>`;
      }).join('');
      document.querySelectorAll('[data-joint]').forEach(button => button.addEventListener('click', () => {
        selected = Number(button.dataset.joint);
        syncSelected(true);
        closeSidebar();
      }));
    }

    function syncSelected(resetPreview) {
      if (!state) return;
      const joint = state.joints[selected];
      if (resetPreview) previewAngle = joint.enabled ? joint.current : joint.home;
      previewAngle = clamp(previewAngle, joint.min, joint.max);
      el('jointTitle').textContent = displayNames[selected];
      el('jointMeta').textContent = `Joint ${selected} · Channel ${joint.channel} · ${joint.name}`;
      el('jointState').textContent = joint.moving ? 'MOVING' : (joint.enabled ? 'HOLDING' : 'OFF');
      el('jointState').className = 'state-pill' + (joint.moving ? ' moving' : '');
      el('angleReadout').textContent = previewAngle;
      el('angleSlider').min = joint.min;
      el('angleSlider').max = joint.max;
      el('angleSlider').value = previewAngle;
      el('angleInput').min = joint.min;
      el('angleInput').max = joint.max;
      el('angleInput').value = previewAngle;
      el('minLabel').textContent = joint.min + '°';
      el('maxLabel').textContent = joint.max + '°';
      el('safeRange').textContent = `${joint.min}° – ${joint.max}°`;
      el('homeAngle').textContent = joint.home + '°';
      const disabled = state.locked;
      el('angleSlider').disabled = disabled;
      el('angleInput').disabled = disabled;
      el('moveBtn').disabled = disabled;
      document.querySelectorAll('[data-step]').forEach(button => button.disabled = disabled);
      renderSidebar();
    }

    function renderState(resetPreview = false) {
      const movingCount = state.joints.filter(j => j.moving).length;
      const pwmConnected = state.pwmReady === true;
      const pwmAddress = Number.isFinite(Number(state.pwmAddress)) ? Number(state.pwmAddress) : 0x40;
      el('pwmValue').textContent = pwmConnected ? 'CONNECTED' : 'NOT FOUND';
      el('pwmAddressValue').textContent = '0x' + pwmAddress.toString(16).toUpperCase().padStart(2, '0') + ' (0x40-0x7F)';
      el('headerStatus').textContent = (pwmConnected ? 'PCA9685 CONNECTED' : 'PCA9685 NOT FOUND') + ' · ' + (state.locked ? 'ระบบล็อกอยู่' : 'พร้อมควบคุม');
      el('connectionDot').className = 'dot online';
      el('connectionText').textContent = 'เชื่อมต่อกับตัวควบคุมแล้ว';
      el('headerStatus').textContent = state.locked ? 'ระบบล็อกอยู่ · ปลดล็อกก่อนควบคุม' : 'พร้อมรับคำสั่งควบคุม';
      el('ssidValue').textContent = state.ssid;
      el('ipValue').textContent = state.ip;
      el('lockValue').textContent = state.locked ? 'ล็อกอยู่' : 'ปลดล็อกแล้ว';
      el('movingValue').textContent = movingCount ? movingCount + ' ข้อต่อ' : 'หยุดนิ่ง';
      el('lockBtn').textContent = state.locked ? 'ปลดล็อก' : 'ล็อก';
      el('lockSideBtn').firstElementChild.textContent = state.locked ? 'ปลดล็อกการควบคุม' : 'ล็อกการควบคุม';
      el('homeBtn').disabled = state.locked;
      el('homeTopBtn').disabled = state.locked;
      syncSelected(resetPreview);
    }

    async function refresh(initial = false) {
      try {
        const response = await fetch('/api/status', { cache: 'no-store' });
        if (!response.ok) throw new Error('offline');
        const nextState = await response.json();
        const shouldReset = initial || !state || (state.joints[selected].moving && !nextState.joints[selected].moving);
        state = nextState;
        renderState(shouldReset);
      } catch (_) {
        el('connectionDot').className = 'dot';
        el('connectionText').textContent = 'ขาดการเชื่อมต่อ';
        el('headerStatus').textContent = 'กำลังพยายามเชื่อมต่อใหม่';
      }
    }

    function setPreview(value) {
      if (!state) return;
      const joint = state.joints[selected];
      previewAngle = clamp(Number(value) || joint.min, joint.min, joint.max);
      el('angleReadout').textContent = previewAngle;
      el('angleSlider').value = previewAngle;
      el('angleInput').value = previewAngle;
    }

    async function moveSelected() {
      try {
        await request('/api/joint', { id: selected, angle: previewAngle });
        showToast(`ส่งคำสั่ง ${displayNames[selected]} ไปที่ ${previewAngle}° แล้ว`);
        await refresh();
      } catch (error) { showToast(error.message, true); }
    }

    async function toggleLock() {
      if (!state) return;
      try {
        await request('/api/lock', { locked: state.locked ? 0 : 1 });
        showToast(state.locked ? 'ปลดล็อกการควบคุมแล้ว' : 'ล็อกการควบคุมแล้ว');
        await refresh();
      } catch (error) { showToast(error.message, true); }
    }

    async function goHome() {
      try { await request('/api/home'); showToast('กำลังกลับตำแหน่ง Home'); await refresh(); }
      catch (error) { showToast(error.message, true); }
    }

    async function stopAll() {
      try { await request('/api/stop'); showToast('หยุดสัญญาณเซอร์โวทุกช่องแล้ว', true); await refresh(true); }
      catch (error) { showToast(error.message, true); }
    }

    async function offSelected() {
      try { await request('/api/off', { id: selected }); showToast(`ปิดเซอร์โว ${displayNames[selected]} แล้ว`); await refresh(true); }
      catch (error) { showToast(error.message, true); }
    }

    function closeSidebar() { el('sidebar').classList.remove('open'); el('overlay').classList.remove('show'); }
    el('menuBtn').addEventListener('click', () => { el('sidebar').classList.toggle('open'); el('overlay').classList.toggle('show'); });
    el('overlay').addEventListener('click', closeSidebar);
    el('angleSlider').addEventListener('input', event => setPreview(event.target.value));
    el('angleSlider').addEventListener('change', moveSelected);
    el('angleInput').addEventListener('input', event => setPreview(event.target.value));
    el('angleInput').addEventListener('keydown', event => { if (event.key === 'Enter') moveSelected(); });
    el('moveBtn').addEventListener('click', moveSelected);
    document.querySelectorAll('[data-step]').forEach(button => button.addEventListener('click', () => { setPreview(previewAngle + Number(button.dataset.step)); moveSelected(); }));
    [el('lockBtn'), el('lockSideBtn')].forEach(button => button.addEventListener('click', toggleLock));
    [el('homeBtn'), el('homeTopBtn')].forEach(button => button.addEventListener('click', goHome));
    [el('stopBtn'), el('stopTopBtn')].forEach(button => button.addEventListener('click', stopAll));
    el('offJointBtn').addEventListener('click', offSelected);

    refresh(true);
    setInterval(refresh, 900);
  </script>
</body>
</html>
)HTML";
