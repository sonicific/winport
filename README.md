# Sonic79 Reconstructed

Đây là bản C++/Win32 **clean-room** được viết lại từ hành vi đã quan sát của hai
binary `Sonic_79*.exe` và tài liệu công khai của Device Cleanup Tool 1.5.1.
Project không chứa source, icon, certificate hoặc resource sao chép từ binary gốc.

Kết luận reverse quan trọng: cả hai binary Sonic có toàn bộ section code/data chính
giống từng byte với `DeviceCleanup.exe` 1.5.1 x64 chính thức. Chúng chỉ thay resource,
làm hỏng vị trí Authenticode và do đó không còn chữ ký hợp lệ. Chi tiết và hash kiểm
chứng nằm trong [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md).

## Chức năng đã dựng lại

- Liệt kê PnP device không còn hiện diện bằng SetupAPI/Configuration Manager.
- Hiển thị friendly name, lần kết nối cuối, class, enumerator, service, COM port và
  device instance ID.
- Mặc định bảo vệ `HTREE\\ROOT\\`, `ROOT\\`, `SWD\\` và `SW\\{...}`; có menu bật
  từng nhóm.
- Xóa một hoặc nhiều ghost device sau xác nhận và chỉ khi chạy Administrator.
- Tạo System Restore Point trước batch xóa; nếu thất bại phải xác nhận lần nữa mới
  tiếp tục.
- Giải phóng bit `ComDB` chỉ khi không còn device nào khác dùng cùng `COMx`.
- Liệt kê và unpair Bluetooth device không kết nối trong dialog riêng.
- Mở Device Manager, Disk Management, Event Viewer, Network Adapters và trang
  properties của device.
- Lưu filter, column, sort, window placement và theme vào INI cạnh executable.

## Build

Yêu cầu Windows SDK và Visual Studio Build Tools có workload C++ desktop.

```bat
build.cmd
```

Artifact được tạo tại `dist\Sonic79Reconstructed.exe`. Project cũng có
`CMakeLists.txt` cho IDE hoặc toolchain CMake thông thường.

## Kiểm thử

`build.cmd` chạy hai lớp test:

- `core_tests`: rule bảo vệ device ID và parser COM port.
- `enumeration_smoke`: gọi SetupAPI/ConfigMgr thật nhưng chỉ đọc, không remove.
- GUI smoke mode: tạo main window/ListView/menu ở trạng thái ẩn rồi tự đóng.

Không có test tự động nào gọi uninstall device, sửa `ComDB`, unpair Bluetooth hoặc
tạo restore point.

## Lưu ý an toàn

Không chạy binary rebrand như một bản “device spoofer”; chức năng thật là xóa device
registration khỏi Windows. Hãy xem kỹ Device ID trước khi remove. Các nhóm soft/root
có thể không tự cài lại nên mặc định bị ẩn.

Source này là logic-equivalent theo hành vi, không phải source C++ nguyên bản và
không cố tái tạo cơ chế self-signature, code impersonation/shared utility hoặc pixel
UI/icon của tác giả.
