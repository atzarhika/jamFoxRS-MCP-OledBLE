// Konfigurasi Bluetooth (Sesuai Firmware V15.8)
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

let isConnected = false;
let bluetoothDevice, rxChar, txChar;
let rxBuffer = "";

// Memori Grafik Global
let speedHistory = Array(30).fill(0);
let currentHistory = Array(30).fill(0);
let speedChart = null;
let currentChart = null;

// --- VARIABEL TELEMETRI ---
let isRecording = false;
let recordStartTime = 0;
let recordInterval;
let gpxDataPoints = []; 
let currentGPS = { lat: 0, lon: 0, alt: 0 };
let wakeLock = null; // BARU: Untuk menyimpan status layar terjaga

// Inisialisasi GPS HP
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

// BARU: Fungsi untuk meminta browser agar layar tetap menyala
async function requestWakeLock() {
    try {
        if ('wakeLock' in navigator) {
            wakeLock = await navigator.wakeLock.request('screen');
            console.log('Wake Lock aktif: Layar akan tetap menyala');
        }
    } catch (err) {
        console.error(`${err.name}, ${err.message}`);
    }
}

// 1. FUNGSI MUAT HALAMAN (SPA)
async function loadPage(pageName, element) {
    try {
        const response = await fetch(`${pageName}.html`);
        const html = await response.text();
        document.getElementById('page-container').innerHTML = html;
        
        lucide.createIcons();
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        if(element) element.classList.add('active');
        
        speedChart = null;
        currentChart = null;

        if(pageName === 'trip') initCharts();
        if(pageName === 'battery') generateCells();
    } catch (error) { console.error('Gagal memuat halaman:', error); }
}

// 2. INISIALISASI GRAFIK DI HALAMAN TRIP
function initCharts() {
    const ctxS = document.getElementById('speedChart')?.getContext('2d');
    const ctxC = document.getElementById('currentChart')?.getContext('2d');
    if (!ctxS || !ctxC) return;

    const opt = { 
        responsive: true, maintainAspectRatio: false, 
        scales: { x: {display:false}, y: {display:false, beginAtZero:true} },
        plugins: { legend: {display:false} },
        elements: { line: {tension:0.4, borderWidth:2}, point: {radius:0} }
    };

    speedChart = new Chart(ctxS, { 
        type: 'line', 
        data: { labels: Array(30).fill(''), datasets: [{ data: [...speedHistory], borderColor: '#d29922', fill: true, backgroundColor: 'rgba(210, 153, 34, 0.1)' }] }, 
        options: opt 
    });

    currentChart = new Chart(ctxC, { 
        type: 'line', 
        data: { labels: Array(30).fill(''), datasets: [{ data: [...currentHistory], borderColor: '#58a6ff', fill: true, backgroundColor: 'rgba(88, 166, 255, 0.1)' }] }, 
        options: opt 
    });
}

// FUNGSI TOGGLE RECORD
async function toggleRecord() { // BARU: Ditambah 'async' karena ada requestWakeLock
    const btn = document.getElementById('recordBtn');
    const icon = document.getElementById('record-icon');
    const timerEl = document.getElementById('record-timer');

    if (!isRecording) {
        // START RECORDING
        await requestWakeLock(); // BARU: Minta layar menyala saat rekam dimulai
        
        isRecording = true;
        gpxDataPoints = [];
        recordStartTime = Date.now();
        btn.style.background = "#f85149";
        icon.setAttribute("data-lucide", "square");
        lucide.createIcons();

        recordInterval = setInterval(() => {
            const elapsed = Date.now() - recordStartTime;
            const h = Math.floor(elapsed / 3600000).toString().padStart(2, '0');
            const m = Math.floor((elapsed % 3600000) / 60000).toString().padStart(2, '0');
            const s = Math.floor((elapsed % 60000) / 1000).toString().padStart(2, '0');
            if (timerEl) timerEl.innerText = `${h}:${m}:${s}`;
        }, 1000);
    } else {
        // STOP RECORDING
        isRecording = false;
        clearInterval(recordInterval);
        
        // BARU: Lepaskan Wake Lock agar layar bisa mati normal kembali
        if (wakeLock !== null) {
            wakeLock.release().then(() => wakeLock = null);
        }

        btn.style.background = "var(--orange)";
        icon.setAttribute("data-lucide", "circle");
        lucide.createIcons();
        if (timerEl) timerEl.innerText = "00:00:00";
        
        saveGPX();
    }
}

