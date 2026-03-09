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
    if (window.event && window.event.currentTarget) window.event.currentTarget.classList.add('active');
    if (pageId === 'battery') generateCells();
}

function generateCells() {
    const grid = document.getElementById('cell-grid');
    if (!grid || grid.innerHTML !== "") return;
    for (let i = 1; i <= 23; i++) {
        grid.innerHTML += `
            <div class="cell-card">
                <span class="cell-id">C${i}</span>
                <b id="c${i}-v">0.000V</b>
                <div class="cell-level-bg"><div id="c${i}-f" class="cell-level-fill" style="width:0%"></div></div>
            </div>`;
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
            txChar.addEventListener('characteristicvaluechanged', handleData);
            rxChar = await service.getCharacteristic(RX_CHAR_UUID);
            bluetoothDevice.addEventListener('gattserverdisconnected', onDisconnected);
            setStatus(true);
        } catch (e) { console.log(e); }
    }
}

function handleData(event) {
    let chunk = new TextDecoder().decode(event.target.value);
    rxBuffer += chunk;
    if (rxBuffer.includes('\n')) {
        let lines = rxBuffer.split('\n');
        rxBuffer = lines.pop();
        lines.forEach(line => {
            if (line.trim()) {
                try {
                    const data = JSON.parse(line);
                    updateUI(data);
                    const logBox = document.getElementById('log-box');
                    if (logBox) logBox.innerText = line + "\n" + logBox.innerText.substring(0, 1000);
                } catch (e) {}
            }
        });
    }
}

function updateUI(data) {
    if (!isConnected) return;
    
    // Dash & Main Info
    document.getElementById('speed').innerText = data.speed || 0;
    document.getElementById('rpm').innerText = data.rpm || 0;
    
    // UPDATE SOC: Sinkron Dashboard & Battery Page
    const socVal = (data.soc || 0) + "%";
    document.getElementById('soc-main').innerText = socVal;
    document.getElementById('soc-batt').innerText = socVal;
    document.getElementById('soc-fill').style.width = socVal;
    
    // Parabolic Gauge
    const rpmPercent = Math.min(((data.rpm || 0) / 8000) * 360, 360);
    document.querySelector('.speed-circle').style.background = 
        `radial-gradient(var(--card-bg) 64%, transparent 65%), conic-gradient(var(--cyan) ${rpmPercent}deg, #21262d 0deg)`;

    // Temps - 3 Kolom
    if (data.temps) {
        document.getElementById('t-ecu').innerText = data.temps.ctrl + "°";
        document.getElementById('t-motor').innerText = data.temps.motor + "°";
        document.getElementById('t-batt').innerText = data.temps.batt + "°";
    }

    // Battery Stats
    const v = data.volts || 0;
    const a = data.amps || 0;
    document.getElementById('volts').innerText = v.toFixed(1) + "V";
    document.getElementById('amps').innerText = a.toFixed(1) + "A";
    document.getElementById('watts').innerText = Math.abs(v * a).toFixed(0) + "W";
    document.getElementById('power-use').innerText = Math.abs(v * a).toFixed(0) + "W";

    // Trip Info
    document.getElementById('trip').innerText = (data.trip || 0.0).toFixed(1) + " km";
    document.getElementById('trip-dist').innerText = (data.trip || 0.0).toFixed(1) + " km";

    // Cells Update
    if (data.cells) {
        data.cells.forEach((mv, i) => {
            const volt = mv / 1000;
            const text = document.getElementById(`c${i+1}-v`);
            const fill = document.getElementById(`c${i+1}-f`);
            if (text) text.innerText = volt.toFixed(3) + "V";
            if (fill) fill.style.width = Math.min(Math.max((volt - 3.0) / 1.2 * 100, 0), 100) + "%";
        });
    }
}

async function sendSplash() {
    if (!rxChar) return;
    const val = document.getElementById('splashInput').value;
    if (val) {
        await rxChar.writeValue(new TextEncoder().encode(`SPLASH,${val}\n`));
        alert("Splash sent!");
    }
}

async function syncTime() {
    if (!rxChar) return;
    const n = new Date();
    const cmd = `TIME,${n.getFullYear()},${n.getMonth()+1},${n.getDate()},${n.getHours()},${n.getMinutes()},${n.getSeconds()}\n`;
    await rxChar.writeValue(new TextEncoder().encode(cmd));
    alert("Time synced!");
}

function setStatus(status) {
    isConnected = status;
    const btn = document.getElementById('connectBtn');
    btn.innerText = status ? "DISCONNECT" : "CONNECT";
    btn.style.background = status ? "#f85149" : "#58a6ff";
    const st = document.getElementById('conn-status');
    st.className = status ? "status-online" : "status-offline";
    st.innerText = status ? "● ONLINE" : "● OFFLINE";
    if (!status) resetUI();
}

function onDisconnected() { setStatus(false); }

function resetUI() {
    const ids = ['speed','rpm','soc-main','soc-batt','volts','amps','watts','t-ecu','t-motor','t-batt','trip','trip-dist'];
    ids.forEach(id => {
        const el = document.getElementById(id);
        if (el) el.innerText = id.includes('soc') ? "0%" : (id.includes('t-') ? "0°" : "0");
    });
    document.getElementById('soc-fill').style.width = "0%";
}

setInterval(() => {
    const clock = document.getElementById('clock');
    if (clock) clock.innerText = new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
}, 1000);