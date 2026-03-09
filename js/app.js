let isConnected = false;
let bluetoothDevice;
let rxChar, txChar;
let rxBuffer = "";

const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

function nav(pageId) {
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
    document.getElementById(pageId).classList.add('active');
    if (window.event) window.event.currentTarget.classList.add('active');
    if (pageId === 'battery') generateCells();
}

function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.innerHTML !== "") return;
    for (let i = 1; i <= 23; i++) {
        grid.innerHTML += `<div class="cell-card"><span class="label">C${i}</span><b id="c${i}-v">0.000V</b></div>`;
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
                    lines.forEach(line => { if (line.trim()) { try { updateUI(JSON.parse(line)); } catch (e) {} } });
                }
            });
            rxChar = await service.getCharacteristic(RX_CHAR_UUID);
            bluetoothDevice.addEventListener('gattserverdisconnected', onDisconnected);
            setStatus(true);
        } catch (e) { alert("Bluetooth Gagal: " + e); }
    }
}

function updateUI(data) {
    if (!isConnected) return;
    
    // 1. GAUGE RPM (100% = 1600 RPM) 
    const rpm = data.rpm || 0;
    const maxRpm = 1600;
    const degrees = Math.min((rpm / maxRpm) * 360, 360);
    
    const circle = document.getElementById('gauge-circle');
    const ball = document.getElementById('glow-ball');
    
    circle.style.setProperty('--deg', `${degrees}deg`);
    circle.style.background = `radial-gradient(var(--card-bg) 64%, transparent 66%), 
                               conic-gradient(from 180deg, var(--cyan) ${degrees}deg, #21262d 0deg)`;
    
    // Bola Cahaya
    ball.style.display = "block";
    const angleRad = (degrees + 180 - 90) * (Math.PI / 180);
    const radius = 94; 
    const x = Math.cos(angleRad) * radius;
    const y = Math.sin(angleRad) * radius;
    ball.style.transform = `translate(calc(-50% + ${x}px), calc(-50% + ${y}px))`;

    // 2. DASHBOARD DATA 
    document.getElementById('speed').innerText = data.speed || 0;
    document.getElementById('rpm-val').innerText = rpm;
    document.getElementById('mode-text').innerText = data.mode || "PARK";
    document.getElementById('soc-dash').innerText = (data.soc || 0) + "%";

    if (data.trip) {
        document.getElementById('range-val').innerText = data.trip.range + " km";
        document.getElementById('trip-val').innerText = data.trip.km.toFixed(1) + " km";
        document.getElementById('trip-dist-large').innerText = data.trip.km.toFixed(1) + " km";
        document.getElementById('avg-wh').innerText = data.trip.avg.toFixed(1);
    }

    // 3. BATTERY 5 KOLOM 
    const v = data.volts || 0;
    const a = data.amps || 0;
    document.getElementById('soc-batt').innerText = (data.soc || 0) + "%";
    document.getElementById('soh-val').innerText = (data.health ? data.health.soh : 0) + "%";
    document.getElementById('v-val').innerText = v.toFixed(1) + "V";
    document.getElementById('a-val').innerText = a.toFixed(1) + "A";
    document.getElementById('w-val').innerText = Math.abs(v * a).toFixed(0) + "W";
    document.getElementById('soc-bar').style.width = (data.soc || 0) + "%";

    if(data.temps) {
        document.getElementById('t-ecu').innerText = data.temps.ctrl + "°";
        document.getElementById('t-motor').innerText = data.temps.motor + "°";
        document.getElementById('t-batt').innerText = data.temps.batt + "°";
    }

    // 4. CELLS
    if (data.cells) {
        data.cells.forEach((mv, i) => {
            const volt = (mv / 1000).toFixed(3);
            const text = document.getElementById(`c${i+1}-v`);
            if (text) text.innerText = volt + "V";
        });
    }
}

function setStatus(status) {
    isConnected = status;
    const btn = document.getElementById('connectBtn');
    btn.innerText = status ? "DISCONNECT" : "CONNECT";
    btn.style.background = status ? "#f85149" : "#58a6ff";
    document.getElementById('conn-status').innerText = status ? "● ONLINE" : "● OFFLINE";
    document.getElementById('conn-status').style.color = status ? "#3fb950" : "#f85149";
}

function onDisconnected() { setStatus(false); document.getElementById('glow-ball').style.display = "none"; }

setInterval(() => {
    document.getElementById('clock').innerText = new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
}, 1000);