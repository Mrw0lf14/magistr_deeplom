document.addEventListener('DOMContentLoaded', function() {
    // Загружаем текущие настройки при загрузке страницы
    loadSettings();
    
    // Обработчик изменения режима WiFi
    document.getElementById('wifi-mode').addEventListener('change', function() {
        const mode = this.value;
        document.getElementById('station-settings').style.display = mode === 'station' ? 'block' : 'none';
        document.getElementById('ap-settings').style.display = mode === 'ap' ? 'block' : 'none';
    });
    
    // Кнопка сканирования WiFi сетей
    document.getElementById('scan-wifi').addEventListener('click', scanWiFi);
    
    // Обработчики форм
    document.getElementById('wifi-settings-form').addEventListener('submit', saveWiFiSettings);
    document.getElementById('usb-settings-form').addEventListener('submit', saveUSBSettings);
    document.getElementById('ports-settings-form').addEventListener('submit', savePortsSettings);
    
    // Системные кнопки
    document.getElementById('reboot-btn').addEventListener('click', rebootDevice);
    document.getElementById('reset-btn').addEventListener('click', resetSettings);
});

function loadSettings() {
    fetch('/api/settings')
        .then(response => response.json())
        .then(data => {
            // WiFi настройки
            document.getElementById('wifi-mode').value = data.wifi.mode;
            document.getElementById('wifi-ssid').value = data.wifi.ssid || '';
            document.getElementById('wifi-password').value = data.wifi.password || '';
            document.getElementById('ap-ssid').value = data.ap.ssid || 'ESP32-AP';
            document.getElementById('ap-password').value = data.ap.password || '';
            
            // Показать/скрыть соответствующие блоки настроек WiFi
            document.getElementById('station-settings').style.display = data.wifi.mode === 'station' ? 'block' : 'none';
            document.getElementById('ap-settings').style.display = data.wifi.mode === 'ap' ? 'block' : 'none';
            
            // USB настройки
            document.getElementById('usb-enabled').checked = data.usb.enabled;
            
            // Настройки портов
            document.getElementById('ext-port1-enabled').checked = data.ports.port1;
            document.getElementById('ext-port2-enabled').checked = data.ports.port2;
        })
        .catch(error => console.error('Error loading settings:', error));
}

function scanWiFi() {
    const wifiList = document.getElementById('wifi-networks');
    wifiList.innerHTML = '<div class="wifi-network">Сканирование...</div>';
    
    fetch('/api/wifi/scan')
        .then(response => response.json())
        .then(networks => {
            wifiList.innerHTML = '';
            
            if (networks.length === 0) {
                wifiList.innerHTML = '<div class="wifi-network">Сети не найдены</div>';
                return;
            }
            
            networks.forEach(network => {
                const networkEl = document.createElement('div');
                networkEl.className = 'wifi-network';
                networkEl.innerHTML = `
                    <strong>${network.ssid}</strong>
                    <span>Сигнал: ${network.rssi}dBm, Канал: ${network.channel}</span>
                    <span>${network.encryption}</span>
                `;
                
                networkEl.addEventListener('click', () => {
                    document.getElementById('wifi-ssid').value = network.ssid;
                });
                
                wifiList.appendChild(networkEl);
            });
        })
        .catch(error => {
            wifiList.innerHTML = '<div class="wifi-network">Ошибка сканирования</div>';
            console.error('Error scanning WiFi:', error);
        });
}

function saveWiFiSettings(e) {
    e.preventDefault();
    
    const settings = {
        wifi: {
            mode: document.getElementById('wifi-mode').value,
            ssid: document.getElementById('wifi-ssid').value,
            password: document.getElementById('wifi-password').value
        },
        ap: {
            ssid: document.getElementById('ap-ssid').value,
            password: document.getElementById('ap-password').value
        }
    };
    
    saveSettings('wifi', settings)
        .then(() => alert('WiFi настройки сохранены! Перезагрузка для применения изменений...'))
        .catch(error => alert('Ошибка сохранения WiFi настроек: ' + error));
}

function saveUSBSettings(e) {
    e.preventDefault();
    
    const settings = {
        enabled: document.getElementById('usb-enabled').checked
    };
    
    saveSettings('usb', settings)
        .then(() => alert('USB настройки сохранены!'))
        .catch(error => alert('Ошибка сохранения USB настроек: ' + error));
}

function savePortsSettings(e) {
    e.preventDefault();
    
    const settings = {
        port1: document.getElementById('ext-port1-enabled').checked,
    };
    
    saveSettings('ports', settings)
        .then(() => alert('Настройки портов сохранены!'))
        .catch(error => alert('Ошибка сохранения настроек портов: ' + error));
}

function saveSettings(type, data) {
    return fetch(`/api/settings/${type}`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(data)
    })
    .then(response => {
        if (!response.ok) throw new Error('Network response was not ok');
        return response.json();
    });
}

function rebootDevice() {
    if (confirm('Вы уверены, что хотите перезагрузить устройство?')) {
        fetch('/api/system/reboot', { method: 'POST' })
            .then(() => alert('Устройство перезагружается...'))
            .catch(error => alert('Ошибка: ' + error));
    }
}

function resetSettings() {
    if (confirm('Вы уверены, что хотите сбросить все настройки к заводским?')) {
        fetch('/api/system/reset', { method: 'POST' })
            .then(() => {
                alert('Настройки сброшены. Устройство перезагружается...');
                setTimeout(() => window.location.reload(), 3000);
            })
            .catch(error => alert('Ошибка: ' + error));
    }
}