// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var realtimeChart;
const MAX_DATA_POINTS = 20; // Số lượng điểm dữ liệu hiển thị trên biểu đồ
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
            const cfg = data.config || null;
            if (cfg) {
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

            // state handling: connecting / connected / failed
            const state = data.state || null;
            try { renderStatusExtras(data.sta || '', cfg, state, data); } catch (e) { console.warn('renderStatusExtras failed', e); }

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
            updateChart(t, h);
        }
    } catch (e) {
        console.warn("Không phải JSON hợp lệ:", event.data);
    }
}

// Render QR code and show Open STA button; try to probe and open STA URL if reachable
function renderStatusExtras(staIp, cfg, state, raw) {
    const openBtn = document.getElementById('openStaBtn');
    const note = document.getElementById('openNote');
    const qrWrap = document.getElementById('qr');
    if (!qrWrap) return;
    // clear previous
    qrWrap.innerHTML = '';
    if (!staIp || staIp === '0.0.0.0') {
        // If in connecting state, still show note and QR for AP or saved config
        if (state === 'connecting') {
            if (openBtn) openBtn.style.display = 'none';
            if (note) note.textContent = 'Đang cố gắng kết nối STA...';
        } else {
            if (openBtn) openBtn.style.display = 'none';
            if (note) note.textContent = 'Thiết bị chưa kết nối STA.';
        }
        return;
    }
    const url = 'http://' + staIp + '/';
    // create QR
    try {
        new QRCode(qrWrap, { text: url, width: 120, height: 120, colorDark: '#000000', colorLight: '#ffffff', correctLevel: QRCode.CorrectLevel.H });
    } catch (e) {
        console.warn('QRCode generation failed', e);
    }
    if (openBtn) {
        openBtn.style.display = 'inline-block';
        openBtn.onclick = function () { window.open(url, '_blank'); };
    }
    // Update note depending on state
    if (state === 'connected') {
        if (note) note.textContent = 'STA connected: ' + staIp + ' — opening...';
        try { window.open(url, '_blank'); } catch (e) { console.warn('open failed', e); }
    } else if (state === 'connecting') {
        if (note) note.textContent = 'Đang cố gắng kết nối STA: ' + staIp + '...';
        // don't force-open yet, but allow user to press the button
    } else {
        if (note) note.textContent = 'STA available: ' + staIp;
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
    // Khởi tạo Chart.js
    const ctx = document.getElementById('realtimeChart').getContext('2d');
    realtimeChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [], // Thời gian sẽ hiển thị ở đây
            datasets: [{
                label: 'Nhiệt độ (°C)',
                borderColor: '#F44336',
                backgroundColor: 'rgba(244, 67, 54, 0.1)',
                data: [],
                borderWidth: 2,
                tension: 0.4 // Tạo độ cong cho đường
            }, {
                label: 'Độ ẩm (%)',
                borderColor: '#2196F3',
                backgroundColor: 'rgba(33, 150, 243, 0.1)',
                data: [],
                borderWidth: 2,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            scales: {
                x: { title: { display: true, text: 'Thời gian' } },
                y: { beginAtZero: false }
            }
        }
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
    // Update Info UI immediately so user sees saved config before server status arrives
    cfgPasswordActual = password;
    updatePasswordDisplay();
    const sEl = document.getElementById('cfg_ssid'); if (sEl) sEl.textContent = ssid || '(chưa có)';
    const tEl = document.getElementById('cfg_token'); if (tEl) tEl.textContent = token || '(chưa có)';
    const srvEl = document.getElementById('cfg_server'); if (srvEl) srvEl.textContent = server || '(chưa có)';
    const prtEl = document.getElementById('cfg_port'); if (prtEl) prtEl.textContent = port || '(chưa có)';
    const statusEl = document.getElementById('deviceStatus'); if (statusEl) statusEl.innerHTML = 'Trạng thái thiết bị: Đang cố gắng kết nối STA...';

    alert("✅ Cấu hình đã được gửi đến thiết bị!");
});


function updateChart(temp, humi) {
    if (!realtimeChart) return;

    const now = new Date();
    const timeString = now.getHours() + ":" + now.getMinutes() + ":" + now.getSeconds();

    // Thêm nhãn thời gian mới
    realtimeChart.data.labels.push(timeString);
    
    // Thêm dữ liệu mới (nếu giá trị null thì lấy giá trị cuối cùng để đường không bị đứt)
    realtimeChart.data.datasets[0].data.push(temp);
    realtimeChart.data.datasets[1].data.push(humi);

    // Nếu quá nhiều điểm dữ liệu, xóa điểm cũ nhất để biểu đồ "trượt"
    if (realtimeChart.data.labels.length > MAX_DATA_POINTS) {
        realtimeChart.data.labels.shift();
        realtimeChart.data.datasets[0].data.shift();
        realtimeChart.data.datasets[1].data.shift();
    }

    realtimeChart.update('none'); // Update mà không cần hiệu ứng animation nặng để mượt hơn
}