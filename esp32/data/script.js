(function(){
  // === Shared state ===
  var updateTime = null;

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
    if (!el) return;
    for(var i=0;i<4;i++)el.appendChild(mkDigit());
  }
  function setDisplay(id,prefix,val){
    var el=document.getElementById(id);
    if (!el) return;
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
    if (!el) return;
    var digits=el.querySelectorAll('.digit');
    for(var i=0;i<4;i++){
      var digit=digits[i];
      ['a','b','c','d','e','f','g'].forEach(function(seg){
        digit.querySelector('.seg-'+seg).classList.toggle('on',seg==='g');
      });
      digit.querySelector('.seg-dp').classList.toggle('on',false);
    }
  }
  ['w1-pkt','w1-bw','w2-pkt','w2-bw','lp-pkt','lp-bw'].forEach(initDisplay);

  var pktIdx=0,bwIdx=0;
  var P=document.getElementById('seg-panel').dataset;

  // === Stale state helper ===
  function isStale(){
    if(!updateTime)return true;
    var elapsed=(Date.now()-updateTime.getTime())/1000;
    return elapsed>=F.redBufferEnd;
  }

  // === State LED update (single bicolor LED per row) ===
  function setLeds(prefix,state,stale){
    var led=document.getElementById(prefix+'-led');
    if(stale){
      led.className='state-led blink-red';
    }else if(state==='up'){
      led.className='state-led green';
    }else if(state==='degraded'){
      led.className='state-led yellow';
    }else{
      led.className='state-led red';
    }
  }
  function updLeds(){
    var stale=isStale();
    setLeds('w1',P.w1State,stale);
    setLeds('w2',P.w2State,stale);
    setLeds('lp',P.lpState,false);  // Local pinger doesn't depend on pfSense freshness
  }
  updLeds();

  function updDisp(){
    var pktM=['L','J','P'],bwM=['d','U'];
    var pm=pktM[pktIdx],bm=bwM[bwIdx];
    var stale=isStale();
    // Show dashes when stale or when WAN is down
    if(stale||P.w1State==='down'){
      setDisplayDashes('w1-pkt');setDisplayDashes('w1-bw');
    }else{
      setDisplay('w1-pkt',pm,[P.w1Lat,P.w1Jit,P.w1Loss][pktIdx]);
      setDisplay('w1-bw',bm,[P.w1Down,P.w1Up][bwIdx]);
    }
    if(stale||P.w2State==='down'){
      setDisplayDashes('w2-pkt');setDisplayDashes('w2-bw');
    }else{
      setDisplay('w2-pkt',pm,[P.w2Lat,P.w2Jit,P.w2Loss][pktIdx]);
      setDisplay('w2-bw',bm,[P.w2Down,P.w2Up][bwIdx]);
    }
    // Local pinger doesn't depend on pfSense freshness
    if(P.lpState==='down'){
      setDisplayDashes('lp-pkt');
    }else{
      setDisplay('lp-pkt',pm,[P.lpLat,P.lpJit,P.lpLoss][pktIdx]);
    }
    // Local bandwidth is sum of WAN1 + WAN2 (show dashes if stale)
    if(stale){
      setDisplayDashes('lp-bw');
    }else{
      setDisplay('lp-bw',bm,[P.lpDown,P.lpUp][bwIdx]);
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

  // === Dynamic favicon based on local pinger state ===
  var faviconLink = document.querySelector('link[rel="icon"]');
  function updateFavicon(state) {
    if (!faviconLink) return;
    var color = state === 'up' ? 'green' : state === 'degraded' ? 'yellow' : 'red';
    faviconLink.href = '/favicon-' + color + '.svg';
  }

  // === Bandwidth source selection ===
  function getBwSource(){
    var sel=document.querySelector('input[name="bw-source"]:checked, input[name="bw-source-m"]:checked');
    return sel?sel.value:'1m';
  }
  function getBwValues(wan,src){
    if(src==='15s')return{down:wan.down_mbps,up:wan.up_mbps};
    if(src==='5m')return{down:wan.down_5m,up:wan.up_5m};
    if(src==='15m')return{down:wan.down_15m,up:wan.up_15m};
    return{down:wan.down_1m,up:wan.up_1m}; // default 1m
  }

  // === Formatting helpers ===
  var $=function(id){return document.getElementById(id);};
  var setText=function(id,val){var e=$(id);if(e)e.textContent=val;var m=$(id+'-m');if(m)m.textContent=val;};
  // Pad number to 3 chars with leading non-breaking spaces, add " ms"
  var fmtMs=function(n){var s=String(n);while(s.length<3)s='\u00A0'+s;return s+' ms';};
  // Pad number to 3 chars with leading non-breaking spaces, add "%"
  var fmtPct=function(n){var s=String(n);while(s.length<3)s='\u00A0'+s;return s+'%';};
  // Pad number to 5 chars (XXX.X format) with leading non-breaking spaces
  var padBw=function(n){var s=n.toFixed(1);while(s.length<5)s='\u00A0'+s;return s;};
  // Format bandwidth pair
  var fmtBw=function(down,up){return padBw(down)+'/'+padBw(up);};

  // === Fetch data ===
  function fetchData(){
    fetch('/api/status').then(function(r){
      if(!r.ok)throw new Error('HTTP '+r.status);
      return r.json();
    }).then(function(d){
      if(!d||!d.wan1||!d.wan2||!d.local)throw new Error('Invalid response');
      // Update 7-segment data using selected bandwidth source
      var bwSrc=getBwSource();
      var w1bw=getBwValues(d.wan1,bwSrc);
      var w2bw=getBwValues(d.wan2,bwSrc);
      P.w1State=d.wan1.state;P.w1Lat=d.wan1.latency_ms;P.w1Jit=d.wan1.jitter_ms;P.w1Loss=d.wan1.loss_pct;
      P.w1Down=w1bw.down.toFixed(1);P.w1Up=w1bw.up.toFixed(1);
      P.w2State=d.wan2.state;P.w2Lat=d.wan2.latency_ms;P.w2Jit=d.wan2.jitter_ms;P.w2Loss=d.wan2.loss_pct;
      P.w2Down=w2bw.down.toFixed(1);P.w2Up=w2bw.up.toFixed(1);
      P.lpState=d.local.state;P.lpLat=d.local.latency_ms;P.lpJit=d.local.jitter_ms;P.lpLoss=d.local.loss_pct;
      P.lpDown=(w1bw.down+w2bw.down).toFixed(1);P.lpUp=(w1bw.up+w2bw.up).toFixed(1);
      updLeds();
      updDisp();
      updateFavicon(d.local.state);
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
      // Update table cells (both wide and narrow versions)
      $('w1-state').innerHTML=stateHtml(d.wan1.state);
      $('w1-mon').textContent=d.wan1.monitor_ip||'';
      $('w1-gw').textContent=d.wan1.gateway_ip||'';
      $('w1-lip').textContent=d.wan1.local_ip||'';
      setText('w1-loss',fmtPct(d.wan1.loss_pct));
      setText('w1-lat',fmtMs(d.wan1.latency_ms));
      setText('w1-jit',fmtMs(d.wan1.jitter_ms));
      setText('w1-bw15',fmtBw(d.wan1.down_mbps,d.wan1.up_mbps));
      setText('w1-avg1',fmtBw(d.wan1.down_1m,d.wan1.up_1m));
      setText('w1-avg5',fmtBw(d.wan1.down_5m,d.wan1.up_5m));
      setText('w1-avg15',fmtBw(d.wan1.down_15m,d.wan1.up_15m));
      $('w2-state').innerHTML=stateHtml(d.wan2.state);
      $('w2-mon').textContent=d.wan2.monitor_ip||'';
      $('w2-gw').textContent=d.wan2.gateway_ip||'';
      $('w2-lip').textContent=d.wan2.local_ip||'';
      setText('w2-loss',fmtPct(d.wan2.loss_pct));
      setText('w2-lat',fmtMs(d.wan2.latency_ms));
      setText('w2-jit',fmtMs(d.wan2.jitter_ms));
      setText('w2-bw15',fmtBw(d.wan2.down_mbps,d.wan2.up_mbps));
      setText('w2-avg1',fmtBw(d.wan2.down_1m,d.wan2.up_1m));
      setText('w2-avg5',fmtBw(d.wan2.down_5m,d.wan2.up_5m));
      setText('w2-avg15',fmtBw(d.wan2.down_15m,d.wan2.up_15m));
      $('lp-state').innerHTML=stateHtml(d.local.state);
      $('lp-mon').textContent=d.local.monitor_ip||'';
      $('lp-gw').textContent=d.router_ip||'';
      $('lp-lip').textContent=d.local.local_ip||'';
      setText('lp-lat',fmtMs(d.local.latency_ms));
      setText('lp-jit',fmtMs(d.local.jitter_ms));
      setText('lp-loss',fmtPct(d.local.loss_pct));
      // Sum WAN1 + WAN2 bandwidth for local row
      setText('lp-bw15',fmtBw(d.wan1.down_mbps+d.wan2.down_mbps,d.wan1.up_mbps+d.wan2.up_mbps));
      setText('lp-avg1',fmtBw(d.wan1.down_1m+d.wan2.down_1m,d.wan1.up_1m+d.wan2.up_1m));
      setText('lp-avg5',fmtBw(d.wan1.down_5m+d.wan2.down_5m,d.wan1.up_5m+d.wan2.up_5m));
      setText('lp-avg15',fmtBw(d.wan1.down_15m+d.wan2.down_15m,d.wan1.up_15m+d.wan2.up_15m));
    }).catch(function(e){console.error('Fetch error:',e);});
  }
  fetchData(); // Run immediately
  setInterval(fetchData,5000); // Then every 5 seconds

  // === Brightness dial control ===
  var brightnessDial = document.getElementById('brightness-dial');
  var brightnessVal = document.getElementById('brightness-val');
  var brightnessPotStatus = document.getElementById('brightness-pot-status');
  var currentBrightness = 8;
  var potLevel = 8;

  // Dial rotation: 0=min (-135deg), 15=max (+135deg), total 270deg range
  function brightnessToAngle(b) { return -135 + (b / 15) * 270; }
  function angleToBrightness(a) {
    var b = Math.round(((a + 135) / 270) * 15);
    return Math.max(0, Math.min(15, b));
  }

  function updateDialRotation() {
    brightnessDial.style.transform = 'rotate(' + brightnessToAngle(currentBrightness) + 'deg)';
  }

  function updateBrightnessUI() {
    brightnessVal.textContent = currentBrightness;
    updateDialRotation();

    // Show pot level and override status
    var potText = 'Pot: ' + potLevel;
    brightnessPotStatus.textContent = potText;
    brightnessPotStatus.classList.toggle('override', currentBrightness !== potLevel);
  }

  function fetchBrightnessState() {
    fetch('/api/brightness').then(function(r) { return r.json(); }).then(function(d) {
      currentBrightness = d.brightness;
      potLevel = d.pot_level;
      updateBrightnessUI();
    }).catch(function(e) { console.error('Brightness fetch error:', e); });
  }

  function postBrightness(val) {
    currentBrightness = parseInt(val);
    updateBrightnessUI();
    fetch('/api/brightness', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({brightness: currentBrightness})
    }).catch(function(e) { console.error('Brightness error:', e); });
  }

  // Dial drag handling
  var dialDragging = false;
  var dialCenterX, dialCenterY;

  function getAngleFromEvent(e) {
    var x = (e.touches ? e.touches[0].clientX : e.clientX) - dialCenterX;
    var y = (e.touches ? e.touches[0].clientY : e.clientY) - dialCenterY;
    var angle = Math.atan2(x, -y) * (180 / Math.PI);
    return Math.max(-135, Math.min(135, angle));
  }

  brightnessDial.addEventListener('mousedown', startDrag);
  brightnessDial.addEventListener('touchstart', startDrag);

  function startDrag(e) {
    e.preventDefault();
    dialDragging = true;
    var rect = brightnessDial.getBoundingClientRect();
    dialCenterX = rect.left + rect.width / 2;
    dialCenterY = rect.top + rect.height / 2;
    document.addEventListener('mousemove', onDrag);
    document.addEventListener('touchmove', onDrag);
    document.addEventListener('mouseup', endDrag);
    document.addEventListener('touchend', endDrag);
  }

  function onDrag(e) {
    if (!dialDragging) return;
    var angle = getAngleFromEvent(e);
    var newBrightness = angleToBrightness(angle);
    if (newBrightness !== currentBrightness) {
      currentBrightness = newBrightness;
      updateBrightnessUI();
    }
  }

  function endDrag(e) {
    if (dialDragging) {
      dialDragging = false;
      postBrightness(currentBrightness);
    }
    document.removeEventListener('mousemove', onDrag);
    document.removeEventListener('touchmove', onDrag);
    document.removeEventListener('mouseup', endDrag);
    document.removeEventListener('touchend', endDrag);
  }

  // Load initial brightness and poll for changes
  fetchBrightnessState();
  setInterval(fetchBrightnessState, 2000);

  // === Display power control ===
  var powerCheckbox = document.getElementById('power-checkbox');
  var switchStatus = document.getElementById('switch-status');
  var displaysOn = true;
  var switchPosition = true;

  function updatePowerUI() {
    powerCheckbox.checked = displaysOn;
    // Show switch position (color indicates override)
    switchStatus.textContent = 'Switch: ' + (switchPosition ? 'ON' : 'OFF');
    switchStatus.classList.toggle('override', displaysOn !== switchPosition);
  }

  function fetchPowerState() {
    fetch('/api/display-power').then(function(r) { return r.json(); }).then(function(d) {
      displaysOn = d.on;
      switchPosition = d.switch_position;
      updatePowerUI();
    }).catch(function(e) { console.error('Power fetch error:', e); });
  }

  powerCheckbox.addEventListener('change', function() {
    displaysOn = powerCheckbox.checked;
    updatePowerUI(); // Update optimistically
    fetch('/api/display-power', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({on: displaysOn})
    }).catch(function(e) { console.error('Power toggle error:', e); });
  });

  // Load initial power state and poll for changes
  fetchPowerState();
  setInterval(fetchPowerState, 2000);

  // === Bandwidth source selection ===
  function syncBwRadios(source) {
    var radio = document.getElementById('bw-' + source);
    var radioM = document.getElementById('bw-' + source + '-m');
    if (radio) radio.checked = true;
    if (radioM) radioM.checked = true;
  }

  function fetchBwSource() {
    fetch('/api/bw-source').then(function(r) { return r.json(); }).then(function(d) {
      syncBwRadios(d.source);
    }).catch(function(e) { console.error('BW source fetch error:', e); });
  }

  function postBwSource(source) {
    syncBwRadios(source);
    fetch('/api/bw-source', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({source: source})
    }).catch(function(e) { console.error('BW source post error:', e); });
  }

  // Add event listeners to bandwidth source radio buttons (both wide and narrow)
  document.querySelectorAll('input[name="bw-source"], input[name="bw-source-m"]').forEach(function(radio) {
    radio.addEventListener('change', function() {
      postBwSource(this.value);
    });
  });

  // Load initial bandwidth source
  fetchBwSource();
})();
