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