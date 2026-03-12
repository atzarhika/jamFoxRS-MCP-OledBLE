/**
 * Votol Dash Pro v1.1 - ULTIMATE INTEGRATION
 * Fitur: Dashboard, GPX (Insta360), Wake Lock, Glow-Ball, BMS Detail (Bars/Delta), & Trip Analytics
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

// --- GPS INITIALIZATION ---
if ("geolocation" in navigator) {
    navigator.geolocation.watchPosition(
        (pos) => {
            currentGPS = { lat: pos.coords.latitude, lon: pos.coords.longitude, alt: pos.coords.altitude || 0 };
            const gpsStatus = document.getElementById('gps-status');
            if (gpsStatus) gpsStatus.innerText = `GPS: Fixed (${currentGPS.lat.toFixed(4)}, ${currentGPS.lon.toFixed(4)})`;
        },
        (err) => console.error("GPS Error:", err),
        { enableHighAccuracy: true }
    );
}

// --- PREVENT SCREEN OFF ---
async function requestWakeLock() {
    try {
        if ('wakeLock' in navigator) {
            wakeLock = await navigator.wakeLock.request('screen');
        }
    } catch (err) { console.error(err); }
}

// --- NAVIGATION ---
async function loadPage(pageName, element) {
    try {
        const response = await fetch(`${pageName}.html`);
        const html = await response.text();
        document.getElementById('page-container').innerHTML = html;
        lucide.createIcons();
        speedChart = null;
        currentChart = null;
        if (pageName === 'battery') generateCells();
        if (pageName === 'trip') initCharts();
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        if (element) element.classList.add('active');
    } catch (error) { console.error(error); }
}

// --- DATA PROCESSING & UI UPDATE ---
function updateUI(data) {
    if (!isConnected || !data) return;

    // 1. Logs Traffic (Traffic JSON)
    const logBox = document.getElementById('log-box');
    if (logBox) {
        logBox.innerText = `[${new Date().toLocaleTimeString()}] ${JSON.stringify(data)}\n` + logBox.innerText.substring(0, 1000);
    }

    // 2. GPX Data Collection
    if (isRecording) {
        gpxDataPoints.push({
            lat: currentGPS.lat, lon: currentGPS.lon, ele: currentGPS.alt,
            time: new Date().toISOString(),
            speed: data.speed || 0, rpm: data.rpm || 0,
            temp: data.temps?.motor || 0, soc: data.soc || 0,
            current: Math.abs(data.amps || 0)
        });
    }

    // 3. History Charts Update
    speedHistory.push(data.speed || 0);
    currentHistory.push(Math.abs(data.amps || 0));
    speedHistory.shift();
    currentHistory.shift();
    if (speedChart && currentChart) {
        speedChart.data.datasets[0].data = [...speedHistory];
        currentChart.data.datasets[0].data = [...currentHistory];
        speedChart.update('none');
        currentChart.update('none');
    }

    // 4. Mode Badge & Gauge Animation
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

    const rpm = data.rpm || 0;
    const circle = document.getElementById('gauge-circle');
    const ball = document.getElementById('glow-ball');
    if (circle) {
        const deg = Math.min((rpm / 1600) * 360, 360);
        circle.style.setProperty('--deg', `${deg}deg`);
        circle.style.background = `radial-gradient(var(--card-bg) 64%, transparent 66%), conic-gradient(from 180deg, var(--cyan) ${deg}deg, #21262d 0deg)`;
        if (ball) {
            const rad = (deg + 180 - 90) * (Math.PI / 180);
            ball.style.transform = `translate(calc(-50% + ${Math.cos(rad)*100}px), calc(-50% + ${Math.sin(rad)*100}px))`;
        }
    }

    // 5. Global Data Update (All IDs)
    const h = data.health || {};
    const t = data.trip || {};
    const vals = {
        'speed': data.speed || 0, 'rpm-val': rpm,
        'soc-dash': (data.soc || 0) + "%", 'soc-batt': (data.soc || 0) + "%",
        'soh-val': (h.soh || 0) + "%", 'v-val': (data.volts || 0).toFixed(1) + "V",
        'a-val': (data.amps || 0).toFixed(1) + "A", 'w-val': Math.abs((data.volts||0)*(data.amps||0)).toFixed(0) + "W",
        'range-val': (t.range || 0) + " km", 'trip-val': (t.km || 0).toFixed(1) + " km",
        't-ecu': (data.temps?.ctrl || 0) + "°", 't-motor': (data.temps?.motor || 0) + "°", 't-batt': (data.temps?.batt || 0) + "°",
        
        // BMS Detail Fix (remainCap & fullCap)
        'cycle-val': h.cycles || 0,
        'cap-rem': (h.remainCap || 0).toFixed(1) + " Ah",
        'cap-full': (h.fullCap || 0).toFixed(1) + " Ah",
        'bms-status': data.amps > 0.5 ? "CHARGING" : (data.amps < -0.5 ? "DISCHARGE" : "STANDBY"),

        // Trip Analytics Fix
        'trip-dist-large': (t.km || 0).toFixed(1) + " km",
        'avg-wh': (t.avg || 0).toFixed(1),
        'est-range-trip': (t.range || 0) + " km"
    };

    for (const [id, val] of Object.entries(vals)) {
        const el = document.getElementById(id);
        if (el) el.innerText = val;
    }

    // 6. BMS Cell Bars & Delta Calculation
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

    const bar = document.getElementById('soc-bar');
    if (bar) bar.style.width = (data.soc || 0) + "%";
}

// --- GPX & RECORD LOGIC ---
async function toggleRecord() {
    const btn = document.getElementById('recordBtn');
    const icon = document.getElementById('record-icon');
    const timerEl = document.getElementById('record-timer');
    if (!isRecording) {
        await requestWakeLock();
        isRecording = true; gpxDataPoints = []; recordStartTime = Date.now();
        btn.style.background = "#f85149"; icon.setAttribute("data-lucide", "square"); lucide.createIcons();
        recordInterval = setInterval(() => {
            const elapsed = Date.now() - recordStartTime;
            const h = Math.floor(elapsed / 3600000).toString().padStart(2, '0');
            const m = Math.floor((elapsed % 3600000) / 60000).toString().padStart(2, '0');
            const s = Math.floor((elapsed % 60000) / 1000).toString().padStart(2, '0');
            if (timerEl) timerEl.innerText = `${h}:${m}:${s}`;
        }, 1000);
    } else {
        isRecording = false; clearInterval(recordInterval);
        if (wakeLock) { wakeLock.release().then(() => wakeLock = null); }
        btn.style.background = "var(--orange)"; icon.setAttribute("data-lucide", "circle"); lucide.createIcons();
        saveGPX();
    }
}

function saveGPX() {
    if (gpxDataPoints.length === 0) return;
    const now = new Date();
    const fileName = `${now.getFullYear()}${(now.getMonth()+1).toString().padStart(2,'0')}${now.getDate().toString().padStart(2,'0')}_${now.getHours().toString().padStart(2,'0')}${now.getMinutes().toString().padStart(2,'0')}.gpx`;
    let gpx = `<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="Votol Dash Pro" xmlns="http://www.topografix.com/GPX/1/1" xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v2">
<trk><name>Votol Session</name><trkseg>`;
    gpxDataPoints.forEach(p => {
        const speedMS = (p.speed / 3.6).toFixed(3);
        gpx += `<trkpt lat="${p.lat}" lon="${p.lon}"><ele>${p.ele.toFixed(2)}</ele><time>${p.time}</time><extensions><gpxtpx:TrackPointExtension><gpxtpx:speed>${speedMS}</gpxtpx:speed><gpxtpx:hr>${p.rpm}</gpxtpx:hr><gpxtpx:cad>${p.soc}</gpxtpx:cad></gpxtpx:TrackPointExtension></extensions></trkpt>`;
    });
    gpx += `\n</trkseg></trk></gpx>`;
    const blob = new Blob([gpx], { type: 'application/gpx+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = fileName; a.click();
}

// --- BLUETOOTH CONNECT ---
async function toggleConnect() {
    if (isConnected) { if (bluetoothDevice) await bluetoothDevice.gatt.disconnect(); } 
    else {
        try {
            bluetoothDevice = await navigator.bluetooth.requestDevice({ filters: [{ name: 'Votol_BLE' }], optionalServices: [SERVICE_UUID] });
            const server = await bluetoothDevice.gatt.connect();
            const service = await server.getPrimaryService(SERVICE_UUID);
            txChar = await service.getCharacteristic(TX_CHAR_UUID);
            await txChar.startNotifications();
            txChar.addEventListener('characteristicvaluechanged', (e) => {
                let chunk = new TextDecoder().decode(e.target.value); rxBuffer += chunk;
                if (rxBuffer.includes('\n')) {
                    let lines = rxBuffer.split('\n'); rxBuffer = lines.pop();
                    lines.forEach(l => { if(l.trim()) try { updateUI(JSON.parse(l)); } catch(e){} });
                }
            });
            rxChar = await service.getCharacteristic(RX_CHAR_UUID);
            bluetoothDevice.addEventListener('gattserverdisconnected', () => setStatus(false));
            setStatus(true);
        } catch (e) { alert("Koneksi Gagal: " + e); }
    }
}

function setStatus(s) { 
    isConnected = s; 
    const b = document.getElementById('connectBtn'); if(b) { b.innerText = s ? "DISCONNECT" : "CONNECT"; b.style.background = s ? "#f85149" : "#58a6ff"; } 
    const st = document.getElementById('conn-status'); if(st) { st.innerText = s ? "● ONLINE" : "● OFFLINE"; st.style.color = s ? "#3fb950" : "#f85149"; }
}

// --- CELL GENERATION (Fix Bar HTML) ---
function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.children.length > 0) return;
    for (let i = 1; i <= 23; i++) {
        grid.innerHTML += `
            <div class="cell-card">
                <div class="cell-info"><small class="cell-id">C${i}</small><b id="c${i}-v">0.000V</b></div>
                <div class="cell-bar-bg"><div id="c${i}-bar" class="cell-bar-fill"></div></div>
            </div>`;
    }
}

function initCharts() {
    const ctxS = document.getElementById('speedChart')?.getContext('2d');
    const ctxC = document.getElementById('currentChart')?.getContext('2d');
    if (!ctxS || !ctxC) return;
    const opt = { responsive:true, maintainAspectRatio:false, scales:{x:{display:false}, y:{display:false, beginAtZero:true}}, plugins:{legend:{display:false}}, elements:{line:{tension:0.4, borderWidth:2}, point:{radius:0}} };
    speedChart = new Chart(ctxS, { type:'line', data:{labels:Array(30).fill(''), datasets:[{data:[...speedHistory], borderColor:'#d29922', fill:true, backgroundColor:'rgba(210,153,34,0.1)'}]}, options:opt });
    currentChart = new Chart(ctxC, { type:'line', data:{labels:Array(30).fill(''), datasets:[{data:[...currentHistory], borderColor:'#58a6ff', fill:true, backgroundColor:'rgba(88,166,255,0.1)'}]}, options:opt });
}

// --- FUNGSI SETTINGS DENGAN FEEDBACK ---
async function sendSplash() {
    if (!rxChar) {
        alert("Gagal: Bluetooth belum terhubung!");
        return;
    }
    const val = document.getElementById('splashInput').value.trim();
    if (val) {
        try {
            await rxChar.writeValue(new TextEncoder().encode(`SPLASH,${val}`));
            alert(`Berhasil! Nama Splash diubah menjadi: ${val}`); 
        } catch (error) {
            alert("Gagal mengirim data: " + error);
        }
    } else {
        alert("Masukkan nama terlebih dahulu!");
    }
}

async function syncTime() {
    if (!rxChar) {
        alert("Gagal: Bluetooth belum terhubung!");
        return;
    }
    try {
        const n = new Date();
        // Format: TIME,YYYY,MM,DD,HH,MM,SS
        const cmd = `TIME,${n.getFullYear()},${n.getMonth()+1},${n.getDate()},${n.getHours()},${n.getMinutes()},${n.getSeconds()}`;
        await rxChar.writeValue(new TextEncoder().encode(cmd));
        alert("Waktu HP berhasil disinkronkan ke Motor!"); 
    } catch (error) {
        alert("Gagal sinkronisasi waktu: " + error);
    }
}

// Tambahkan pengecekan ini di loadPage agar Cell Bar selalu muncul saat pindah tab
async function loadPage(pageName, element) {
    try {
        const response = await fetch(`${pageName}.html`);
        const html = await response.text();
        document.getElementById('page-container').innerHTML = html;
        
        lucide.createIcons();
        
        // Memastikan inisialisasi ulang saat navigasi
        if (pageName === 'battery') generateCells(); 
        if (pageName === 'trip') initCharts();
        
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        if (element) element.classList.add('active');
    } catch (error) { console.error('Gagal memuat halaman:', error); }
}

window.addEventListener('DOMContentLoaded', () => loadPage('dash'));

setInterval(() => { 
    const clock = document.getElementById('clock'); 
    if(clock) clock.innerText = new Date().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'}); 
}, 1000);