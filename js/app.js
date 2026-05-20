/**
 * Votol Dash Pro v1.1 - ULTIMATE TEST VERSION
 * Fitur: Dashboard, Mode, GPX (Votol Speed), Wake Lock, Glow-Ball, BMS Detail, & Settings Alert
 */

const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

let isConnected = false;
let bluetoothDevice, rxChar, txChar;
let rxBuffer = "";

let speedHistory = Array(30).fill(0);
let currentHistory = Array(30).fill(0);
let speedChart = null;
let currentChart = null;

let isRecording = false;
let recordStartTime = 0;
let recordInterval;
let gpxDataPoints = []; 
let currentGPS = { lat: 0, lon: 0, alt: 0 };
let wakeLock = null;

// --- GPS & WAKE LOCK ---
if ("geolocation" in navigator) {
    navigator.geolocation.watchPosition(
        (pos) => {
            currentGPS = { lat: pos.coords.latitude, lon: pos.coords.longitude, alt: pos.coords.altitude || 0 };
            const gpsStatus = document.getElementById('gps-status');
            if (gpsStatus) gpsStatus.innerText = `GPS: Fixed (${currentGPS.lat.toFixed(4)}, ${currentGPS.lon.toFixed(4)})`;
        },
        (err) => console.error(err),
        { enableHighAccuracy: true }
    );
}

async function requestWakeLock() {
    try {
        if ('wakeLock' in navigator) wakeLock = await navigator.wakeLock.request('screen');
    } catch (err) { console.error(err); }
}

// --- TELEMETRY COLLECTION ---
function collectTelemetry(votolData) {
    if (!isRecording) return;
    gpxDataPoints.push({
        lat: currentGPS.lat,
        lon: currentGPS.lon,
        ele: currentGPS.alt || 0,
        time: new Date().toISOString(),
        speed: parseFloat(votolData.speed) || 0,
        rpm: parseInt(votolData.rpm) || 0,
        soc: votolData.soc || 0,
        current: votolData.amps || 0 
    });
}

// --- NAVIGATION ---
async function loadPage(pageName, element) {
    try {
        const response = await fetch(`${pageName}.html`);
        const html = await response.text();
        document.getElementById('page-container').innerHTML = html;
        lucide.createIcons();
        if (pageName === 'battery') generateCells();
        if (pageName === 'trip') initCharts();
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        if (element) element.classList.add('active');
    } catch (error) { console.error(error); }
}

