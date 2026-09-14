# Báo cáo reverse engineering

Ngày kiểm tra: 2026-09-13. Phân tích thực hiện tĩnh; không chạy hai binary Sonic và
không gọi thao tác remove nào trên máy.

## Kết luận

`Sonic_79.exe` và `Sonic_79_original.exe` là hai bản resource-rebrand của
`DeviceCleanup.exe` 1.5.1 x64. Tên `original` trong file người dùng cung cấp **không
có nghĩa là binary chính thức**: nó vẫn có title/version/icon Sonic và không còn chữ
ký hợp lệ.

Khẳng định này không chỉ dựa vào string/PDB. Năm section chứa code và dữ liệu thực thi
quan trọng có SHA-256 giống tuyệt đối giữa cả ba file:

| Section | SHA-256 |
| --- | --- |
| `.text` | `ef87a073657e84e2b4b18b860af39cd37118a6a4173bb10cd6b4a7637bf7f594` |
| `.rdata` | `5ed0a8af25f7e286c8658bc888c430f347f70aa482c177b05341f1ed66120ced` |
| `.data` | `27f30659a282a697cc7bcc53c97f8623aa24a2fbcdc954d065529cbb5c48af50` |
| `.pdata` | `ddb263e84012753001ea039a9863b8e364887a093b835a7018b56f03a78c6958` |
| `.reloc` (raw data) | `276366de6a5c3cde5979c67bb419e240df9f9e6a23a3edde8d33b873cecbce40` |

## Mẫu và provenance

| File | Size | SHA-256 | Authenticode |
| --- | ---: | --- | --- |
| `Sonic_79.exe` | 136,960 | `cc318e8c13695c84dc7b88459fc4ed89c3d205b00c85a6502a4154af14fe55ef` | No signature found |
| `Sonic_79_original.exe` | 136,960 | `7ef4f187d6785cc4cd5cec72d70137a1bf5895f336bc9e302c2aa0a141053da0` | No signature found |
| Official `DeviceCleanup.exe` | 104,192 | `51be83b9d14dd68e0293569c58159addefdccb110115e04725fa55813808e862` | Uwe Sieber self-signed certificate; trusted timestamp; untrusted publisher root |

Binary chính thức được tải trực tiếp từ
`https://www.uwe-sieber.de/files/DeviceCleanup_x64.zip`; SHA-256 của ZIP tại thời
điểm phân tích là
`c59f6cb9d8cc0942a692481701bd0588d7ac5ebee476093119b56982f8ef1734`.

PE timestamp của code là `2025-09-08 16:28:50 UTC`. PDB path còn nguyên:

```text
u:\1Source\VC\DeviceCleanupGui1510\Release_x64\DeviceCleanup.pdb
```

## Binary diff

Official resource section có raw size `0x4400`, trong khi hai bản Sonic là `0xC400`
(tăng đúng `0x8000`). Vì section này lớn lên, `.reloc` chuyển từ RVA `0x24000` sang
`0x2C000` và raw offset từ `0x17600` sang `0x1F600`.

Resource thực sự thay đổi so với official:

- Dialog chính: caption `Device Cleanup Tool` thành `Sonic 79 Tool`.
- About dialog: title/branding thay đổi. `Sonic_79.exe` còn thay attribution Uwe
  Sieber bằng `SONIC GROUP 2026`; file có hậu tố `_original` giữ attribution Uwe ở
  About nhưng vẫn là resource Sonic.
- Version info: `ProductName`, `InternalName`, `OriginalFilename` và description.
- Toàn bộ icon group được thay; icon Sonic lớn hơn đáng kể.

Menu, accelerators, Bluetooth dialog, bốn bitmap nhỏ và manifest có payload giống
official. `tools/pe_resource_diff.py` tái tạo phép so sánh này.

## Vì sao chữ ký hỏng

Official PE đặt Security Directory tại file offset `0x17C00`, size `0x1B00`, đúng vị
trí certificate table ở cuối file. Sau khi resource Sonic phình thêm `0x8000`, blob
certificate được dời vật lý tới `0x1FC00`, nhưng trường Security Directory vẫn giữ
offset cũ `0x17C00`.