// LOGIKA PENGUMPULAN DATA
function collectTelemetry(votolData) {
    if (!isRecording) return;
    const point = {
        lat: currentGPS.lat,
        lon: currentGPS.lon,
        ele: currentGPS.alt,
        time: new Date().toISOString(),
        speed: votolData.speed || 0,
        rpm: votolData.rpm || 0,
        temp: votolData.temps?.motor || 0,
        soc: votolData.soc || 0,
        current: Math.abs(votolData.amps || 0)
    };
    gpxDataPoints.push(point);
}

// 3. UPDATE UI DARI DATA BLUETOOTH
function updateUI(data) {
    if (!isConnected) return;
    collectTelemetry(data);

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
    const speedEl = document.getElementById('speed');
    if (speedEl) speedEl.innerText = data.speed || 0;
    
    const rpmValEl = document.getElementById('rpm-val');
    if (rpmValEl) rpmValEl.innerText = rpm;

    const circle = document.getElementById('gauge-circle');
    if (circle) {
        const deg = Math.min((rpm / 1600) * 360, 360);
        circle.style.setProperty('--deg', `${deg}deg`);
        circle.style.background = `radial-gradient(var(--card-bg) 64%, transparent 66%), conic-gradient(from 180deg, var(--cyan) ${deg}deg, #21262d 0deg)`;
        
        const ball = document.getElementById('glow-ball');
        if (ball) {
            ball.style.display = "block";
            const rad = (deg + 180 - 90) * (Math.PI / 180);
            const x = Math.cos(rad) * 100;
            const y = Math.sin(rad) * 100;
            ball.style.transform = `translate(calc(-50% + ${x}px), calc(-50% + ${y}px))`;
        }
    }

    const tripData = data.trip || {};
    const vals = {
        'range-val': (tripData.range || 0) + " km",
        'trip-val': (tripData.km || 0).toFixed(1) + " km",
        'soc-dash': (data.soc || 0) + "%",
        'soc-batt': (data.soc || 0) + "%",
        'soh-val': (data.health?.soh || 0) + "%",
        'v-val': (data.volts || 0).toFixed(1) + "V",
        'a-val': (data.amps || 0).toFixed(1) + "A",
        'w-val': Math.abs((data.volts || 0) * (data.amps || 0)).toFixed(0) + "W",
        't-ecu': (data.temps?.ctrl || 0) + "°",
        't-motor': (data.temps?.motor || 0) + "°",
        't-batt': (data.temps?.batt || 0) + "°",
        'trip-dist-large': (tripData.km || 0).toFixed(1) + " km",
        'avg-wh': (tripData.avg || 0).toFixed(1),
        'est-range-trip': (tripData.range || 0) + " km"
    };

    for (const [id, val] of Object.entries(vals)) {
        const el = document.getElementById(id);
        if (el) el.innerText = val;
    }

    const bar = document.getElementById('soc-bar');
    if (bar) bar.style.width = (data.soc || 0) + "%";

    const logBox = document.getElementById('log-box');
    if (logBox) {
        logBox.innerText = JSON.stringify(data) + "\n" + logBox.innerText.substring(0, 1000);
    }

    if (data.cells) {
        data.cells.forEach((mv, i) => {
            const el = document.getElementById(`c${i+1}-v`);
            if (el) el.innerText = (mv / 1000).toFixed(3) + "V";
        });
    }
}

