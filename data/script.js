// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
}

function onOpen(event) {
    console.log('Connection opened');
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Gửi:", data);
    } else {
        console.warn("⚠️ WebSocket chưa sẵn sàng!");
        alert("⚠️ WebSocket chưa kết nối!");
    }
}

function onMessage(event) {
    console.log("📩 Nhận:", event.data);
    try {
        var data = JSON.parse(event.data);

        // Nếu đây là thông điệp trạng thái (gửi khi client kết nối)
        if (data.type && data.type === 'status') {
            const statusEl = document.getElementById('deviceStatus');
            const sta = data.sta || '';
            const ap = data.ap || '';
            let text = '';
            if (sta && sta !== '0.0.0.0') {
                text += `STA IP: <a href="http://${sta}" target="_blank">${sta}</a>`;
            } else {
                text += 'STA IP: (not connected)';
            }
            text += ' — ';
            if (ap && ap !== '0.0.0.0') {
                text += `AP IP: ${ap}`;
            } else {
                text += 'AP IP: (none)';
            }
            if (statusEl) statusEl.innerHTML = 'Trạng thái thiết bị: ' + text;

            // If config is present, populate the info section fields
            if (data.config) {
                const cfg = data.config;
                const s = document.getElementById('cfg_ssid');
                const p = document.getElementById('cfg_pass');
                const t = document.getElementById('cfg_token');
                const srv = document.getElementById('cfg_server');
                const prt = document.getElementById('cfg_port');
                if (s) s.textContent = cfg.ssid || '(chưa có)';
                cfgPasswordActual = cfg.password || '';
                updatePasswordDisplay();
                if (t) t.textContent = cfg.token || '(chưa có)';
                if (srv) srv.textContent = cfg.server || '(chưa có)';
                if (prt) prt.textContent = cfg.port || '(chưa có)';
            }

            return; // status handled
        }
        // Chuẩn hóa giá trị nhiệt độ/độ ẩm nếu có
        var t = (data.temperature !== undefined && !isNaN(parseFloat(data.temperature))) ? parseFloat(data.temperature) : null;
        var h = (data.humidity !== undefined && !isNaN(parseFloat(data.humidity))) ? parseFloat(data.humidity) : null;

        if (t !== null || h !== null) {
            // Hiện container gauge lần đầu nhận dữ liệu
            if (!window.hasSensorData) {
                const gauges = document.querySelector('.gauges-container');
                if (gauges) gauges.style.display = 'flex';
                window.hasSensorData = true;
            }
            if (t !== null && gaugeTemp) gaugeTemp.refresh(t);
            if (h !== null && gaugeHumi) gaugeHumi.refresh(h);
        }
    } catch (e) {
        console.warn("Không phải JSON hợp lệ:", event.data);
    }
}


// ==================== UI NAVIGATION ====================
let relayList = [];
let deleteTarget = null;

function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');
    document.getElementById(id).style.display = id === 'settings' ? 'flex' : 'block';
    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    event.currentTarget.classList.add('active');
}


// ==================== HOME GAUGES ====================
// make gauges accessible from onMessage
var gaugeTemp = null;
var gaugeHumi = null;
window.onload = function () {
    // hide gauges until first sensor data is received
    window.hasSensorData = false;
    const gauges = document.querySelector('.gauges-container');
    if (gauges) gauges.style.display = 'none';

    gaugeTemp = new JustGage({
        id: "gauge_temp",
        value: 0,
        min: 0,
        max: 100,
        donut: true,
        pointer: false,
        gaugeWidthScale: 0.25,
        gaugeColor: "transparent",
        levelColorsGradient: true,
        levelColors: ["#00BCD4", "#4CAF50", "#FFC107", "#F44336"]
    });

    gaugeHumi = new JustGage({
        id: "gauge_humi",
        value: 0,
        min: 0,
        max: 100,
        donut: true,
        pointer: false,
        gaugeWidthScale: 0.25,
        gaugeColor: "transparent",
        levelColorsGradient: true,
        levelColors: ["#42A5F5", "#00BCD4", "#0288D1"]
    });
};

// ==================== CONFIG UI HELPERS ====================
var cfgPasswordActual = '';
var cfgShowPassword = false;
function togglePassword() {
    cfgShowPassword = !cfgShowPassword;
    const btn = document.getElementById('togglePassBtn');
    if (btn) btn.textContent = cfgShowPassword ? 'Ẩn' : 'Hiện';
    updatePasswordDisplay();
}
function updatePasswordDisplay() {
    const el = document.getElementById('cfg_pass');
    if (!el) return;
    if (!cfgPasswordActual) el.textContent = '(chưa có)';
    else el.textContent = cfgShowPassword ? cfgPasswordActual : '•'.repeat(Math.min(20, cfgPasswordActual.length));
}
function requestConfig() {
    const msg = JSON.stringify({ page: 'get_config' });
    Send_Data(msg);
}


// ==================== DEVICE FUNCTIONS ====================
function openAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'flex';
}
function closeAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'none';
}
function saveRelay() {
    const name = document.getElementById('relayName').value.trim();
    const gpio = document.getElementById('relayGPIO').value.trim();
    if (!name || !gpio) return alert("⚠️ Please fill all fields!");
    relayList.push({ id: Date.now(), name, gpio, state: false });
    renderRelays();
    closeAddRelayDialog();
}
function renderRelays() {
    const container = document.getElementById('relayContainer');
    container.innerHTML = "";
    relayList.forEach(r => {
        const card = document.createElement('div');
        card.className = 'device-card';
        card.innerHTML = `
      <i class="fa-solid fa-bolt device-icon"></i>
      <h3>${r.name}</h3>
      <p>GPIO: ${r.gpio}</p>
      <button class="toggle-btn ${r.state ? 'on' : ''}" onclick="toggleRelay(${r.id})">
        ${r.state ? 'ON' : 'OFF'}
      </button>
      <i class="fa-solid fa-trash delete-icon" onclick="showDeleteDialog(${r.id})"></i>
    `;
        container.appendChild(card);
    });
}
function toggleRelay(id) {
    const relay = relayList.find(r => r.id === id);
    if (relay) {
        relay.state = !relay.state;
        const relayJSON = JSON.stringify({
            page: "device",
            value: {
                name: relay.name,
                status: relay.state ? "ON" : "OFF",
                gpio: relay.gpio
            }
        });
        Send_Data(relayJSON);
        renderRelays();
    }
}
function showDeleteDialog(id) {
    deleteTarget = id;
    document.getElementById('confirmDeleteDialog').style.display = 'flex';
}
function closeConfirmDelete() {
    document.getElementById('confirmDeleteDialog').style.display = 'none';
}
function confirmDelete() {
    relayList = relayList.filter(r => r.id !== deleteTarget);
    renderRelays();
    closeConfirmDelete();
}


// ==================== SETTINGS FORM (BỔ SUNG) ====================
document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const ssid = document.getElementById("ssid").value.trim();
    const password = document.getElementById("password").value.trim();
    const token = document.getElementById("token").value.trim();
    const server = document.getElementById("server").value.trim();
    const port = document.getElementById("port").value.trim();

    const settingsJSON = JSON.stringify({
        page: "setting",
        value: {
            ssid: ssid,
            password: password,
            token: token,
            server: server,
            port: port
        }
    });

    Send_Data(settingsJSON);
    alert("✅ Cấu hình đã được gửi đến thiết bị!");
});
