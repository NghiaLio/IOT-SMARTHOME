# Hệ thống Cấu hình WiFi cho ESP32

## Mô tả

Dự án này là một hệ thống cấu hình WiFi cho các thiết bị ESP32 sử dụng ESP-IDF. Hệ thống cho phép quản lý kết nối WiFi, cung cấp giao diện web để cấu hình, hỗ trợ I2S cho âm thanh, tích hợp module RFID RC522, và các tính năng khác như backup dữ liệu.

## Tính năng chính

- **Quản lý WiFi**: Kết nối và cấu hình mạng WiFi.
- **Giao diện web**: Cung cấp trang web để cấu hình qua trình duyệt.
- **Hỗ trợ I2S**: Xử lý âm thanh qua giao thức I2S.
- **RFID**: Tích hợp module RC522 để đọc thẻ RFID.
- **Backup dữ liệu**: Lưu trữ và khôi phục cấu hình.
- **LED Strip**: Hỗ trợ đèn LED địa chỉ (WS2812).

## Yêu cầu phần cứng

- Board phát triển ESP32 (ví dụ: ESP32-S3-DevKitC, ESP32-C6-DevKitC, v.v.)
- Module RFID RC522 (tùy chọn)
- Đèn LED địa chỉ WS2812 (tùy chọn)
- Cáp USB để cấp nguồn và lập trình

## Cách sử dụng

### Thiết lập môi trường

Đảm bảo ESP-IDF đã được cài đặt và cấu hình. Thiết lập biến môi trường `IDF_PATH` (ví dụ: `C:\Users\ADMIN\esp\v5.5.1\esp-idf`).

### Cấu hình dự án

1. Sao chép dự án vào thư mục làm việc.
2. Chạy `idf.py set-target <chip_name>` để thiết lập chip mục tiêu (ví dụ: esp32s3).
3. Chạy `idf.py menuconfig` để cấu hình dự án nếu cần.

### Build và Flash

Chạy `idf.py -p PORT flash monitor` để build, flash và monitor dự án.

(To exit the serial monitor, type `Ctrl-]`.)

### Giám sát

Sử dụng ESP-IDF Monitor để xem log và tương tác với thiết bị.

## Cấu trúc dự án chi tiết

```
wifi-config/
│
├── main/                          # Thư mục mã nguồn chính
│   ├── main.c                     # File chính của chương trình (không sử dụng)
│   ├── i2s.c                      # File chính thực tế - Khởi tạo hệ thống
│   │
│   ├── wifi_manager.c/h           # Quản lý WiFi với Access Point và Web Server
│   ├── wifi_handler.c/h           # Xử lý kết nối WiFi Station mode
│   │
│   ├── rfid_handler.c/h           # Xử lý module RFID RC522
│   ├── firebase_handler.c/h       # Tích hợp Firebase (lấy/gửi dữ liệu)
│   │
│   ├── i2s_mic.c/h                # Xử lý microphone qua I2S
│   ├── voice_handler.c/h          # Xử lý giọng nói với Wit.ai API
│   │
│   ├── sensors.c/h                # Quản lý cảm biến (DHT, ADC)
│   ├── actuators.c/h              # Điều khiển thiết bị (LED, Servo, Buzzer)
│   │
│   ├── backup.c                   # Backup và restore dữ liệu
│   ├── test.c                     # File kiểm thử
│   │
│   ├── html_page.h                # Giao diện web cấu hình WiFi
│   ├── config.h                   # Cấu hình chân GPIO và hằng số
│   ├── data.json                  # Dữ liệu cấu hình mẫu
│   │
│   ├── CMakeLists.txt             # Cấu hình CMake cho thư mục main
│   ├── Kconfig.projbuild          # Menu cấu hình menuconfig
│   └── idf_component.yml          # Khai báo dependencies
│
├── managed_components/            # Thành phần quản lý bởi IDF Component Manager
│   ├── abobija__rc522/            # Thư viện RFID RC522
│   ├── espressif__cJSON/          # Thư viện xử lý JSON
│   ├── espressif__led_strip/      # Thư viện điều khiển LED WS2812
│   └── espressif__esp_codec_dev/  # Thư viện codec âm thanh
│
├── build/                         # Thư mục build (tự động tạo)
│   ├── bootloader/                # Bootloader đã build
│   ├── esp-idf/                   # Các component ESP-IDF đã build
│   ├── partition_table/           # Bảng phân vùng
│   └── *.bin                      # File binary để flash
│
├── CMakeLists.txt                 # Cấu hình CMake chính của dự án
├── partitions.csv                 # Định nghĩa bảng phân vùng flash
│
├── sdkconfig                      # Cấu hình SDK chính (tự sinh)
├── sdkconfig.defaults             # Cấu hình mặc định chung
├── sdkconfig.defaults.esp32*      # Cấu hình mặc định cho từng chip
│
├── pytest_blink.py                # Script kiểm thử Python
└── README.md                      # File tài liệu này
```