// --- UPDATE UI ---
function updateUI(data) {
    if (!isConnected || !data) return;
    
    collectTelemetry(data);

    // Logs & History
    const logBox = document.getElementById('log-box');
    if (logBox) {
        logBox.innerText = `[${new Date().toLocaleTimeString()}] ${JSON.stringify(data)}\n` + logBox.innerText.substring(0, 500);
    }
    speedHistory.push(data.speed || 0);
    currentHistory.push(Math.abs(data.amps || 0));
    speedHistory.shift(); currentHistory.shift();
    if (speedChart && currentChart) {
        speedChart.data.datasets[0].data = [...speedHistory];
        currentChart.data.datasets[0].data = [...currentHistory];
        speedChart.update('none'); currentChart.update('none');
    }

    // 1. MODE BERKENDARA
    const modeEl = document.getElementById('mode-text');
    if (modeEl && data.mode) {
        modeEl.innerText = data.mode;
        const m = data.mode.toUpperCase();
        if (m === "SPORT") modeEl.style.backgroundColor = "#d29922";
        else if (m === "DRIVE") modeEl.style.backgroundColor = "#238636";
        else if (m === "REVERSE") modeEl.style.backgroundColor = "#bc8cff";
        else if (m === "PARK") modeEl.style.backgroundColor = "#30363d";
        else modeEl.style.backgroundColor = "#f85149";
    }

    // 2. GAUGE & BOLA BIRU (GLOW-BALL)
    const rpm = data.rpm || 0;
    const circle = document.getElementById('gauge-circle');
    const ball = document.getElementById('glow-ball');
    if (circle) {
        const deg = Math.min((rpm / 1600) * 360, 360);
        circle.style.setProperty('--deg', `${deg}deg`);
        circle.style.background = `radial-gradient(var(--card-bg) 64%, transparent 66%), conic-gradient(from 180deg, var(--cyan) ${deg}deg, #21262d 0deg)`;
        if (ball) {
            ball.style.display = rpm > 0 ? "block" : "none";
            const rad = (deg + 180 - 90) * (Math.PI / 180);
            ball.style.transform = `translate(calc(-50% + ${Math.cos(rad)*100}px), calc(-50% + ${Math.sin(rad)*100}px))`;
        }
    }

    // 3. MAPPING PARAMETER
    const h = data.health || {};
    const t = data.trip || {};
    const vals = {
        'speed': data.speed || 0, 'rpm-val': rpm,
        'soc-dash': (data.soc || 0) + "%", 'soc-batt': (data.soc || 0) + "%",
        'soh-val': (h.soh || 0) + "%", 'v-val': (data.volts || 0).toFixed(1) + "V",
        'a-val': (data.amps || 0).toFixed(1) + "A",
        'cycle-val': (h.cycles || 0),
        'range-val': (t.range || 0) + " km", 'trip-val': (t.km || 0).toFixed(1) + " km",
        'cap-rem': (h.remainCap || 0).toFixed(1) + " Ah",
        'cap-full': (h.fullCap || 0).toFixed(1) + " Ah",
        'trip-dist-large': (t.km || 0).toFixed(1) + " km",
        'avg-wh': (t.avg || 0).toFixed(1),
        'est-range-trip': (t.range || 0) + " km",
        't-ecu': (data.temps?.ctrl || 0) + "°", 't-motor': (data.temps?.motor || 0) + "°", 't-batt': (data.temps?.batt || 0) + "°"
    };

    for (const [id, val] of Object.entries(vals)) {
        const el = document.getElementById(id);
        if (el) el.innerText = val;
    }

    // 4. BMS CELLS
    if (data.cells && data.cells.length > 0) {
        const cellVolts = data.cells.map(mv => mv / 1000);
        const maxV = Math.max(...cellVolts);
        const minV = Math.min(...cellVolts);
        const deltaEl = document.getElementById('cell-delta');
        if (deltaEl) deltaEl.innerText = (maxV - minV).toFixed(3) + "V";

        cellVolts.forEach((vol, i) => {
            const elText = document.getElementById(`c${i+1}-v`);
            const elBar = document.getElementById(`c${i+1}-bar`);
            if (elText) elText.innerText = vol.toFixed(3) + "V";
            if (elBar) {
                let pct = ((vol - 2.5) / (3.65 - 2.5)) * 100;
                elBar.style.width = Math.max(0, Math.min(100, pct)) + "%";
                if (vol === maxV) elBar.style.backgroundColor = "#d29922";
                else if (vol === minV) elBar.style.backgroundColor = "#f85149";
                else elBar.style.backgroundColor = "#3fb950";
            }
        });
    }

    // 5. SINKRONISASI PENGATURAN UI (SETTINGS) DENGAN ESP32 LIVE
    if (data.set) {
        let popCb = document.getElementById('modePopupEn');
        let slpCb = document.getElementById('autoSleepEn');
        let olECb = document.getElementById('oledAlarmEn');
        let bzECb = document.getElementById('buzzAlarmEn');
        
        // Pengecekan elemen wajib dilakukan agar tidak error saat berada di halaman selain Settings
        if (popCb && popCb.checked !== (data.set.pop === 1)) popCb.checked = (data.set.pop === 1);
        if (slpCb && slpCb.checked !== (data.set.slp === 1)) slpCb.checked = (data.set.slp === 1);

        // PERBAIKAN: Checkbox OLED & BUZZER
        if (olECb) {
            // Berikan fungsi onchange otomatis agar saat dicentang/uncheck langsung tersimpan
            if (!olECb.onchange) olECb.onchange = sendAlarmOled; 
            // Jangan timpa centangnya jika elemen ini sedang ditekan/difokuskan oleh user
            if (document.activeElement !== olECb && olECb.checked !== (data.set.olE === 1)) {
                olECb.checked = (data.set.olE === 1);
            }
        }

        if (bzECb) {
            if (!bzECb.onchange) bzECb.onchange = sendAlarmBuzzer;
            if (document.activeElement !== bzECb && bzECb.checked !== (data.set.bzE === 1)) {
                bzECb.checked = (data.set.bzE === 1);
            }
        }

        // Update input angka limit alarm hanya jika user sedang tidak mengetik di dalamnya
        let olLInp = document.getElementById('oledAlarmLimit');
        if (olLInp && document.activeElement !== olLInp && olLInp.value != data.set.olL) {
            olLInp.value = data.set.olL;
        }

        let bzLInp = document.getElementById('buzzAlarmLimit');
        if (bzLInp && document.activeElement !== bzLInp && bzLInp.value != data.set.bzL) {
            bzLInp.value = data.set.bzL;
        }
    }

    // 6. UPDATE INDIKATOR KALIBRASI (AGAR TERLIHAT DI UI)
    if (data.cal !== undefined) {
        let calEl = document.getElementById('calib-status');
        if (calEl) calEl.innerText = data.cal.toFixed(3) + "x";
    }
}

