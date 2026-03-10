// Konfigurasi Bluetooth (WAJIB ADA)
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

let isConnected = false;
let bluetoothDevice, rxChar, txChar;
let rxBuffer = "";

// KOMENTAR: Variabel memori grafik global agar data tidak hilang saat pindah menu
let speedHistory = Array(30).fill(0);
let currentHistory = Array(30).fill(0);
let speedChart = null;
let currentChart = null;

async function loadPage(pageName, element) {
    try {
        const response = await fetch(`${pageName}.html`);
        const html = await response.text();
        document.getElementById('page-container').innerHTML = html;
        lucide.createIcons();
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        if(element) element.classList.add('active');
        
        // KOMENTAR: Reset referensi grafik lama agar bisa membuat grafik baru di elemen canvas yang baru
        speedChart = null;
        currentChart = null;

        if(pageName === 'trip') initCharts();
        if(pageName === 'battery') generateCells();
    } catch (error) { console.error('Error:', error); }
}

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

    // KOMENTAR: Inisialisasi grafik menggunakan data yang tersimpan di memory global
    speedChart = new Chart(ctxS, { type: 'line', data: { labels: Array(30).fill(''), datasets: [{ data: [...speedHistory], borderColor: '#d29922', fill: true, backgroundColor: 'rgba(210, 153, 34, 0.1)' }] }, options: opt });
    currentChart = new Chart(ctxC, { type: 'line', data: { labels: Array(30).fill(''), datasets: [{ data: [...currentHistory], borderColor: '#58a6ff', fill: true, backgroundColor: 'rgba(88, 166, 255, 0.1)' }] }, options: opt });
}

function updateUI(data) {
    if (!isConnected) return;

    // 1. UPDATE DATA MEMORY (Selalu berjalan di latar belakang)
    speedHistory.push(data.speed || 0);
    currentHistory.push(Math.abs(data.amps || 0));
    speedHistory.shift();
    currentHistory.shift();

    // 2. UPDATE VISUAL GRAFIK (Hanya jika halaman Trip sedang dibuka)
    if (speedChart && currentChart) {
        speedChart.data.datasets[0].data = [...speedHistory];
        currentChart.data.datasets[0].data = [...currentHistory];
        speedChart.update('none');
        currentChart.update('none');
    }

    // 3. UPDATE LOGS STREAM (Selalu update jika halaman logs terbuka)
    const logBox = document.getElementById('log-box');
    if (logBox) {
        const timestamp = new Date().toLocaleTimeString();
        logBox.innerText = `[${timestamp}] ${JSON.stringify(data)}\n` + logBox.innerText.substring(0, 2000);
    }

    // 4. FIX: UPDATE MODE BERKENDARA
    const modeEl = document.getElementById('mode-text');
    if (modeEl && data.mode) {
        modeEl.innerText = data.mode;
        const m = data.mode.toUpperCase();
        // Logika warna badge mode (Sinkron V15.8)
        if (m === "SPORT") modeEl.style.backgroundColor = "#d29922";
        else if (m === "DRIVE") modeEl.style.backgroundColor = "#238636";
        else if (m === "REVERSE") modeEl.style.backgroundColor = "#bc8cff";
        else if (m === "PARK" || m === "PARKING") modeEl.style.backgroundColor = "#30363d";
        else modeEl.style.backgroundColor = "#f85149";
    }

    // 5. UPDATE GAUGE & DASHBOARD
    const rpm = data.rpm || 0;
    const circle = document.getElementById('gauge-circle');
    if (circle) {
        const deg = Math.min((rpm / 1600) * 360, 360);
        circle.style.setProperty('--deg', `${deg}deg`);
        circle.style.background = `radial-gradient(var(--card-bg) 64%, transparent 66%), conic-gradient(from 180deg, var(--cyan) ${deg}deg, #21262d 0deg)`;
        const ball = document.getElementById('glow-ball');
        if (ball) {
            ball.style.display = "block";
            const rad = (deg + 180 - 90) * (Math.PI / 180);
            ball.style.transform = `translate(calc(-50% + ${Math.cos(rad)*100}px), calc(-50% + ${Math.sin(rad)*100}px))`;
        }
    }

    // 6. UPDATE DATA GLOBAL (Sesuai ID di HTML)
    const vals = {
        'speed': data.speed || 0, 'rpm-val': rpm, 'soc-dash': (data.soc || 0) + "%",
        'soc-batt': (data.soc || 0) + "%", 'soh-val': (data.health?.soh || 0) + "%",
        'v-val': (data.volts || 0).toFixed(1) + "V", 'a-val': (data.amps || 0).toFixed(1) + "A",
        'w-val': Math.abs((data.volts||0)*(data.amps||0)).toFixed(0) + "W",
        'trip-dist-large': (data.trip?.km || 0).toFixed(1) + " km", 'avg-wh': (data.trip?.avg || 0).toFixed(1),
        'est-range-trip': (data.trip?.range || 0) + " km", 'range-val': (data.trip?.range || 0) + " km",
        'trip-val': (data.trip?.km || 0).toFixed(1) + " km",
        't-ecu': (data.temps?.ctrl || 0) + "°", 't-motor': (data.temps?.motor || 0) + "°", 't-batt': (data.temps?.batt || 0) + "°"
    };

    for (const [id, val] of Object.entries(vals)) {
        const el = document.getElementById(id);
        if (el) el.innerText = val;
    }
}

