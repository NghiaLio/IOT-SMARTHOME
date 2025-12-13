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

## Cấu trúc dự án

- `main/`: Chứa mã nguồn chính (main.c, wifi_manager.c, i2s.c, backup.c, v.v.)
- `managed_components/`: Các thành phần được quản lý (RC522, cJSON, led_strip, v.v.)
- `build/`: Thư mục build (tự động tạo)
- `sdkconfig*`: Cấu hình SDK cho các chip khác nhau

## Ghi chú

Dự án này sử dụng ESP-IDF phiên bản 5.5.1. Đảm bảo tương thích với phiên bản ESP-IDF của bạn.

Để biết thêm thông tin, tham khảo [Tài liệu ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/).
