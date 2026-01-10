// http_routes.cpp
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "http_routes.h"
#include "hostname.h"
#include "leds.h"
#include "wan_metrics.h"
#include "local_pinger.h"
#include "freshness_bar.h"

// ---- Favicon SVGs ----
static const char* FAVICON_GREEN = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#2ecc71"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";
static const char* FAVICON_YELLOW = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#f1c40f"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";
static const char* FAVICON_RED = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#e74c3c"/>
  <text x="16" y="16" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
  <text x="16" y="24" text-anchor="middle" font-size="12" font-weight="700" fill="#ffffff" font-family="system-ui, sans-serif">W</text>
</svg>)";

static const char* favicon_for_state(WanState state) {
    switch (state) {
        case WanState::UP: return "/favicon-green.svg";
        case WanState::DEGRADED: return "/favicon-yellow.svg";
        default: return "/favicon-red.svg";
    }
}

// ---- Helper: State cell with colored circle ----
static String state_cell_html(WanState state) {
    switch (state) {
        case WanState::UP:
            return "&#x1F7E2; UP";
        case WanState::DEGRADED:
            return "&#x1F7E1; DEGRADED";
        case WanState::DOWN:
        default:
            return "&#x1F534; DOWN";
    }
}

// ---- Helper: makes human readable last updated string ----
static String last_update_human(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);

    if (m.last_update_ms == 0) {
        return "never";
    }

    unsigned long now = millis();
    unsigned long elapsed = now - m.last_update_ms;
    unsigned long secs = elapsed / 1000UL;

    if (secs < 60) {
        return String(secs) + "s ago";
    }

    unsigned long mins = secs / 60;
    if (mins < 60) {
        unsigned long rem_s = secs % 60;
        String s = String(mins) + "m";
        if (rem_s > 0) {
            s += " " + String(rem_s) + "s";
        }
        s += " ago";
        return s;
    }

    unsigned long hours = mins / 60;
    unsigned long rem_m = mins % 60;
    String s = String(hours) + "h";
    if (rem_m > 0) {
        s += " " + String(rem_m) + "m";
    }
    s += " ago";
    return s;
}

// ---- Helper: WAN description (TODO: make configurable) ----
static String wan_description(int wan_id) {
    switch (wan_id) {
        case 1: return "PeakWifi";
        case 2: return "Starlink";
        default: return "";
    }
}

// ---- Helper: WAN metrics table row ----
static String wan_metrics_row_html(int wan_id) {
    const WanMetrics& m = wan_metrics_get(wan_id);
    String id = "w" + String(wan_id);

    String html = "<tr>";
    html += "<td>WAN" + String(wan_id) + "</td>";
    html += "<td>" + wan_description(wan_id) + "</td>";
    html += "<td id=\"" + id + "-state\">" + state_cell_html(m.state) + "</td>";
    html += "<td id=\"" + id + "-mon\">" + String(m.monitor_ip) + "</td>";
    html += "<td id=\"" + id + "-gw\">" + String(m.gateway_ip) + "</td>";
    html += "<td id=\"" + id + "-lip\">" + String(m.local_ip) + "</td>";
    html += "<td id=\"" + id + "-loss\">" + String(m.loss_pct) + "%</td>";
    html += "<td id=\"" + id + "-lat\">" + String(m.latency_ms) + " ms</td>";
    html += "<td id=\"" + id + "-jit\">" + String(m.jitter_ms) + " ms</td>";
    html += "<td id=\"" + id + "-down\">" + String(m.down_mbps, 1) + " Mbps</td>";
    html += "<td id=\"" + id + "-up\">" + String(m.up_mbps, 1) + " Mbps</td>";
    html += "</tr>";

    return html;
}

// ---- Helper: Local pinger last updated string ----
static String local_pinger_last_update_human() {
    const LocalPingerMetrics& m = local_pinger_get();

    if (m.last_update_ms == 0) {
        return "never";
    }

    unsigned long now = millis();
    unsigned long elapsed = now - m.last_update_ms;
    unsigned long secs = elapsed / 1000UL;

    if (secs < 60) {
        return String(secs) + "s ago";
    }

    unsigned long mins = secs / 60;
    if (mins < 60) {
        unsigned long rem_s = secs % 60;
        String s = String(mins) + "m";
        if (rem_s > 0) {
            s += " " + String(rem_s) + "s";
        }
        s += " ago";
        return s;
    }

    unsigned long hours = mins / 60;
    unsigned long rem_m = mins % 60;
    String s = String(hours) + "h";
    if (rem_m > 0) {
        s += " " + String(rem_m) + "m";
    }
    s += " ago";
    return s;
}

