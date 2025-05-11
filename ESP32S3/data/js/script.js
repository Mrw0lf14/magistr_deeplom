document.addEventListener('DOMContentLoaded', function() {
    const loginBtn = document.getElementById('login-btn');
    const usernameInput = document.getElementById('username');
    const passwordInput = document.getElementById('password');
    const errorElement = document.getElementById('error');
    let failedAttempts = 0;
    const MAX_ATTEMPTS = 5;
    const BLOCK_TIME = 300000; // 5 минут в миллисекундах

    loginBtn.addEventListener('click', login);
    
    async function login() {
        const username = usernameInput.value;
        const password = passwordInput.value;
        
        if (!username || !password) {
            showError('Введите логин и пароль');
            return;
        }

        try {
            const requestBody = JSON.stringify({username, password});
            const response = await fetch('/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: requestBody
            });
            console.log("Sending:", requestBody); // Добавьте эту строку
            if (response.ok) {
                window.location.href = '/dashboard.html';
            } else {
                handleFailedLogin();
            }
        } catch (err) {
            console.error('Error:', err);
            showError('Ошибка соединения');
        }
    }

    function handleFailedLogin() {
        failedAttempts++;
        const remainingAttempts = MAX_ATTEMPTS - failedAttempts;
        
        if (failedAttempts >= MAX_ATTEMPTS) {
            showError('Слишком много попыток. Попробуйте через 5 минут.');
            loginBtn.disabled = true;
            
            setTimeout(() => {
                failedAttempts = 0;
                errorElement.textContent = '';
                loginBtn.disabled = false;
            }, BLOCK_TIME);
        } else {
            showError(`Неверные данные. Попыток осталось: ${remainingAttempts}`);
        }
    }

    function showError(message) {
        errorElement.textContent = message;
    }
});

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

// Загрузка списка файлов
function loadFiles(path = '') {
    fetch(`/api/files?path=${encodeURIComponent(path)}`)
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            const fileList = document.getElementById('file-list');
            if (!fileList) return;
            
            fileList.innerHTML = '';
            
            // Обновление breadcrumb
            const breadcrumb = document.getElementById('breadcrumb');
            if (breadcrumb) {
                breadcrumb.innerHTML = '<a href="/files" class="dir-link">Корневая папка</a>';
                
                if (path) {
                    const parts = path.split('/').filter(p => p);
                    let currentPath = '';
                    parts.forEach((part, index) => {
                        currentPath += `${part}/`;
                        const separator = document.createElement('span');
                        separator.textContent = ' / ';
                        breadcrumb.appendChild(separator);
                        
                        const link = document.createElement('a');
                        link.href = `#${currentPath}`;
                        link.className = 'dir-link';
                        link.textContent = part;
                        link.onclick = (e) => {
                            e.preventDefault();
                            loadFiles(currentPath);
                        };
                        breadcrumb.appendChild(link);
                    });
                }
            }
            
            // Добавление родительской директории (кроме корня)
            if (path && fileList) {
                const parentPath = path.split('/').slice(0, -1).join('/');
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td class="file-name">
                        <span class="file-icon">📁</span>
                        <a href="#${parentPath}" class="dir-link" onclick="event.preventDefault(); loadFiles('${parentPath}')">..</a>
                    </td>
                    <td>-</td>
                    <td>-</td>
                    <td></td>
                `;
                fileList.appendChild(row);
            }
            
            // Добавление файлов и папок
            if (data.files && fileList) {
                data.files.forEach(file => {
                    const row = document.createElement('tr');
                    
                    if (file.isDir) {
                        row.innerHTML = `
                            <td class="file-name">
                                <span class="file-icon">📁</span>
                                <a href="#${file.fullPath}" class="dir-link" onclick="event.preventDefault(); loadFiles('${file.fullPath}')">${file.name}</a>
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
            }
        })
        .catch(error => {
            console.error('Error fetching files:', error);
            const fileList = document.getElementById('file-list');
            if (fileList) {
                fileList.innerHTML = '<tr><td colspan="4">Ошибка загрузки файлов</td></tr>';
            }
        });
}