SHA-256 của `0x1B00` byte cuối cả ba file đều là:

```text
0fdd48bd91fc0a00a3ba6cd69f8fb803f7070624ae1a0c43183ff2a5503d1981
```

Nghĩa là certificate blob cũ vẫn được copy theo file, nhưng PE loader/WinVerifyTrust
không còn trỏ vào nó; hơn nữa Authenticode digest cũng đã đổi bởi resource edits.
`signtool verify /pa` trả `No signature found` cho cả hai bản Sonic.

## Integrity path đã xác minh

- Entry point: `0x1400038B8`.
- Startup gọi routine tại `0x14000C2DC` từ `0x140008342` với đường dẫn executable.
- Routine dựng `WINTRUST_FILE_INFO` và `WINTRUST_DATA`, gọi `WinVerifyTrust`.
- Startup chỉ chấp nhận `0` hoặc `0x800B0109` (`CERT_E_UNTRUSTEDROOT`). Các kết quả
  khác đi vào message `Certificate check fail, EXE was manipulated` rồi thoát.

Điều này giải thích certificate self-signed của bản official: signature/digest vẫn
hợp lệ nhưng publisher root không nằm trong trust store. Hai bản Sonic không sửa
`.text`, nên cũng không hề bypass check; theo code path chúng sẽ bị từ chối lúc mở.

## Capability map

| Capability | Bằng chứng | Mức tin cậy |
| --- | --- | --- |
| Enumerate mọi PnP node rồi lọc ghost | `SetupDiGetClassDevsW`, `SetupDiEnumDeviceInfo`, `CM_Get_DevNode_Status`, official manual | Xác minh |
| Friendly name/class/enumerator/service | `CM_Get_DevNode_Registry_PropertyW`, column strings | Xác minh |
| Last connected | `CM_Get_DevNode_PropertyW` dynamic lookup, registry paths, UI string | Xác minh hành vi; fallback chi tiết được clean-room hóa |
| Uninstall device | `CM_Uninstall_DevNode`, SetupAPI `DIF_REMOVE` calls, error strings | Xác minh |
| Bảo vệ ROOT/SWD/SW | prefix strings, menu/INI, official manual | Xác minh |
| COM reservation cleanup | `COM Name Arbiter`, `ComDB`, `PortName`, official manual | Xác minh |
| Bluetooth dialog/unpair | toàn bộ Bluetooth enumeration APIs và `BluetoothRemoveDevice` | Xác minh |
| System Restore | `SRSetRestorePointW`, frequency registry value, official manual | Xác minh |
| UAC restart | `ShellExecuteW`, `runas`, menu text | Xác minh |
| Network/C2 | Không có WinHTTP/WinINet/Winsock import hay endpoint lạ | Không thấy bằng chứng |

Nhóm process/token APIs (`CreateToolhelp32Snapshot`, `DuplicateTokenEx`, privilege
names, `services.exe`) tồn tại trong code nhưng chưa đủ bằng chứng để kết luận tất cả
đều chạy trong normal cleanup flow; rất có thể đây là shared utility/elevation code.
Bản clean-room không tái tạo impersonation/SYSTEM-token path vì không cần cho chức
năng người dùng đã xác minh.

## Mapping sang source clean-room

| Hành vi reference | Module mới |
| --- | --- |
| SetupAPI/ConfigMgr enumeration và removal | `src/device_manager.*` |
| ROOT/SWD/SW guard, COM parser | `src/device_rules.*` |
| `ComDB` ownership check và clear bit | `src/com_port_manager.*` |
| Bluetooth enumeration/unpair | `src/bluetooth_manager.*`, `src/bluetooth_dialog.*` |
| `SRSetRestorePointW` và frequency override | `src/system_restore.*` |
| INI schema/window/column settings | `src/settings.*` |
| ListView, menus, confirmation, tools | `src/app_window.*` |

Đây là functional reconstruction. Tên class/variable, layout nội bộ, thuật toán
fallback timestamp và error mapping không được tuyên bố là source nguyên bản.
