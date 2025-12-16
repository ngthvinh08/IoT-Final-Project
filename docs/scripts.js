// Giả lập chức năng bật/tắt đèn
function toggleLight() {
    const lightToggle = document.getElementById('light-toggle');
    const lightStatus = document.getElementById('light-status');
    const deviceCard = lightToggle.closest('.device-card');
    
    if (lightToggle.checked) {
        // Đây là nơi bạn sẽ gửi lệnh BẬT đến API của RainMaker/Blynk
        lightStatus.textContent = "Đang Bật";
        deviceCard.style.backgroundColor = '#4d4d4d'; // Màu nền hơi sáng hơn khi bật
    } else {
        // Đây là nơi bạn sẽ gửi lệnh TẮT đến API của RainMaker/Blynk
        lightStatus.textContent = "Đang Tắt";
        deviceCard.style.backgroundColor = '#2c2c2c'; // Màu nền ban đầu
    }
}

// Giả lập chức năng thay đổi tốc độ quạt
let fanSpeed = 1;
function changeFanSpeed() {
    const fanBtn = document.getElementById('fan-speed-btn');
    const fanStatus = document.getElementById('fan-status');
    
    fanSpeed = (fanSpeed % 3) + 1; // Lặp qua 1, 2, 3
    
    // Đây là nơi bạn sẽ gửi lệnh thay đổi tốc độ quạt đến API
    fanBtn.textContent = `Tốc độ ${fanSpeed}`;
    fanStatus.textContent = `Đang Bật - Tốc độ ${fanSpeed}`;
    
    console.log(`Đã gửi lệnh: Thay đổi tốc độ quạt lên ${fanSpeed}`);
}

// Gọi hàm toggleLight khi tải trang để thiết lập trạng thái ban đầu
document.addEventListener('DOMContentLoaded', () => {
    // Thiết lập trạng thái ban đầu
    document.getElementById('light-toggle').checked = false; 
    toggleLight(); 
});