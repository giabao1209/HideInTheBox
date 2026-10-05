# Hide In The Box — personal Unreal demo

Demo cá nhân **UE 5.7 / C++ / single-player**, dựa trên vòng chơi GDD và bài học Character, Controller, Input, Animation, Feedback. Không phụ thuộc dự án Vault hay asset công ty.

**Source đang được chuẩn bị; thay đổi map-authored này cần build lại trên UE 5.7 và chưa được xác nhận UHT/UBT/PIE.** Repo không chứa Unreal Engine hoặc `.umap`/`.uasset`; bạn sẽ tạo map, mesh layout và asset trong Editor.

## Bắt đầu

1. Checkout `feature/vaultbreaker-demo`.
2. Làm [hướng dẫn Editor](Docs/EDITOR_SETUP_VI.md): build, dựng map thật, gắn mesh/camera trong Blueprint và nối AnimBP/montage.
3. Chạy [checklist demo](Docs/DEMO_AND_TESTS_VI.md), gồm packaged build và gamepad.

## Phần code

- Enhanced Input: click move/attack, WASD và analog movement tương đối camera, aim độc lập; cleanup/flush/pause/disconnect.
- Sáu slot Basic, Pulse, Dodge, Guard, Mend, Bolt; gán lại ability, remap phím và lưu binding. Cooldown theo ability tránh đổi slot để bỏ cooldown.
- Một command và một buffer; melee montage/notify, phân biệt complete/interrupted/cancel; timed fallback chỉ khi không gán montage.
- HP/MP, dodge miễn MP với cửa sổ miễn damage, guard, heal; nonlethal damage không tự ngắt swing.
- Town → Dungeon → Return/prep → Defense năm wave → Town; Gate → Anima, turret, repair, respawn Defense.
- Ba cache greed tùy chọn: thêm tài nguyên đổi lấy Threat và enemy đông hơn.
- Level-authored actors: map đặt sàn/tường/đèn/objectives/cache/socket/spawn anchors. C++ đọc anchor theo Actor Tags; không sinh geometry/layout trong lúc chạy.
- Debug ba trang với FPS/frame time và biểu đồ240 mẫu; history 128 sự kiện có filter/freeze/dump và kiểm tra cấu hình; xem [hướng dẫn debug](Docs/DEBUG_SYSTEM_VI.md).
- HUD sáu slot, diagnostics runtime, vòng destination/hover/target, hit burst fade, đèn hai vùng và material hooks.

Blueprints là lớp lắp ráp asset và tham số, không phải nơi viết lại gameplay loop: BP_DemoPlayer gán Skeletal Mesh, camera có thể chỉnh qua component CameraBoom/Camera kế thừa, BP_Gate/BP_Anima/BP_Turret gán mesh, BP_DemoGameMode chọn class. AnimBP có graph locomotion → Slot → Output Pose; Attack Montage chứa HB Attack Hit notify. Controls và map tags nằm trong hướng dẫn Editor. Gamepad trên Windows chưa chứng minh console support.

## Kiểm tra

```powershell
python Scripts/validate_source.py
python Scripts/test_rules.py
powershell -File Scripts/Build-Editor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
```

Native rules test cần compiler C++17 (`g++` hoặc `CXX`), kiểm tra header logic thực; không thay thế UHT/UBT. [Validation record](Docs/VALIDATION.md) phân biệt phần đã chạy và chưa chạy.

[Phạm vi/kiến trúc](Docs/SCOPE_AND_ARCHITECTURE_VI.md) ghi rõ phần GDD chưa triển khai: procedural dungeon, inventory/gear/progression, companion/boss, navigation, GAS, multiplayer, save run, console certification và art hoàn chỉnh. Đây là lát cắt demo, không phải toàn bộ game trong GDD.