## Chức năng các module chính

### 1. WiFi Management (`wifi_manager.c`, `wifi_handler.c`)

- **WiFi Manager**:
  - Khởi động Access Point (AP) mode để cấu hình
  - Cung cấp Web Server tại `192.168.4.1` với giao diện HTML
  - Lưu/đọc cấu hình WiFi từ NVS (Non-Volatile Storage)
  - Quản lý chuyển đổi giữa AP và Station mode
- **WiFi Handler**:
  - Kết nối WiFi ở chế độ Station
  - Xử lý sự kiện kết nối/mất kết nối
  - Tự động retry khi mất kết nối

### 2. RFID System (`rfid_handler.c`)

- Khởi tạo và cấu hình module RC522 qua SPI
- Đọc UID từ thẻ/tag RFID
- Lưu UID vào NVS để quản lý truy cập
- Xử lý sự kiện phát hiện thẻ mới

### 3. Firebase Integration (`firebase_handler.c`)

- Kết nối với Firebase Realtime Database
- Lấy dữ liệu cấu hình từ cloud
- Gửi dữ liệu cảm biến lên Firebase
- Đồng bộ trạng thái thiết bị

### 4. Voice Processing (`i2s_mic.c`, `voice_handler.c`)

- **I2S Microphone**:
  - Khởi tạo giao tiếp I2S với microphone
  - Thu âm thanh với sampling rate có thể cấu hình
  - Xử lý buffer âm thanh
- **Voice Handler**:
  - Gửi dữ liệu âm thanh đến Wit.ai API
  - Phân tích phản hồi JSON từ Wit.ai
  - Trích xuất intent và entities từ giọng nói

### 5. Sensors (`sensors.c`)

- **DHT Sensor**: Đọc nhiệt độ và độ ẩm
- **ADC**: Đọc giá trị analog từ các cảm biến
- Hỗ trợ đa loại cảm biến môi trường

### 6. Actuators (`actuators.c`)

- **LED Control**: Nhấp nháy LED thông báo
- **Servo Motor**: Điều khiển góc quay (0-180°)
  - Servo chính (cửa)
  - Servo phụ (mưa)
- **Buzzer**: Phát âm thanh cảnh báo
  - Beep ngắn
  - Alarm liên tục

### 7. Data Backup (`backup.c`)

- Sao lưu cấu hình vào NVS
- Khôi phục cấu hình khi khởi động
- Bảo vệ dữ liệu khi mất nguồn

## Cấu hình GPIO (config.h)

Tham khảo file `main/config.h` để cấu hình các chân GPIO cho:

- RFID RC522 (SPI pins)
- I2S Microphone (SCK, WS, DIN)
- DHT Sensor
- LED, Servo, Buzzer
- LED Strip WS2812

## Quy trình hoạt động

1. **Khởi động**:

   - Kiểm tra cấu hình WiFi trong NVS
   - Nếu chưa có → Khởi động AP mode + Web Server
   - Nếu có → Kết nối WiFi Station mode

2. **Cấu hình qua Web**:

   - Truy cập `http://192.168.4.1`
   - Nhập SSID và Password
   - Hệ thống lưu và khởi động lại ở Station mode

3. **Hoạt động bình thường**:
   - Kết nối Firebase để đồng bộ
   - Đọc cảm biến định kỳ
   - Xử lý RFID khi có thẻ
   - Nhận lệnh giọng nói (nếu có)
   - Điều khiển actuators theo logic

## Cấu hình SDK

Các file `sdkconfig.defaults.*` chứa cấu hình tối ưu cho từng chip:

- **ESP32-S3**: Hỗ trợ đầy đủ WiFi, I2S, SPI
- **ESP32-C3/C6**: Chip RISC-V, tiết kiệm năng lượng
- **ESP32-H2**: Hỗ trợ Thread/Zigbee
- **ESP32-P4**: Chip hiệu suất cao

Chạy `idf.py menuconfig` để tùy chỉnh:

- WiFi settings
- Partition table
- Component configs
- Debug level

## Bảng phân vùng (partitions.csv)

Định nghĩa cách phân chia bộ nhớ flash:

- Bootloader
- Partition table
- NVS (lưu WiFi config, RFID UIDs)
- OTA partitions (nếu có)
- Application firmware
- Storage (SPIFFS/FAT)

## Ghi chú

Dự án này sử dụng ESP-IDF phiên bản 5.5.1. Đảm bảo tương thích với phiên bản ESP-IDF của bạn.

Để biết thêm thông tin, tham khảo [Tài liệu ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/).
