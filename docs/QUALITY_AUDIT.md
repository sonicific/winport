# Audit chất lượng và an toàn

Ngày audit: 2026-09-14. Phạm vi gồm toàn bộ source clean-room, build MSVC/CMake,
test runtime chỉ đọc và script tham khảo
[`Device-Cleanup.ps1`](https://github.com/insovs/Device-Cleanup).

## Kết luận

Các luồng đọc và UI chính đã được build, static-analyze và smoke-test thành công.
Luồng xóa đã được harden đáng kể, nhưng không thể gọi một phần mềm quản trị thiết bị
là “hoàn hảo” nếu chưa chạy destructive integration test trong nhiều Windows VM và
trên nhiều loại phần cứng. Vì an toàn dữ liệu, audit này không uninstall thiết bị,
không sửa `ComDB`, không unpair Bluetooth và không tạo Restore Point trên máy thật.

## Lỗi/rủi ro đã sửa

| Hạng mục | Trước audit | Sau audit |
| --- | --- | --- |
| Race giữa scan và remove | Dùng kết quả scan cũ | Kiểm tra lại `CM_Get_DevNode_Status` ngay trước từng uninstall; device đang hiện diện hoặc không xác minh được sẽ bị skip |
| CPU/interrupt affinity | Không nhận diện | Đọc `DevicePolicy` và `AssignmentSetOverride`; hiển thị ở cột Safety và chặn mặc định |
| Protected device | Chỉ ẩn ROOT/SWD/SW ở filter | Guard nằm trong removal engine, không thể bypass chỉ bằng cách bật cột/filter; override chỉ có hiệu lực trong phiên và cần hai lần xác nhận |
| Fallback removal | CM rồi SetupAPI trong một số lỗi | CM → SetupAPI `DIF_REMOVE` → `pnputil /remove-device`, sau đó poll xác nhận device đã biến mất |
| Fallback nguy hiểm | Chưa đánh giá đầy đủ | Cấm xóa `HKLM\\...\\Enum` trực tiếp và không dùng `SetupDiRemoveDevice` sai vai trò |
| UI bị đứng khi scan | Scan đồng bộ trên UI thread | `std::async` + timer polling, gom refresh phát sinh khi scan đang chạy |
| Device tree stale | Refresh list ngay sau batch | Yêu cầu re-enumeration đồng bộ một lần sau batch, rồi scan lại |
| INI trong thư mục chỉ đọc | Chỉ ghi cạnh EXE | Fallback sang `%ProgramData%\\Sonic79Reconstructed`; nếu có hai file thì đọc bản mới hơn |
| Bluetooth scope | Có thể liệt kê thiết bị `Unknown` chưa pair | Chỉ trả remembered/authenticated, disconnected device; bổ sung error propagation |
| Modal Bluetooth | Có thể nuốt `WM_QUIT` | Re-post `WM_QUIT` về main loop; nút Remove bám theo selection |
| Build manifest | CMake sinh trùng manifest | Manifest trở thành source riêng; cả `build.cmd` và CMake đều embed đúng resource `#1` |

## Đối chiếu script tham khảo

Điểm hữu ích đã tiếp thu là phát hiện interrupt affinity và tách rõ nhóm protected.
Không copy source/XAML; implementation mới dùng Win32 C++ độc lập.

Một số tuyên bố trong README của repo tham khảo không khớp code hiện tại:

- Repo mô tả 5 fallback. Worker thực tế chỉ gọi `Remove-PnpDevice`, `pnputil` và
  `reg.exe delete`; class C# `SetupApiRemove` được compile nhưng không được gọi, và
  không có lời gọi `devcon`.
- Ghost được chọn bằng `Status -eq 'Unknown'`. Điều này rộng hơn định nghĩa phantom
  của SetupAPI. Bản C++ dùng hai tín hiệu được Windows tài liệu hóa: `CR_NO_SUCH_DEVNODE`
  hoặc `CM_PROB_PHANTOM`.
- Script có thể đưa cả device đang hiện diện nhưng có affinity vào danh sách protected
  rồi cho người dùng tick để xóa. Bản C++ chỉ quản lý device đã xác nhận non-present.
- Script xóa song song tối đa 12 device và có fallback xóa trực tiếp registry Enum.
  Bản C++ xử lý tuần tự để tránh tranh chấp PnP và không bao giờ xóa raw registry key.

## Kết quả kiểm thử trên host audit

- MSVC `/W4 /permissive-`: pass.
- MSVC `/analyze /WX`: pass (chỉ suppress hai warning sai từ header SDK).
- `core_tests`: pass.
- SetupAPI enumeration smoke: 177–178 phantom device tùy thời điểm; 6 mục được bảo vệ
  do affinity; tất cả đều thỏa điều kiện phantom.
- Present-device guard smoke: pass trên một PnP device đang hoạt động; không có thao
  tác ghi/uninstall.
- Bluetooth smoke: pass; host hiện có 0 remembered device bị disconnect.
- GUI smoke: pass; async scan hoàn tất, số row/cột đúng, menu safety tồn tại.
- CMake 4.2.3 + NMake: configure/build pass; CTest pass.
- Manifest đã trích ngược từ EXE và xác nhận `asInvoker`, Common Controls v6,
  PerMonitorV2 DPI và long-path awareness.

Số ghost có thể đổi giữa hai lần test do Windows tự cập nhật cây thiết bị; đây không
phải test không ổn định miễn mọi record vẫn vượt qua phantom invariant.

## Những gì chưa thể xác nhận trên máy này

- Kết quả uninstall thật của từng loại USB/HID/storage/network/virtual device.
- Hành vi khi driver veto removal, khi `pnputil` timeout, hoặc khi Windows yêu cầu reboot.
- Khả năng phục hồi thực tế của Restore Point khi System Protection bị tắt/bị policy chặn.
- Việc giải phóng và tái cấp COM number sau reboot.
- Unpair Bluetooth với nhiều radio/vendor stack.
- Chữ ký phát hành: artifact hiện là build local `NotSigned`.

Để release production, bước còn thiếu có giá trị nhất là một ma trận VM có snapshot
(Windows 10/11, user/admin, System Restore on/off), USB/COM/Bluetooth disposable,
fault injection cho access denied/veto/timeout, rồi ký Authenticode bằng certificate
của publisher.

## Tính năng nên cân nhắc tiếp

Không cần thêm fallback phá registry. Các bổ sung hợp lý cho phiên bản sau là:

1. Export preview/report (CSV hoặc JSON) trước khi xóa và audit log sau batch.
2. Bộ rule allow/deny theo class/enumerator/vendor cho môi trường doanh nghiệp.
3. CLI `--scan`/`--dry-run` để automation có output machine-readable.
4. Bộ localization, accessibility/keyboard-navigation test và ký số CI release.

Các mục này tăng khả năng vận hành/audit; chúng không nên làm yếu các guard hiện tại.