// 4. KONEKSI BLUETOOTH
async function toggleConnect() {
    if (isConnected) { 
        if (bluetoothDevice) await bluetoothDevice.gatt.disconnect(); 
    } else {
        try {
            bluetoothDevice = await navigator.bluetooth.requestDevice({ 
                filters: [{ name: 'Votol_BLE' }], optionalServices: [SERVICE_UUID] 
            });
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

// 5. SETTINGS ACTIONS
async function sendSplash() {
    if (!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    const val = document.getElementById('splashInput').value.trim();
    if (val) {
        await rxChar.writeValue(new TextEncoder().encode(`SPLASH,${val}`));
        alert("Splash Updated!");
    }
}

async function syncTime() {
    if (!rxChar) { alert("Hubungkan Bluetooth!"); return; }
    const n = new Date();
    const cmd = `TIME,${n.getFullYear()},${n.getMonth()+1},${n.getDate()},${n.getHours()},${n.getMinutes()},${n.getSeconds()}`;
    await rxChar.writeValue(new TextEncoder().encode(cmd));
    alert("Time Synced!");
}

// 6. UTILITY FUNCTIONS
function setStatus(s) { 
    isConnected = s; 
    const b = document.getElementById('connectBtn'); 
    if(b) { b.innerText = s ? "DISCONNECT" : "CONNECT"; b.style.background = s ? "#f85149" : "#58a6ff"; } 
    const st = document.getElementById('conn-status');
    if(st) { st.innerText = s ? "● ONLINE" : "● OFFLINE"; st.style.color = s ? "#3fb950" : "#f85149"; }
}

function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.innerHTML !== "") return;
    for (let i = 1; i <= 23; i++) {
        grid.innerHTML += `<div class="cell-card"><small class="cell-id">C${i}</small><b id="c${i}-v">0.000V</b></div>`;
    }
}

// BARU: Fungsi Save GPX yang sudah dimodifikasi untuk Insta360 (Format Garmin)
function saveGPX() {
    if (gpxDataPoints.length === 0) { alert("Data kosong!"); return; }

    const now = new Date();
    const fileName = `${now.getFullYear()}${(now.getMonth()+1).toString().padStart(2,'0')}${now.getDate().toString().padStart(2,'0')}_${now.getHours().toString().padStart(2,'0')}${now.getMinutes().toString().padStart(2,'0')}.gpx`;

    // HEADER: Menggunakan skema Garmin lengkap untuk mendukung kalkulasi G-Force
    let gpxContent = `<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="Votol Dash Pro" 
  xmlns="http://www.topografix.com/GPX/1/1"
  xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v2">
<trk><name>Votol Telemetry Session</name><trkseg>`;

    gpxDataPoints.forEach(p => {
        // Konversi Speed ke m/s dengan presisi tinggi untuk mengurangi delay
        const speedMS = (p.speed / 3.6).toFixed(3);

        gpxContent += `
<trkpt lat="${p.lat}" lon="${p.lon}">
    <ele>${p.ele.toFixed(2)}</ele> <time>${p.time}</time>
    <extensions>
        <gpxtpx:TrackPointExtension>
            <gpxtpx:speed>${speedMS}</gpxtpx:speed> <gpxtpx:hr>${p.rpm}</gpxtpx:hr>       <gpxtpx:cad>${p.soc}</gpxtpx:cad>      </gpxtpx:TrackPointExtension>
    </extensions>
</trkpt>`;
    });

    gpxContent += `\n</trkseg></trk></gpx>`;

    const blob = new Blob([gpxContent], { type: 'application/gpx+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = fileName; a.click();
    URL.revokeObjectURL(url);
}

window.addEventListener('DOMContentLoaded', () => loadPage('dash'));
setInterval(() => { 
    const clock = document.getElementById('clock');
    if(clock) clock.innerText = new Date().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'}); 
}, 1000);