// ... (Sisa fungsi Bluetooth, setStatus, dan generateCells tetap sama) ...

async function toggleConnect() {
    if (isConnected) { 
        if (bluetoothDevice) await bluetoothDevice.gatt.disconnect(); 
    } else {
        try {
            bluetoothDevice = await navigator.bluetooth.requestDevice({ 
                filters: [{ name: 'Votol_BLE' }], 
                optionalServices: [SERVICE_UUID] 
            });
            const server = await bluetoothDevice.gatt.connect();
            const service = await server.getPrimaryService(SERVICE_UUID);

            // SETUP NOTIFIKASI (Terima data dari ESP32)
            txChar = await service.getCharacteristic(TX_CHAR_UUID);
            await txChar.startNotifications();
            txChar.addEventListener('characteristicvaluechanged', (e) => {
                let chunk = new TextDecoder().decode(e.target.value); rxBuffer += chunk;
                if (rxBuffer.includes('\n')) {
                    let lines = rxBuffer.split('\n'); rxBuffer = lines.pop();
                    lines.forEach(l => { if(l.trim()) try { updateUI(JSON.parse(l)); } catch(e){} });
                }
            });

            // SETUP WRITE CHARACTERISTIC (Kirim data ke ESP32)
            // Ini adalah bagian yang sebelumnya hilang atau tidak terhubung ke tombol
            rxChar = await service.getCharacteristic(RX_CHAR_UUID);

            bluetoothDevice.addEventListener('gattserverdisconnected', () => setStatus(false));
            setStatus(true);
        } catch (e) { alert("Koneksi Gagal: " + e); }
    }
}

// FUNGSI UPDATE SPLASHSCREEN (Sinkron dengan firmware startsWith("SPLASH,"))
async function sendSplash() {
    if (!rxChar) { alert("Hubungkan Bluetooth terlebih dahulu!"); return; }
    const val = document.getElementById('splashInput').value.trim();
    if (val) {
        // Firmware V15.8 membaca perintah "SPLASH,isi_teks"
        const encoder = new TextEncoder();
        await rxChar.writeValue(encoder.encode(`SPLASH,${val}`));
        alert("Perintah Update Splash dikirim: " + val);
    } else {
        alert("Nama splash tidak boleh kosong!");
    }
}

// FUNGSI SYNC TIME (Sinkron dengan firmware sscanf "TIME,%d,%d,%d,%d,%d,%d")
async function syncTime() {
    if (!rxChar) { alert("Hubungkan Bluetooth terlebih dahulu!"); return; }
    const n = new Date();
    // Format yang diminta firmware: TIME,YYYY,MM,DD,HH,MM,SS
    const timeCmd = `TIME,${n.getFullYear()},${n.getMonth()+1},${n.getDate()},${n.getHours()},${n.getMinutes()},${n.getSeconds()}`;
    
    const encoder = new TextEncoder();
    try {
        await rxChar.writeValue(encoder.encode(timeCmd));
        alert("Waktu Berhasil Disinkronkan ke RTC Motor!");
    } catch (e) {
        alert("Gagal Sinkronisasi: " + e);
    }
}
function setStatus(s) { isConnected = s; const b = document.getElementById('connectBtn'); if(b) { b.innerText = s ? "DISCONNECT" : "CONNECT"; b.style.background = s ? "#f85149" : "#58a6ff"; } document.getElementById('conn-status').innerText = s ? "● ONLINE" : "● OFFLINE"; }

function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.innerHTML !== "") return;
    for (let i = 1; i <= 23; i++) grid.innerHTML += `<div class="cell-card"><small class="cell-id">C${i}</small><b id="c${i}-v">0.000V</b></div>`;
}

window.addEventListener('DOMContentLoaded', () => loadPage('dash'));
setInterval(() => { document.getElementById('clock').innerText = new Date().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'}); }, 1000);