# 🤖 Dự Án AIoT Giám Sát và Dự Đoán Chất Lượng Không Khí (AQI) 🌬️

## Giới Thiệu

Đây là dự án **Internet of Things (IoT)** tích hợp **Trí tuệ Nhân tạo Nhúng (TinyML)** để xây dựng hệ thống giám sát và dự đoán Chỉ số Chất lượng Không khí (**AQI**) theo thời gian thực. Hệ thống sử dụng vi điều khiển giá rẻ **ESP32** để chạy mô hình học sâu, cho phép dự đoán AQI ngay tại thiết bị mà không cần kết nối đám mây, đảm bảo tính tự chủ, tốc độ phản hồi nhanh và tiết kiệm năng lượng.

---

## 🎯 Mục Tiêu Cốt Lõi

* **Xác thực TinyML:** Triển khai thành công mô hình dự báo chuỗi thời gian **GRU** lên vi điều khiển **ESP32** có tài nguyên hạn chế.
* **Dự đoán theo thời gian thực:** Dự đoán chỉ số AQI tại thời điểm **$t+1$** dựa trên chuỗi dữ liệu đầu vào là **24 giờ** trước đó.
* **Hoạt động độc lập:** Đảm bảo hệ thống có thể thực hiện suy luận và cảnh báo ngay cả khi **mất kết nối Internet**.

---

## ⚙️ Kiến Trúc Hệ Thống

Dự án sử dụng kiến trúc ba lớp kết hợp **Edge Computing** và **Cloud IoT**:

1.  **Lớp Thiết bị (Device Layer / Edge AI):**
    * **Vi điều khiển:** **ESP32** (làm Sink Node).
    * **Cảm biến (Đầu vào 24x4):** **DHT22** (Nhiệt độ/Độ ẩm), **MQ-7** (CO), **GP2Y1010AU0F** (PM2.5).
    * **Chức năng:** Thu thập, **Tiền xử lý** (làm sạch, chuẩn hóa Min-Max), **Suy luận TinyML** cục bộ, và **Giải chuẩn hóa** kết quả.
2.  **Lớp Nền tảng (Platform Layer):** **ThingsBoard Cloud** để quản lý thiết bị, lưu trữ dữ liệu và nhận kết quả dự đoán qua giao thức **MQTT**.
3.  **Lớp Ứng dụng (Application Layer):** Giao diện **Dashboard** hiển thị trực quan dữ liệu thực tế và dự đoán, cùng chức năng **cảnh báo** khi AQI vượt ngưỡng.

---

## 🧠 Mô Hình Học Sâu (TinyML)

| Tiêu chí | Chi tiết | Phân tích |
| :--- | :--- | :--- |
| **Mô hình** | **GRU (Gated Recurrent Unit)**| Được chọn vì hiệu quả xử lý chuỗi thời gian và cấu trúc đơn giản hơn LSTM, tối ưu cho TinyML|
| **Kiến trúc** | 1 lớp GRU (32 units) + Dropout (0.2) + Dense (16) + Dense Output (1)| Tổng tham số nhỏ: **4193** (~16.38KB)|
| **Tối ưu hóa** | TensorFlow Lite Micro (**TFLite Micro**), sử dụng nền tảng **Edge Impulse** để đóng gói thành thư viện Arduino| Lựa chọn **Float32** (thay vì Int8) để giữ **độ chính xác tối đa** cho AQI, tận dụng FPU của ESP32|

---

## ✅ Kết Quả và Hiệu Năng Đạt Được

| Chỉ số | Giá trị | Ý nghĩa |
| :--- | :--- | :--- |
| **Sai số (Test MSE)** | $\approx 0.0268$ ộ chính xác cao, chứng minh khả năng học được mối quan hệ phi tuyến|
| **Kích thước Mô hình (Flash)** | $\approx 98KB$| Rất nhỏ gọn, chiếm dưới 10% dung lượng Flash của ESP32|
| **Bộ nhớ RAM (Arena)** | $\approx 103KB$| Phù hợp với giới hạn bộ nhớ RAM của ESP32|
| **Thời gian Suy luận** | $\approx 259$ mili-giây| Tốc độ dự đoán nhanh, đáp ứng yêu cầu dự đoán AQI theo giờ|
| **Độ tin cậy** | Đường biến thiên dự đoán **bám sát tốt** dữ liệu thực tế. Sai lệch chủ yếu xuất hiện ở các **biến động đột ngột**|

---

## 📈 Hạn Chế và Hướng Phát Triển

* **Mở rộng Đặc trưng:** Bổ sung các yếu tố khí tượng quan trọng (tốc độ gió, hướng gió, áp suất khí quyển) và các loại khí độc khác (NO₂, SO₂).
* **Học thích ứng:** Nghiên cứu kỹ thuật **Adaptive Learning** để mô hình tự cập nhật khi môi trường thay đổi.
* **Cập nhật OTA:** Phát triển cơ chế **Cập nhật Mô hình Từ xa** (Over-the-Air Model Update) để duy trì hiệu năng trong dài hạn.
* **Ứng dụng Di động:** Xây dựng ứng dụng đồng bộ với Dashboard để tăng tiện ích cho người dùng cuối.