// Форматирование размера файла
function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Обновление статуса батареи
function updateBatteryStatus() {
    fetch('/api/battery')
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            const batteryLevel = document.getElementById('battery-level');
            if (batteryLevel) {
                batteryLevel.textContent = `${data.level}%`;
            }
            
            const chargingStatus = document.getElementById('charging-status');
            if (chargingStatus) {
                chargingStatus.textContent = data.charging ? '⚡' : '';
            }
        })
        .catch(error => console.error('Error fetching battery status:', error));
}

// Форматирование размера файла
function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Загрузка и отображение списка файлов
function loadFiles() {
    fetch('/api/files')
        .then(response => {
            if (!response.ok) throw new Error('Ошибка сети');
            return response.json();
        })
        .then(data => {
            const fileList = document.getElementById('file-list');
            if (!fileList) return;
            
            fileList.innerHTML = '';
            
            // Добавление файлов и папок
            if (data.files && data.files.length > 0) {
                data.files.forEach(file => {
                    const row = document.createElement('tr');
                    
                    if (file.isDir) {
                        row.innerHTML = `
                            <td class="file-name">
                                <span class="file-icon">📁</span>
                                ${file.name}
                            </td>
                            <td>-</td>
                            <td>${file.modified || '-'}</td>
                            <td></td>
                        `;
                    } else {
                        row.innerHTML = `
                            <td class="file-name">
                                <span class="file-icon">📄</span>
                                ${file.name}
                            </td>
                            <td>${file.size ? formatFileSize(file.size) : '-'}</td>
                            <td>${file.modified || '-'}</td>
                            <td><a href="/download?path=${encodeURIComponent(file.fullPath)}" class="download-btn">Скачать</a></td>
                        `;
                    }
                    
                    fileList.appendChild(row);
                });
            } else {
                fileList.innerHTML = '<tr><td colspan="4">На SD карте нет файлов</td></tr>';
            }
        })
        .catch(error => {
            console.error('Ошибка:', error);
            const fileList = document.getElementById('file-list');
            if (fileList) {
                fileList.innerHTML = '<tr><td colspan="4">Ошибка загрузки списка файлов</td></tr>';
            }
        });
}

// Инициализация и периодическое обновление
document.addEventListener('DOMContentLoaded', function() {
    // Первоначальная загрузка
    loadFiles();
    
    // Обновление каждые 30 секунд
    setInterval(loadFiles, 30000);
    
    // Обновление статуса батареи
    updateBatteryStatus();
    setInterval(updateBatteryStatus, 60000);
});