# Sonic79 Reconstructed

Đây là bản C++/Win32 **clean-room** được viết lại từ hành vi đã quan sát của hai
binary `Sonic_79*.exe` và tài liệu công khai của Device Cleanup Tool 1.5.1.
Project không chứa source, icon, certificate hoặc resource sao chép từ binary gốc.

Kết luận reverse quan trọng: cả hai binary Sonic có toàn bộ section code/data chính
giống từng byte với `DeviceCleanup.exe` 1.5.1 x64 chính thức. Chúng chỉ thay resource,
làm hỏng vị trí Authenticode và do đó không còn chữ ký hợp lệ. Chi tiết và hash kiểm
chứng nằm trong [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md).

## Chức năng đã dựng lại

- Liệt kê PnP device không còn hiện diện bằng SetupAPI/Configuration Manager; việc
  quét chạy nền nên cửa sổ không bị treo.
- Hiển thị friendly name, lần kết nối cuối, class, enumerator, service, COM port và
  device instance ID, kèm cột `Safety`.
- Mặc định bảo vệ `HTREE\\ROOT\\`, `ROOT\\`, `SWD\\` và `SW\\{...}`; có menu bật
  từng nhóm.
- Phát hiện cấu hình interrupt/CPU affinity trong `Affinity Policy` và bảo vệ device
  đó. Override phải được bật rõ ràng, chỉ tồn tại trong phiên và có hai lớp xác nhận.
- Xóa một hoặc nhiều ghost device sau xác nhận và chỉ khi chạy Administrator.
- Ngay trước từng lần xóa, app kiểm tra lại device vẫn là phantom để tránh xóa nhầm
  thiết bị vừa được cắm lại. Fallback theo thứ tự `CM_Uninstall_DevNode`, SetupAPI
  `DIF_REMOVE`, rồi `pnputil /remove-device`; không xóa thẳng registry.
- Tạo System Restore Point trước batch xóa; nếu thất bại phải xác nhận lần nữa mới
  tiếp tục.
- Giải phóng bit `ComDB` chỉ khi không còn device nào khác dùng cùng `COMx`.
- Liệt kê và unpair Bluetooth device đã nhớ/đã xác thực nhưng không kết nối trong
  dialog riêng; không đưa thiết bị Bluetooth lạ chưa ghép đôi vào danh sách.
- Mở Device Manager, Disk Management, Event Viewer, Network Adapters và trang
  properties của device.
- Lưu filter, column, sort, window placement và theme vào INI cạnh executable; tự
  fallback sang `%ProgramData%\\Sonic79Reconstructed` khi thư mục cài đặt chỉ đọc.

Báo cáo hardening và đối chiếu script tham khảo nằm trong
[docs/QUALITY_AUDIT.md](docs/QUALITY_AUDIT.md).

## Build

Yêu cầu Windows SDK và Visual Studio Build Tools có workload C++ desktop.

```bat
build.cmd
```

Kiểm tra tĩnh riêng (MSVC `/analyze`, warning được coi là lỗi):

```bat
analyze.cmd
```

Artifact được tạo tại `dist\Sonic79Reconstructed.exe`. Project cũng có
`CMakeLists.txt` cho IDE hoặc toolchain CMake thông thường.

## Kiểm thử

`build.cmd` chạy các lớp test sau:

- `core_tests`: rule bảo vệ, parser COM port và quote command line.
- `enumeration_smoke`: gọi SetupAPI/ConfigMgr thật, xác nhận mọi kết quả là phantom,
  và chứng minh removal guard từ chối một device đang hiện diện.
- `bluetooth_smoke`: gọi API Bluetooth thật ở chế độ chỉ đọc và kiểm tra dữ liệu lặp.
- GUI smoke mode: quét nền, tạo main window/ListView/menu, xác nhận số row/cột rồi
  tự đóng.

`CMakeLists.txt` cũng đăng ký bốn test tương ứng với CTest.

Không có test tự động nào gọi uninstall device, sửa `ComDB`, unpair Bluetooth hoặc
tạo restore point. Đây là chủ ý: bốn đường ghi hệ thống này chỉ nên được thử trong
Windows VM có snapshot và phần cứng disposable.

## Lưu ý an toàn

Không chạy binary rebrand như một bản “device spoofer”; chức năng thật là xóa device
registration khỏi Windows. Hãy xem kỹ Device ID và cột Safety trước khi remove. Các
nhóm soft/root có thể không tự cài lại nên mặc định bị ẩn và vẫn bị chặn nếu bật hiển
thị. Binary build cục bộ hiện chưa được ký số.

Source này là logic-equivalent theo hành vi, không phải source C++ nguyên bản và
không cố tái tạo cơ chế self-signature, code impersonation/shared utility hoặc pixel
UI/icon của tác giả.