// ---- Helper: Local pinger metrics table row ----
static String local_pinger_metrics_row_html() {
    const LocalPingerMetrics& m = local_pinger_get();

    String html = "<tr>";
    html += "<td>Local</td>";
    html += "<td>" + get_network_hostname() + "</td>";
    html += "<td id=\"lp-state\">" + state_cell_html(m.state) + "</td>";
    html += "<td id=\"lp-mon\">" + String(local_pinger_get_target()) + "</td>";  // Monitor target
    html += "<td id=\"lp-gw\">" + String(wan_metrics_get_router_ip()) + "</td>";  // Router IP
    html += "<td id=\"lp-lip\">" + get_network_ip() + "</td>";  // ESP32's IP
    html += "<td id=\"lp-loss\">" + String(m.loss_pct) + "%</td>";
    html += "<td id=\"lp-lat\">" + String(m.latency_ms) + " ms</td>";
    html += "<td id=\"lp-jit\">" + String(m.jitter_ms) + " ms</td>";
    html += "<td>-</td>";  // No download for local pinger
    html += "<td>-</td>";  // No upload for local pinger
    html += "</tr>";

    return html;
}

// ---- Main page HTML ----
static String root_page_html() {
    String hostname = get_network_hostname();
    const LocalPingerMetrics& local = local_pinger_get();
    const char* favicon_url = favicon_for_state(local.state);

    String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>wan-watcher</title>
  <link rel="icon" type="image/svg+xml" href=")";
    html += favicon_url;
    html += R"(">
  <style>
    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      margin: 1.5rem;
    }
    h1 { margin-bottom: 0.5rem; display: flex; align-items: center; gap: 0.3em; }
    h1 img { height: 1em; width: 1em; }
    .status { margin-top: 0.5rem; margin-bottom: 1.5rem; }
    code { background: #f5f5f5; padding: 2px 4px; border-radius: 3px; }
    table { border-collapse: collapse; margin: 1rem 0; }
    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
    th { background: #f5f5f5; }
    .freshness-bar {
      display: flex;
      height: 20px;
      border: 1px solid #555;
      border-radius: 4px;
      overflow: hidden;
      background: #222;
      grid-column: 2 / 5;
      margin-top: 12px;
      gap: 2px;
      padding: 2px;
    }
    .freshness-led {
      flex: 1;
      height: 100%;
      border-radius: 2px;
      background: #333;
      transition: background-color 0.1s;
    }
    .freshness-led.green { background: #2ecc71; box-shadow: 0 0 4px #2ecc71; }
    .freshness-led.yellow { background: #f1c40f; box-shadow: 0 0 4px #f1c40f; }
    .freshness-led.red { background: #e74c3c; box-shadow: 0 0 4px #e74c3c; }
    .freshness-bar.blink .freshness-led.red { animation: led-blink 0.5s infinite; }
    @keyframes led-blink { 50% { background: #333; box-shadow: none; } }
    /* 7-segment display panel */
    .display-panel { background: #fff; padding: 1rem; border-radius: 8px; margin: 1rem 0; display: inline-block; border: 1px solid #ccc; }
    .display-grid { display: grid; grid-template-columns: 50px repeat(3, 1fr); gap: 8px; align-items: center; }
    .display-grid .row-label { color: #333; font-size: 0.8em; font-weight: bold; text-align: right; padding-right: 8px; }
    .display-grid .col-header { color: #333; font-size: 0.9em; font-weight: bold; text-align: center; }
    .seg-display { display: inline-flex; background: #111; padding: 8px 12px; border-radius: 6px; cursor: pointer; justify-self: center; }
    /* State LEDs */
    .led-row { display: flex; justify-content: center; gap: 8px; padding: 8px 0; }
    .state-led { width: 18px; height: 18px; border-radius: 50%; border: 1px solid #333; }
    .state-led.off { background: #444; }
    .state-led.green { background: #2ecc71; box-shadow: 0 0 8px #2ecc71; }
    .state-led.yellow { background: #f1c40f; box-shadow: 0 0 8px #f1c40f; }
    .state-led.red { background: #e74c3c; box-shadow: 0 0 8px #e74c3c; }
    .digit { position: relative; width: 28px; height: 50px; margin: 0 2px; }
    .seg { position: absolute; background: #222; border-radius: 2px; }
    .seg.on { background: #f00; box-shadow: 0 0 6px #f00; }
    .seg-a,.seg-d,.seg-g { width: 18px; height: 5px; left: 5px; }
    .seg-a { top: 0; }
    .seg-g { top: 22px; }
    .seg-d { top: 45px; }
    .seg-b,.seg-c,.seg-e,.seg-f { width: 5px; height: 18px; }
    .seg-f { left: 0; top: 3px; }
    .seg-b { right: 0; top: 3px; }
    .seg-e { left: 0; top: 25px; }
    .seg-c { right: 0; top: 25px; }
    .seg-dp { position: absolute; width: 5px; height: 5px; right: -3px; bottom: 2px; border-radius: 50%; background: #222; }
    .seg-dp.on { background: #f00; box-shadow: 0 0 6px #f00; }
  </style>
</head>
<body>
)";
    html += "<h1><img src=\"" + String(favicon_url) + "\" alt=\"\">wan-watcher<img src=\"" + String(favicon_url) + "\" alt=\"\"></h1>";
    html += R"(
)";

    html += "<p><strong>Hostname:</strong> <code>" + hostname + "</code><br>";

    const char* timestamp = wan_metrics_get_timestamp();
    if (timestamp[0] != '\0') {
        html += "<strong>Last update:</strong> <code id=\"last-update\" data-iso=\"" + String(timestamp) + "\">" + String(timestamp) + "</code>";
    } else {
        html += "<strong>Last update:</strong> <code id=\"last-update\">Never</code>";
    }
    html += " <span id=\"elapsed-time\" style=\"color:#666;\"></span></p>";

    // 7-segment display panel
    const WanMetrics& w1 = wan_metrics_get(1);
    const WanMetrics& w2 = wan_metrics_get(2);
    const LocalPingerMetrics& lp = local_pinger_get();

    html += "<div class=\"display-panel\" id=\"seg-panel\"";
    html += " data-w1-state=\"" + String(wan_state_to_string(w1.state)) + "\"";
    html += " data-w1-lat=\"" + String(w1.latency_ms) + "\"";
    html += " data-w1-jit=\"" + String(w1.jitter_ms) + "\"";
    html += " data-w1-loss=\"" + String(w1.loss_pct) + "\"";
    html += " data-w1-down=\"" + String(w1.down_mbps, 1) + "\"";
    html += " data-w1-up=\"" + String(w1.up_mbps, 1) + "\"";
    html += " data-w2-state=\"" + String(wan_state_to_string(w2.state)) + "\"";
    html += " data-w2-lat=\"" + String(w2.latency_ms) + "\"";
    html += " data-w2-jit=\"" + String(w2.jitter_ms) + "\"";
    html += " data-w2-loss=\"" + String(w2.loss_pct) + "\"";
    html += " data-w2-down=\"" + String(w2.down_mbps, 1) + "\"";
    html += " data-w2-up=\"" + String(w2.up_mbps, 1) + "\"";
    html += " data-lp-state=\"" + String(wan_state_to_string(lp.state)) + "\"";
    html += " data-lp-lat=\"" + String(lp.latency_ms) + "\"";
    html += " data-lp-jit=\"" + String(lp.jitter_ms) + "\"";
    html += " data-lp-loss=\"" + String(lp.loss_pct) + "\"";
    html += ">";
    html += R"(
  <div class="display-grid">
    <!-- Header row -->
    <div></div>
    <div class="col-header">WAN1</div>
    <div class="col-header">WAN2</div>
    <div class="col-header">Local</div>
    <!-- State LED row -->
    <div class="row-label"></div>
    <div class="led-row" id="w1-leds">
      <div class="state-led" id="w1-led-up"></div>
      <div class="state-led" id="w1-led-deg"></div>
      <div class="state-led" id="w1-led-down"></div>
    </div>
    <div class="led-row" id="w2-leds">
      <div class="state-led" id="w2-led-up"></div>
      <div class="state-led" id="w2-led-deg"></div>
      <div class="state-led" id="w2-led-down"></div>
    </div>
    <div class="led-row" id="lp-leds">
      <div class="state-led" id="lp-led-up"></div>
      <div class="state-led" id="lp-led-deg"></div>
      <div class="state-led" id="lp-led-down"></div>
    </div>
    <!-- L/J/P row -->
    <div class="row-label">L/J/P</div>
    <div class="seg-display pkt-display" id="w1-pkt"></div>
    <div class="seg-display pkt-display" id="w2-pkt"></div>
    <div class="seg-display pkt-display" id="lp-pkt"></div>
    <!-- d/U row -->
    <div class="row-label">d/U</div>
    <div class="seg-display bw-display" id="w1-bw"></div>
    <div class="seg-display bw-display" id="w2-bw"></div>
    <div></div>
    <!-- Freshness bar row (24 LEDs generated by JS) -->
    <div class="row-label">Fresh</div>
    <div class="freshness-bar" id="freshness-bar"></div>
  </div>
</div>
<script>
(function(){
  // === Shared state ===
  var updateTime = null;
  var el = document.getElementById('last-update');
  if (el && el.dataset.iso) {
    updateTime = new Date(el.dataset.iso);
    if (!isNaN(updateTime)) {
      el.textContent = updateTime.toLocaleString();
    } else {
      updateTime = null;
    }
  }

  // === Freshness bar (24 discrete LEDs matching hardware) ===
  var bar = document.getElementById('freshness-bar');
  var elapsedEl = document.getElementById('elapsed-time');
  var leds = [];

  // Timing constants (defaults, updated from API)
  var F = {
    greenFillEnd: 15, greenBufferEnd: 20,
    yellowFillEnd: 35, yellowBufferEnd: 40,
    redFillEnd: 55, redBufferEnd: 60,
    fillDuration: 15, ledCount: 24
  };

  // Create LED elements
  for (var i = 0; i < F.ledCount; i++) {
    var led = document.createElement('div');
    led.className = 'freshness-led';
    bar.appendChild(led);
    leds.push(led);
  }

  function updateFreshness() {
    // Use fractional seconds for smooth LED transitions
    var elapsedMs = updateTime ? Date.now() - updateTime.getTime() : 999000;
    var elapsed = elapsedMs / 1000; // fractional seconds for LED calc
    var greenCount = 0, yellowCount = 0, redCount = 0;

    if (elapsed >= F.redBufferEnd) {
      // >60s: All red, blinking
      redCount = F.ledCount;
      bar.classList.add('blink');
    } else {
      bar.classList.remove('blink');
      if (elapsed < F.greenFillEnd) {
        // 0-15s: Green fills
        greenCount = Math.floor((elapsed * F.ledCount) / F.fillDuration);
      } else if (elapsed < F.greenBufferEnd) {
        // 15-20s: Buffer - all green
        greenCount = F.ledCount;
      } else if (elapsed < F.yellowFillEnd) {
        // 20-35s: Yellow overwrites green
        var yellowElapsed = elapsed - F.greenBufferEnd;
        yellowCount = Math.floor((yellowElapsed * F.ledCount) / F.fillDuration);
        greenCount = F.ledCount - yellowCount;
      } else if (elapsed < F.yellowBufferEnd) {
        // 35-40s: Buffer - all yellow
        yellowCount = F.ledCount;
      } else if (elapsed < F.redFillEnd) {
        // 40-55s: Red overwrites yellow
        var redElapsed = elapsed - F.yellowBufferEnd;
        redCount = Math.floor((redElapsed * F.ledCount) / F.fillDuration);
        yellowCount = F.ledCount - redCount;
      } else {
        // 55-60s: Buffer - all red
        redCount = F.ledCount;
      }
    }

    // Update LED colors (red on left, then yellow, then green, then off)
    for (var i = 0; i < F.ledCount; i++) {
      var led = leds[i];
      led.className = 'freshness-led';
      if (redCount > 0 && i < redCount) {
        led.classList.add('red');
      } else if (yellowCount > 0 && i < redCount + yellowCount) {
        led.classList.add('yellow');
      } else if (greenCount > 0 && i < redCount + yellowCount + greenCount) {
        led.classList.add('green');
      }
    }

    elapsedEl.textContent = '(' + Math.floor(elapsed) + 's ago)';
  }
  updateFreshness();
  setInterval(updateFreshness, 250); // 250ms is sufficient for 625ms LED transitions

  // Match panel width to table
  function matchPanelWidth() {
    var tbl = document.querySelector('table');
    var panel = document.getElementById('seg-panel');
    if (tbl && panel) {
      panel.style.width = tbl.offsetWidth + 'px';
      panel.style.boxSizing = 'border-box';
    }
  }
  window.addEventListener('load', matchPanelWidth);
  window.addEventListener('resize', matchPanelWidth);

  // === 7-segment displays ===
  var SEG={
    '0':'abcdef','1':'bc','2':'abdeg','3':'abcdg','4':'bcfg','5':'acdfg',
    '6':'acdefg','7':'abc','8':'abcdefg','9':'abcdfg',
    'L':'def','J':'bcde','P':'abefg','d':'bcdeg','U':'bcdef','-':'g',' ':''
  };
  function mkDigit(){
    var d=document.createElement('div');d.className='digit';
    ['a','b','c','d','e','f','g'].forEach(function(s){
      var e=document.createElement('div');e.className='seg seg-'+s;d.appendChild(e);
    });
    var dp=document.createElement('div');dp.className='seg-dp';d.appendChild(dp);
    return d;
  }
  function initDisplay(id){
    var el=document.getElementById(id);
    for(var i=0;i<4;i++)el.appendChild(mkDigit());
  }
  function setDisplay(id,prefix,val){
    var el=document.getElementById(id);
    var digits=el.querySelectorAll('.digit');
    var v=val.toString();
    var hasDP=v.indexOf('.')>=0;
    v=v.replace('.','');
    while(v.length<3)v=' '+v;
    v=v.substring(v.length-3);
    var chars=[prefix,v[0],v[1],v[2]];
    var dps=[false,false,hasDP&&v.length>=2,false];
    for(var i=0;i<4;i++){
      var c=chars[i];
      var segs=SEG[c]||'';
      var digit=digits[i];
      ['a','b','c','d','e','f','g'].forEach(function(seg){
        digit.querySelector('.seg-'+seg).classList.toggle('on',segs.indexOf(seg)>=0);
      });
      digit.querySelector('.seg-dp').classList.toggle('on',dps[i]);
    }
  }
  function setDisplayDashes(id){
    var el=document.getElementById(id);
    var digits=el.querySelectorAll('.digit');
    for(var i=0;i<4;i++){
      var digit=digits[i];
      ['a','b','c','d','e','f','g'].forEach(function(seg){
        digit.querySelector('.seg-'+seg).classList.toggle('on',seg==='g');
      });
      digit.querySelector('.seg-dp').classList.toggle('on',false);
    }
  }
  ['w1-pkt','w1-bw','w2-pkt','w2-bw','lp-pkt'].forEach(initDisplay);

  var pktIdx=0,bwIdx=0;
  var P=document.getElementById('seg-panel').dataset;

  // === State LED update ===
  function setLeds(prefix,state){
    var $=function(id){return document.getElementById(id);};
    var up=$(prefix+'-led-up'),deg=$(prefix+'-led-deg'),dn=$(prefix+'-led-down');
    up.className='state-led '+(state==='up'?'green':'off');
    deg.className='state-led '+(state==='degraded'?'yellow':'off');
    dn.className='state-led '+(state==='down'?'red':'off');
  }
  function updLeds(){
    setLeds('w1',P.w1State);
    setLeds('w2',P.w2State);
    setLeds('lp',P.lpState);
  }
  updLeds();

  function updDisp(){
    var pktM=['L','J','P'],bwM=['d','U'];
    var pm=pktM[pktIdx],bm=bwM[bwIdx];
    if(P.w1State==='down'){
      setDisplayDashes('w1-pkt');setDisplayDashes('w1-bw');
    }else{
      setDisplay('w1-pkt',pm,[P.w1Lat,P.w1Jit,P.w1Loss][pktIdx]);
      setDisplay('w1-bw',bm,[P.w1Down,P.w1Up][bwIdx]);
    }
    if(P.w2State==='down'){
      setDisplayDashes('w2-pkt');setDisplayDashes('w2-bw');
    }else{
      setDisplay('w2-pkt',pm,[P.w2Lat,P.w2Jit,P.w2Loss][pktIdx]);
      setDisplay('w2-bw',bm,[P.w2Down,P.w2Up][bwIdx]);
    }
    if(P.lpState==='down'){
      setDisplayDashes('lp-pkt');
    }else{
      setDisplay('lp-pkt',pm,[P.lpLat,P.lpJit,P.lpLoss][pktIdx]);
    }
  }
  updDisp();
  document.querySelectorAll('.pkt-display').forEach(function(e){
    e.addEventListener('click',function(){pktIdx=(pktIdx+1)%3;updDisp();});
  });
  document.querySelectorAll('.bw-display').forEach(function(e){
    e.addEventListener('click',function(){bwIdx=(bwIdx+1)%2;updDisp();});
  });
  setInterval(function(){pktIdx=(pktIdx+1)%3;bwIdx=(bwIdx+1)%2;updDisp();},5000);

  // === State emoji helper ===
  function stateHtml(s){
    if(s==='up')return'\u{1F7E2} UP';
    if(s==='degraded')return'\u{1F7E1} DEGRADED';
    return'\u{1F534} DOWN';
  }

  // === Fetch data ===
  function fetchData(){
    fetch('/api/status').then(function(r){return r.json();}).then(function(d){
      // Update 7-segment data
      P.w1State=d.wan1.state;P.w1Lat=d.wan1.latency_ms;P.w1Jit=d.wan1.jitter_ms;P.w1Loss=d.wan1.loss_pct;
      P.w1Down=d.wan1.down_mbps;P.w1Up=d.wan1.up_mbps;
      P.w2State=d.wan2.state;P.w2Lat=d.wan2.latency_ms;P.w2Jit=d.wan2.jitter_ms;P.w2Loss=d.wan2.loss_pct;
      P.w2Down=d.wan2.down_mbps;P.w2Up=d.wan2.up_mbps;
      P.lpState=d.local.state;P.lpLat=d.local.latency_ms;P.lpJit=d.local.jitter_ms;P.lpLoss=d.local.loss_pct;
      updLeds();
      updDisp();
      // Update timestamp
      if(d.timestamp){
        updateTime=new Date(d.timestamp);
        var el=document.getElementById('last-update');
        if(el)el.textContent=updateTime.toLocaleString();
      }
      // Update freshness timing constants from API
      if(d.freshness){
        F.greenFillEnd=d.freshness.green_fill_end;
        F.greenBufferEnd=d.freshness.green_buffer_end;
        F.yellowFillEnd=d.freshness.yellow_fill_end;
        F.yellowBufferEnd=d.freshness.yellow_buffer_end;
        F.redFillEnd=d.freshness.red_fill_end;
        F.redBufferEnd=d.freshness.red_buffer_end;
        F.fillDuration=d.freshness.fill_duration;
      }
      // Update table cells
      var $=function(id){return document.getElementById(id);};
      $('w1-state').innerHTML=stateHtml(d.wan1.state);
      $('w1-mon').textContent=d.wan1.monitor_ip||'';
      $('w1-gw').textContent=d.wan1.gateway_ip||'';
      $('w1-lip').textContent=d.wan1.local_ip||'';
      $('w1-loss').textContent=d.wan1.loss_pct+'%';
      $('w1-lat').textContent=d.wan1.latency_ms+' ms';
      $('w1-jit').textContent=d.wan1.jitter_ms+' ms';
      $('w1-down').textContent=d.wan1.down_mbps.toFixed(1)+' Mbps';
      $('w1-up').textContent=d.wan1.up_mbps.toFixed(1)+' Mbps';
      $('w2-state').innerHTML=stateHtml(d.wan2.state);
      $('w2-mon').textContent=d.wan2.monitor_ip||'';
      $('w2-gw').textContent=d.wan2.gateway_ip||'';
      $('w2-lip').textContent=d.wan2.local_ip||'';
      $('w2-loss').textContent=d.wan2.loss_pct+'%';
      $('w2-lat').textContent=d.wan2.latency_ms+' ms';
      $('w2-jit').textContent=d.wan2.jitter_ms+' ms';
      $('w2-down').textContent=d.wan2.down_mbps.toFixed(1)+' Mbps';
      $('w2-up').textContent=d.wan2.up_mbps.toFixed(1)+' Mbps';
      $('lp-state').innerHTML=stateHtml(d.local.state);
      $('lp-gw').textContent=d.router_ip||'';
      $('lp-loss').textContent=d.local.loss_pct+'%';
      $('lp-lat').textContent=d.local.latency_ms+' ms';
      $('lp-jit').textContent=d.local.jitter_ms+' ms';
    }).catch(function(e){console.error('Fetch error:',e);});
  }
  fetchData(); // Run immediately
  setInterval(fetchData,5000); // Then every 5 seconds
})();
</script>)";

    // Metrics table
    html += R"(
<h3>Network Metrics</h3>
<table>
  <tr>
    <th>Interface</th>
    <th>Description</th>
    <th>State</th>
    <th>Monitor IP</th>
    <th>Gateway IP</th>
    <th>Local IP</th>
    <th>Loss</th>
    <th>Latency</th>
    <th>Jitter</th>
    <th>Download</th>
    <th>Upload</th>
  </tr>
)";
    html += wan_metrics_row_html(1);
    html += wan_metrics_row_html(2);
    html += local_pinger_metrics_row_html();
    html += "</table>";

    html += R"(
</body></html>
)";

    return html;
}

// ---- 404 handler ----
static void handle_not_found(WebServer& server) {
    server.send(404, "text/plain", "Not found");
}

// ---- Helper: Parse JSON and update WAN metrics ----
static bool parse_wan_json(JsonObject& obj, int wan_id) {
    const char* state_str = obj["state"] | "down";
    WanState state = wan_state_from_string(state_str);
    uint8_t loss_pct = obj["loss_pct"] | 100;
    uint16_t latency_ms = obj["latency_ms"] | 0;
    uint16_t jitter_ms = obj["jitter_ms"] | 0;
    float down_mbps = obj["down_mbps"] | 0.0f;
    float up_mbps = obj["up_mbps"] | 0.0f;
    const char* local_ip = obj["local_ip"] | "";
    const char* gateway_ip = obj["gateway_ip"] | "";
    const char* monitor_ip = obj["monitor_ip"] | "";

    wan_metrics_update(wan_id, state, loss_pct, latency_ms, jitter_ms,
                       down_mbps, up_mbps, local_ip, gateway_ip, monitor_ip);

    // Update LEDs based on WAN
    if (wan_id == 1) {
        wan1_set_leds(state);
    } else if (wan_id == 2) {
        wan2_set_leds(state);
    }

    Serial.printf("WAN%d updated: state=%s loss=%d%% lat=%dms local=%s gw=%s\n",
                  wan_id, state_str, loss_pct, latency_ms, local_ip, gateway_ip);

    return true;
}

// ---- Handler: POST /api/wans (batch) ----
static void handle_wans_post(WebServer& server) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    // Extract top-level router info
    const char* router_ip = doc["router_ip"] | "";
    const char* timestamp = doc["timestamp"] | "";
    wan_metrics_set_router_info(router_ip, timestamp);

    // Process wan1 if present
    if (doc["wan1"].is<JsonObject>()) {
        JsonObject wan1 = doc["wan1"];
        parse_wan_json(wan1, 1);
    }

    // Process wan2 if present
    if (doc["wan2"].is<JsonObject>()) {
        JsonObject wan2 = doc["wan2"];
        parse_wan_json(wan2, 2);
    }

    // Build response with all WANs
    JsonDocument resp;
    resp["status"] = "ok";

    for (int i = 1; i <= MAX_WANS; i++) {
        const WanMetrics& m = wan_metrics_get(i);
        JsonObject wan = resp["wan" + String(i)].to<JsonObject>();
        wan["state"] = wan_state_to_string(m.state);
        wan["loss_pct"] = m.loss_pct;
        wan["latency_ms"] = m.latency_ms;
        wan["jitter_ms"] = m.jitter_ms;
        wan["down_mbps"] = m.down_mbps;
        wan["up_mbps"] = m.up_mbps;
    }

    String output;
    serializeJson(resp, output);
    server.send(200, "application/json", output);
}

// ---- Handler: GET /api/status ----
static void handle_status_get(WebServer& server) {
    const WanMetrics& w1 = wan_metrics_get(1);
    const WanMetrics& w2 = wan_metrics_get(2);
    const LocalPingerMetrics& lp = local_pinger_get();
    const char* timestamp = wan_metrics_get_timestamp();

    JsonDocument doc;
    doc["timestamp"] = timestamp;
    doc["router_ip"] = wan_metrics_get_router_ip();

    JsonObject wan1 = doc["wan1"].to<JsonObject>();
    wan1["state"] = wan_state_to_string(w1.state);
    wan1["latency_ms"] = w1.latency_ms;
    wan1["jitter_ms"] = w1.jitter_ms;
    wan1["loss_pct"] = w1.loss_pct;
    wan1["down_mbps"] = w1.down_mbps;
    wan1["up_mbps"] = w1.up_mbps;
    wan1["monitor_ip"] = w1.monitor_ip;
    wan1["gateway_ip"] = w1.gateway_ip;
    wan1["local_ip"] = w1.local_ip;

    JsonObject wan2 = doc["wan2"].to<JsonObject>();
    wan2["state"] = wan_state_to_string(w2.state);
    wan2["latency_ms"] = w2.latency_ms;
    wan2["jitter_ms"] = w2.jitter_ms;
    wan2["loss_pct"] = w2.loss_pct;
    wan2["down_mbps"] = w2.down_mbps;
    wan2["up_mbps"] = w2.up_mbps;
    wan2["monitor_ip"] = w2.monitor_ip;
    wan2["gateway_ip"] = w2.gateway_ip;
    wan2["local_ip"] = w2.local_ip;

    JsonObject local = doc["local"].to<JsonObject>();
    local["state"] = wan_state_to_string(lp.state);
    local["latency_ms"] = lp.latency_ms;
    local["jitter_ms"] = lp.jitter_ms;
    local["loss_pct"] = lp.loss_pct;

    // Freshness bar timing constants (in seconds, matching hardware)
    JsonObject freshness = doc["freshness"].to<JsonObject>();
    freshness["green_fill_end"] = FRESHNESS_GREEN_FILL_END_MS / 1000;
    freshness["green_buffer_end"] = FRESHNESS_GREEN_BUFFER_END_MS / 1000;
    freshness["yellow_fill_end"] = FRESHNESS_YELLOW_FILL_END_MS / 1000;
    freshness["yellow_buffer_end"] = FRESHNESS_YELLOW_BUFFER_END_MS / 1000;
    freshness["red_fill_end"] = FRESHNESS_RED_FILL_END_MS / 1000;
    freshness["red_buffer_end"] = FRESHNESS_RED_BUFFER_END_MS / 1000;
    freshness["fill_duration"] = FRESHNESS_FILL_DURATION_MS / 1000;
    freshness["led_count"] = TOTAL_LEDS;

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

// ---- Public: wire up all routes ----
void setup_routes(WebServer& server) {
    // Root: status page
    server.on("/", [&server]() {
        server.send(200, "text/html", root_page_html());
    });

    // JSON API endpoints
    server.on("/api/status", HTTP_GET, [&server]() {
        handle_status_get(server);
    });
    server.on("/api/wans", HTTP_POST, [&server]() {
        handle_wans_post(server);
    });

    // Favicons
    server.on("/favicon-green.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_GREEN);
    });
    server.on("/favicon-yellow.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_YELLOW);
    });
    server.on("/favicon-red.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_RED);
    });
    server.on("/favicon.svg", [&server]() {
        server.send(200, "image/svg+xml", FAVICON_GREEN);  // Default to green
    });
    server.on("/favicon.ico", [&server]() {
        server.send(204);  // Some browsers still request .ico
    });

    server.onNotFound([&server]() {
        handle_not_found(server);
    });
}