// --- GPX SAVE ---
function saveGPX() {
    if (gpxDataPoints.length === 0) return;
    
    const now = new Date();
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, '0');
    const day = String(now.getDate()).padStart(2, '0');
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    const fileName = `Votol-${year}${month}${day}_${hours}${minutes}${seconds}.gpx`;

    let gpx = `<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="Votol Dash Pro" xmlns="http://www.topografix.com/GPX/1/1" xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v2">
<trk><name>Votol Telemetry Session</name><trkseg>`;

    gpxDataPoints.forEach(p => {
        const speedMS = (p.speed / 3.6).toFixed(3);
        
        gpx += `
<trkpt lat="${p.lat}" lon="${p.lon}">
    <ele>${p.ele.toFixed(2)}</ele>
    <time>${p.time}</time>
    <extensions>
        <gpxtpx:TrackPointExtension>
            <gpxtpx:speed>${speedMS}</gpxtpx:speed>
            <gpxtpx:hr>${p.rpm}</gpxtpx:hr>
            <gpxtpx:cad>${p.soc}</gpxtpx:cad>
        </gpxtpx:TrackPointExtension>
        <real_rpm>${p.rpm}</real_rpm>
        <real_soc>${p.soc}</real_soc>
        <real_current>${p.current || 0}</real_current>
    </extensions>
</trkpt>`;
    });

    gpx += `\n</trkseg></trk></gpx>`;
    
    const blob = new Blob([gpx], { type: 'application/gpx+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; 
    a.download = fileName; 
    a.click();
    URL.revokeObjectURL(url);
}

async function toggleRecord() {
    const btn = document.getElementById('recordBtn');
    const timerEl = document.getElementById('record-timer');
    if (!isRecording) {
        await requestWakeLock();
        isRecording = true; gpxDataPoints = []; recordStartTime = Date.now();
        btn.style.background = "#f85149";
        recordInterval = setInterval(() => {
            const elapsed = Date.now() - recordStartTime;
            const h = Math.floor(elapsed / 3600000).toString().padStart(2, '0');
            const m = Math.floor((elapsed % 3600000) / 60000).toString().padStart(2, '0');
            const s = Math.floor((elapsed % 60000) / 1000).toString().padStart(2, '0');
            if (timerEl) timerEl.innerText = `${h}:${m}:${s}`;
        }, 1000);
    } else {
        isRecording = false; clearInterval(recordInterval);
        if (wakeLock) wakeLock.release().then(() => wakeLock = null);
        btn.style.background = "var(--orange)";
        saveGPX();
    }
}

// --- BLUETOOTH & SETTINGS ---
let notificationHandler = null;

async function toggleConnect() {
    if (isConnected) {
        try {
            if (bluetoothDevice && bluetoothDevice.gatt.connected) {
                bluetoothDevice.gatt.disconnect();
            }
        } catch (e) { console.error(e); }
        return;
    }

    try {
        bluetoothDevice = await navigator.bluetooth.requestDevice({
            filters: [{ name: 'Votol_BLE' }],
            optionalServices: [SERVICE_UUID]
        });

        bluetoothDevice.addEventListener('gattserverdisconnected', onDisconnected);
        const server = await bluetoothDevice.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        txChar = await service.getCharacteristic(TX_CHAR_UUID);
        rxChar = await service.getCharacteristic(RX_CHAR_UUID);
        await txChar.startNotifications();

        if (notificationHandler) {
            txChar.removeEventListener('characteristicvaluechanged', notificationHandler);
        }

        notificationHandler = (e) => {
            try {
                let chunk = new TextDecoder().decode(e.target.value);
                rxBuffer += chunk;
                if (rxBuffer.length > 5000) rxBuffer = ""; // Reset jika data tumpuk rusak

                if (rxBuffer.includes('\n')) {
                    let lines = rxBuffer.split('\n');
                    rxBuffer = lines.pop();
                    lines.forEach(line => {
                        if (!line.trim()) return;
                        try {
                            const json = JSON.parse(line);
                            updateUI(json);
                        } catch (err) { }
                    });
                }
            } catch (err) { console.error("Notification Error:", err); }
        };

        txChar.addEventListener('characteristicvaluechanged', notificationHandler);
        setStatus(true);
        console.log("Bluetooth Connected");

    } catch (e) {
        console.error(e);
        alert("Koneksi Gagal: " + e);
        onDisconnected();
    }
}

function setStatus(status) {
    isConnected = status;
    const btn = document.getElementById('connectBtn');
    if (btn) btn.innerText = status ? "DISCONNECT" : "CONNECT";
    
    const st = document.getElementById('conn-status');
    if (st) {
        st.innerText = status ? "● ONLINE" : "● OFFLINE";
        st.style.color = status ? "#3fb950" : "#f85149";
    }
}

function onDisconnected() {
    console.log("Bluetooth Disconnected");
    if (txChar && notificationHandler) {
        txChar.removeEventListener('characteristicvaluechanged', notificationHandler);
    }
    rxChar = null; txChar = null; rxBuffer = "";
    setStatus(false);
}

// ===============================
// FUNGSI SETTINGS (DIKIRIM KE ESP32)
// ===============================

async function sendSplash() {
    if (!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    const v = document.getElementById('splashInput').value.trim();
    if (v) { try { await rxChar.writeValue(new TextEncoder().encode(`SPLASH,${v}`)); alert("Splash Name Diperbarui!"); } catch(e) { alert(e); } }
}

async function syncTime() {
    if (!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    try {
        const n = new Date();
        const cmd = `TIME,${n.getFullYear()},${n.getMonth()+1},${n.getDate()},${n.getHours()},${n.getMinutes()},${n.getSeconds()}`;
        await rxChar.writeValue(new TextEncoder().encode(cmd));
        alert("Waktu Berhasil Disinkronkan!");
    } catch(e) { alert(e); }
}

async function sendAlarmOled() {
    if (!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    const en = document.getElementById('oledAlarmEn').checked ? 1 : 0;
    const limit = document.getElementById('oledAlarmLimit').value;
    const cmd = `ALARM,OLED,${en},${limit}`;
    try { await rxChar.writeValue(new TextEncoder().encode(cmd)); alert("OLED Warning berhasil disimpan!"); } catch (e) { alert("Gagal: " + e); }
}

async function sendAlarmBuzzer() {
    if (!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    const en = document.getElementById('buzzAlarmEn').checked ? 1 : 0;
    const limit = document.getElementById('buzzAlarmLimit').value;
    const cmd = `ALARM,BUZZ,${en},${limit}`;
    try { await rxChar.writeValue(new TextEncoder().encode(cmd)); alert("Buzzer Warning berhasil disimpan!"); } catch (e) { alert("Gagal: " + e); }
}

async function sendTripCalibration() {
    if(!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    let real = parseFloat(document.getElementById('realTrip').value);
    let dash = parseFloat(document.getElementById('dashTrip').value);
    
    if(isNaN(real) || isNaN(dash) || dash === 0) {
        alert("Masukkan angka yang benar! Jarak dashboard tidak boleh 0.");
        return;
    }

    let factor = real / dash; 
    if(factor < 0.5 || factor > 2.0) {
        alert("Perbedaan jarak terlalu ekstrim! Pastikan inputnya benar.");
        return;
    }

    let command = "CALIB," + factor.toFixed(4);
    try { 
        await rxChar.writeValue(new TextEncoder().encode(command)); 
        alert("Berhasil! Jarak dan Kecepatan dashboard sekarang dikali " + factor.toFixed(4) + " agar akurat.");
    } catch(e) { alert("Gagal kalibrasi: " + e); }
}

async function sendModeToggle() {
    if(!rxChar) return;
    let en = document.getElementById('modePopupEn').checked ? 1 : 0;
    let command = `SET,MODE,${en}`;
    try { await rxChar.writeValue(new TextEncoder().encode(command)); } catch(e) { console.error(e); }
}

async function sendSleepToggle() {
    if(!rxChar) return;
    let en = document.getElementById('autoSleepEn').checked ? 1 : 0;
    let command = `SET,SLEEP,${en}`;
    try { await rxChar.writeValue(new TextEncoder().encode(command)); } catch(e) { console.error(e); }
}

function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.children.length > 0) return;
    for (let i = 1; i <= 23; i++) {
        grid.innerHTML += `<div class="cell-card"><div class="cell-info"><small class="cell-id">C${i}</small><b id="c${i}-v">0.000V</b></div><div class="cell-bar-bg"><div id="c${i}-bar" class="cell-bar-fill"></div></div></div>`;
    }
}

function initCharts() {
    const ctxS = document.getElementById('speedChart')?.getContext('2d');
    const ctxC = document.getElementById('currentChart')?.getContext('2d');
    if (!ctxS || !ctxC) return;
    const opt = { responsive:true, maintainAspectRatio:false, scales:{x:{display:false}, y:{display:false}}, plugins:{legend:{display:false}}, elements:{line:{tension:0.4, borderWidth:2}, point:{radius:0}} };
    speedChart = new Chart(ctxS, { type:'line', data:{labels:Array(30).fill(''), datasets:[{data:[...speedHistory], borderColor:'#d29922', fill:true, backgroundColor:'rgba(210,153,34,0.1)'}]}, options:opt });
    currentChart = new Chart(ctxC, { type:'line', data:{labels:Array(30).fill(''), datasets:[{data:[...currentHistory], borderColor:'#58a6ff', fill:true, backgroundColor:'rgba(88,166,255,0.1)'}]}, options:opt });
}

window.addEventListener('DOMContentLoaded', () => loadPage('dash'));
setInterval(() => { const clock = document.getElementById('clock'); if(clock) clock.innerText = new Date().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'}); }, 1000);