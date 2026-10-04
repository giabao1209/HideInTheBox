# Hide In The Box — Vaultbreaker prototype

Prototype Unreal Engine **5.7 / C++ / single-player**, dựng độc lập từ GDD Vaultbreaker và yêu cầu demo Character, PlayerController, input, animation và player feedback.

**Trạng thái bàn giao: source prototype, chưa build bằng Unreal, chưa chạy PIE/Standalone/packaged.** Môi trường tạo code không có Unreal Engine. Chưa có `.umap`, `.uasset`, model, AnimBP hoặc montage. Ngày mai cần làm checklist Editor trước khi coi đây là game chạy được.

## Bắt đầu ngày mai

1. Clone repo và checkout nhánh chứa prototype.
2. Xem [Editor setup](Docs/EDITOR_SETUP_VI.md): build Editor target, tạo một Empty Level, thêm PlayerStart, save thành `/Game/Maps/DemoArena`.
3. Chọn `HBGameMode`, đặt startup maps, nhấn Play. Native code tạo sàn, ánh sáng, Gate/Anima, enemies và HUD.
4. Thử gameplay với hình khối trước. Sau đó gắn mesh → AnimBP → montage → `HB Attack Hit` notify.
5. Chạy [kịch bản demo và test](Docs/DEMO_AND_TESTS_VI.md). Không đổi trạng thái “Not run” khi chưa thực sự chạy.

## Phần đã viết

- Click sàn để move; click enemy để tiếp cận và đánh một đòn; Shift+click enemy chỉ đánh nếu trong tầm.
- WASD làm đường đối chiếu direct movement. Gamepad: left stick move, right stick aim, A chọn enemy gần nhất trong hướng aim rồi tiếp cận.
- Enhanced Input cho action và stick, radial dead zone 0.2, clamp đường chéo. WASD là đường polling phụ phục vụ nghiên cứu.
- Một command có hiệu lực và một buffered command. Buffered click thay thế click trước; analog không ghi vào buffer.
- Vòng đích, vòng hover và vòng attack bằng instanced static mesh, không collision, không dùng debug drawing. Hover cùng target chỉ còn vòng target đậm.
- Melee có animation lock, một hit tối đa mỗi swing, fallback timed attack khi chưa có montage chạy được. Montage hợp lệ dùng notify để gây damage.
- `UHBAnimInstance` cung cấp tốc độ từ velocity, forward/right speed, falling và attacking. Chưa có animation assets.
- Town → Dungeon → Return/prep → Defense → Town. Một arena tái sử dụng; sáu enemy dungeon; năm wave defense; Gate rơi thì enemy chuyển sang Anima.
- Dungeon kill cho Magic Stones và Threat; Threat tăng số enemy defense. Turret ở bốn socket cố định, repair Gate, không xây/sửa cả trong break giữa wave.
- Pulse tốn MP, turret giữ target đến khi chết/ra tầm, player respawn sau ba giây trong defense. Chết solo dungeon hoặc mất Anima kết thúc run.
- HUD trạng thái, pause, restart, cleanup timer/delegate/ring và reset input khi flush/disconnect.

## Điều khiển prototype

| Chức năng | Chuột / bàn phím | Gamepad kiểu Xbox |
| --- | --- | --- |
| Di chuyển | Click sàn; WASD phụ | Left stick |
| Aim | Chọn enemy bằng chuột | Right stick |
| Đánh / xác nhận build | LMB | A |
| Đánh tại chỗ | Shift + LMB enemy | Chưa có binding |
| Pulse | RMB | X |
| Sang phase | Enter | Y |
| Bật/tắt build | B | LB |
| Chọn socket tiếp | Tab | D-pad Right |
| Repair Gate | R | RB |
| Cancel / thoát build | Space | B |
| Pause / resume | P | Start |
| Restart level | F5 | Back |

Đây là binding demo cố định, chưa phải hệ sáu slot remappable của GDD. Pause không phải menu UI hoàn chỉnh. Dùng gamepad trên Windows không chứng minh console support.

## Đọc code theo đường demo

| Câu hỏi | File |
| --- | --- |
| Input đi đâu? | `HBPlayerController.cpp` |
| Ai sở hữu command/buffer/attack lock? | `HBPlayerCharacter.cpp` |
| Vì sao indicator không hiện ngay khi click buffer? | `UpdateFeedback()` và `HBIndicator.cpp` |
| Animation lấy tốc độ ở đâu; hit đi qua gì? | `HBAnimation.cpp`, `StartAttack()`, `DeliverAnimationHit()` |
| Phase, stones, threat, wave ở đâu? | `HBGameMode.cpp` |
| Enemy chọn Gate/Anima thế nào? | `HBEnemyCharacter.cpp` |
| Turret và repair? | `HBStructure.cpp` |

Đọc [phạm vi và quyết định thiết kế](Docs/SCOPE_AND_ARCHITECTURE_VI.md) trước khi port về Vault. Không có source `AVaultCharacter` thật trong repo này; tên lớp `HB*` là implementation độc lập.

## Kiểm tra

```powershell
python Scripts/validate_source.py
powershell -File Scripts/Build-Editor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
```

Lệnh Python chỉ kiểm tra cấu trúc source/config/docs; **không thay thế UHT/UBT**. Sau khi build, Unreal Automation có nhóm `HideInTheBox.Rules` để kiểm tra dead zone và wave scaling. Các kiểm tra gameplay/Editor nằm trong test checklist.

## Giới hạn cố ý

Không có GAS, navigation/pathfinding, procedural dungeon, save/load, inventory, gear, race/stat progression, companion, boss, barricade, sáu slot/rebinding hoặc đồ họa anime hoàn chỉnh. Điều khiển click và enemy dùng direct steering trong arena trống; không tìm đường vòng. Structure là objective, chưa phải vật cản. Tuning hiện tại là số demo; Threat chưa có lựa chọn greed tùy ý. Không có license mới áp lên tài liệu thiết kế hoặc asset của bên thứ ba.
