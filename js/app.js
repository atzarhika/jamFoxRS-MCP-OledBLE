let isConnected = false;
let bluetoothDevice;
let rxChar, txChar;
let rxBuffer = "";

const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

// FUNGSI LOAD PAGE DINAMIS
async function loadPage(pageName, element) {
    try {
        const response = await fetch(`${pageName}.html`);
        const html = await response.text();
        document.getElementById('page-container').innerHTML = html;
        
        lucide.createIcons();
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        if(element) element.classList.add('active');

        if(pageName === 'battery') generateCells();
        
    } catch (error) { console.error('Gagal memuat halaman:', error); }
}

window.addEventListener('DOMContentLoaded', () => loadPage('dash'));

function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.innerHTML !== "") return;
    for (let i = 1; i <= 23; i++) {
        grid.innerHTML += `<div class="cell-card"><span class="cell-id">C${i}</span><b id="c${i}-v">0.000V</b></div>`;
    }
}

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
            txChar = await service.getCharacteristic(TX_CHAR_UUID);
            await txChar.startNotifications();
            txChar.addEventListener('characteristicvaluechanged', (e) => {
                let chunk = new TextDecoder().decode(e.target.value);
                rxBuffer += chunk;
                if (rxBuffer.includes('\n')) {
                    let lines = rxBuffer.split('\n');
                    rxBuffer = lines.pop();
                    lines.forEach(line => { 
                        if (line.trim()) { 
                            try { updateUI(JSON.parse(line)); } catch (e) {} 
                            const lb = document.getElementById('log-box');
                            if (lb) lb.innerText = line + "\n" + lb.innerText.substring(0, 500);
                        } 
                    });
                }
            });
            rxChar = await service.getCharacteristic(RX_CHAR_UUID);
            bluetoothDevice.addEventListener('gattserverdisconnected', onDisconnected);
            setStatus(true);
        } catch (e) { console.log(e); }
    }
}

function updateUI(data) {
    if (!isConnected) return;
    
    // 1. GAUGE & DASH CORE
    const speedEl = document.getElementById('speed');
    if (speedEl) speedEl.innerText = data.speed || 0;

    const rpm = data.rpm || 0;
    const rpmValEl = document.getElementById('rpm-val');
    if (rpmValEl) rpmValEl.innerText = rpm;

    // UPDATE MODE BERKENDARA & WARNA BADGE (Sinkron V15.8)
    const modeEl = document.getElementById('mode-text');
    if (modeEl && data.mode) {
        modeEl.innerText = data.mode;
        const mode = data.mode.toUpperCase();
        if (mode === "SPORT") modeEl.style.backgroundColor = "#d29922";
        else if (mode === "DRIVE") modeEl.style.backgroundColor = "#238636";
        else if (mode === "REVERSE") modeEl.style.backgroundColor = "#bc8cff";
        else if (mode === "PARK") modeEl.style.backgroundColor = "#30363d";
        else modeEl.style.backgroundColor = "#f85149";
    }

    const circle = document.getElementById('gauge-circle');
    const ball = document.getElementById('glow-ball');
    if (circle) {
        const degrees = Math.min((rpm / 1600) * 360, 360);
        circle.style.setProperty('--deg', `${degrees}deg`);
        circle.style.background = `radial-gradient(var(--card-bg) 64%, transparent 66%), conic-gradient(from 180deg, var(--cyan) ${degrees}deg, #21262d 0deg)`;
        if (ball) {
            ball.style.display = "block";
            const angleRad = (degrees + 180 - 90) * (Math.PI / 180);
            const x = Math.cos(angleRad) * 94;
            const y = Math.sin(angleRad) * 94;
            ball.style.transform = `translate(calc(-50% + ${x}px), calc(-50% + ${y}px))`;
        }
    }

    // 2. DATA TRIP, BATTERY, & TEMPS (GLOBAL IDs)
    const ids = {
        'range-val': (data.trip?.range || 0) + " km",
        'trip-val': (data.trip?.km || 0).toFixed(1) + " km",
        'soc-dash': (data.soc || 0) + "%",
        'soc-batt': (data.soc || 0) + "%",
        'soh-val': (data.health?.soh || 0) + "%",
        'v-val': (data.volts || 0).toFixed(1) + "V",
        'a-val': (data.amps || 0).toFixed(1) + "A",
        'w-val': Math.abs((data.volts || 0) * (data.amps || 0)).toFixed(0) + "W",
        'trip-dist-large': (data.trip?.km || 0).toFixed(1) + " km",
        'avg-wh': (data.trip?.avg || 0).toFixed(1),
        'est-range-trip': (data.trip?.range || 0) + " km",
        't-ecu': (data.temps?.ctrl || 0) + "°",
        't-motor': (data.temps?.motor || 0) + "°",
        't-batt': (data.temps?.batt || 0) + "°"
    };

    for (const [id, val] of Object.entries(ids)) {
        const el = document.getElementById(id);
        if (el) el.innerText = val;
    }
    
    const bar = document.getElementById('soc-bar');
    if (bar) bar.style.width = (data.soc || 0) + "%";

    // 3. CELLS UPDATE
    if (data.cells) {
        data.cells.forEach((mv, i) => {
            const el = document.getElementById(`c${i+1}-v`);
            if (el) el.innerText = (mv / 1000).toFixed(3) + "V";
        });
    }
}

// SETTINGS ACTIONS
async function sendSplash() {
    if (!rxChar) return;
    const val = document.getElementById('splashInput').value;
    if (val) {
        await rxChar.writeValue(new TextEncoder().encode(`SPLASH,${val}\n`));
        alert("Splash Screen Updated!");
    }
}

async function syncTime() {
    if (!rxChar) return;
    const n = new Date();
    const cmd = `TIME,${n.getFullYear()},${n.getMonth()+1},${n.getDate()},${n.getHours()},${n.getMinutes()},${n.getSeconds()}\n`;
    await rxChar.writeValue(new TextEncoder().encode(cmd));
    alert("Time Synced!");
}

function setStatus(status) {
    isConnected = status;
    const btn = document.getElementById('connectBtn');
    btn.innerText = status ? "DISCONNECT" : "CONNECT";
    btn.style.background = status ? "#f85149" : "#58a6ff";
    const st = document.getElementById('conn-status');
    st.innerText = status ? "● ONLINE" : "● OFFLINE";
    st.style.color = status ? "#3fb950" : "#f85149";
}

function onDisconnected() { setStatus(false); }

setInterval(() => {
    const clock = document.getElementById('clock');
    if (clock) clock.innerText = new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
}, 1000);