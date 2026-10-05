# Lắp demo trong Unreal Editor

Đây là demo cá nhân độc lập, target **UE 5.7, C++, Windows single-player**. Code gameplay đã có; bạn tạo map, Blueprint, material và animation bằng Editor. Không cần nối vào dự án Vault. Môi trường chuẩn bị source không có Unreal: UHT/UBT, PIE và packaging vẫn cần xác nhận trên máy bạn. Không có asset `.uasset`/`.umap` giả trong repo.

## 1. Lấy code và build

Checkout `feature/vaultbreaker-demo`. Cài UE 5.7 cùng C++ toolchain của engine. Đóng Editor, chạy từ thư mục repo:

```powershell
powershell -File Scripts/Build-Editor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
```

Hoặc generate project files rồi build target `HideInTheBoxEditor`, Development Editor, Win64. Mở `HideInTheBox.uproject` sau khi build thành công. Nếu dùng source engine, chọn lại Engine Version cho project. Generate project files thành công không đồng nghĩa compile thành công. Giữ log lỗi đầu tiên nếu build bị chặn; không dùng Live Coding để xác nhận thay đổi reflected header.

### Mở bản ZIP và xem lỗi build

Không cần Git trên máy: giải nén toàn bộ ZIP của nhánh demo. File `.sln` không được lưu trong repo; chuột phải `HideInTheBox.uproject` → Generate Visual Studio project files (Windows11 có thể cần Show more options). Mở solution, chọn Development Editor/Win64 và Build project HideInTheBox. Nếu không có menu Generate, có thể dùng Build-Editor.ps1 ở trên để build trực tiếp.

Hai file `Source/HideInTheBox.Target.cs` và `Source/HideInTheBoxEditor.Target.cs` dùng `DefaultBuildSettings = BuildSettingsVersion.V6` theo thông báo upgrade của UE5.7. Bản ZIP cũ có V5: đổi thành V6 ở cả hai file rồi build lại. Không sửa file sinh trong Intermediate hoặc cài thêm engine chỉ để tắt cảnh báo.

`MSB3073 ... exited with code 6` chỉ là thông báo tổng kết. Lấy lỗi gốc tại Visual Studio → View → Output (Ctrl+Alt+O) → Show output from: Build, hoặc log `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`. Gửi toàn bộ Output Build nếu không tìm thấy dòng lỗi. Warning [Upgrade] không đủ để kết luận nguyên nhân build thất bại.

## 2. Chạy bản hình khối trước

1. New Level → Empty Level. Save `/Game/Maps/DemoArena`.
2. Thêm PlayerStart: **X=-400, Y=0, Z=100**. Không thêm pawn Auto Possess khác.
3. World Settings → GameMode Override = **HBGameMode**.
4. Project Settings → Maps & Modes: Editor Startup Map và Game Default Map = DemoArena.
5. Save All → Play → click viewport để lấy focus.

Code tự sinh sàn, tường biên, đèn, Gate, Anima, enemy và HUD lúc Play. Viewport trước Play gần như trống là bình thường; đừng thêm sàn chồng lên sàn runtime. Enter vào Dungeon, LMB enemy để tiến tới đánh, RMB Pulse. Kiểm tra trước khi thay asset.

## 3. Tạo các asset để lắp

Tạo thư mục `/Game/Demo/Blueprints`, `/Game/Demo/Materials`, `/Game/Demo/Animation`, `/Game/Demo/Data`.

| Asset | Loại / parent | Gán ở đâu |
| --- | --- | --- |
| BP_DemoPlayer | Blueprint → HBPlayerCharacter | BP_DemoGameMode → Default Pawn Class |
| BP_DemoEnemy | Blueprint → HBEnemyCharacter | BP_DemoGameMode → Enemy Class |
| BP_DemoController | Blueprint → HBPlayerController | BP_DemoGameMode → Player Controller Class |
| BP_DemoGameMode | Blueprint → HBGameMode | World Settings và Project Settings → Default GameMode |
| DA_DemoLoadout | Miscellaneous → Data Asset → HBLoadoutData | BP_DemoPlayer → Loadout component → Definition |

Giữ HUD Class = **HBHUD**. DA mặc định có sáu ability và sáu slot. Chưa cần đổi số liệu. Nếu không gán Definition, code dùng cấu hình native mặc định. Chọn component Loadout/Feedback trong cây Components để thấy property tương ứng. Compile/Save từng BP.

## 4. Material và ánh sáng: ưu tiên dễ làm, rõ hình

Tạo **M_DemoTint**: Surface, Opaque, Default Lit. Vector Parameter tên chính xác `Color` → Base Color; Constant 0.65 → Roughness. Có thể nối `Color × 0.1` → Emissive Color. Tạo hai Material Instance từ nó:

- **MI_Floor**: Color RGB tuyến tính khoảng (0.025, 0.04, 0.065), nền xanh đen.
- **MI_Wall**: Color (0.06, 0.08, 0.12), sáng hơn nền một chút.

Tạo **M_DemoRing**: Surface, Translucent, Unlit, Two Sided. Vector Parameter `Color` × Scalar Parameter `Glow` (2) → Emissive Color; Scalar Parameter `Opacity` (1) → Opacity. Bật Usage → Used with Instanced Static Meshes nếu engine chưa tự bật. Tên Color/Opacity phải đúng để runtime đổi màu và fade.

| Nơi cấu hình | Property | Asset |
| --- | --- | --- |
| BP_DemoGameMode | Floor Material / Wall Material | MI_Floor / MI_Wall |
| BP_DemoGameMode | Tint Material | M_DemoTint |
| BP_DemoPlayer và BP_DemoEnemy | Appearance → Tint Material | M_DemoTint |
| BP_DemoPlayer → Feedback component | Ring Material | M_DemoRing |
| BP_DemoController | Feedback → Ring Material | M_DemoRing |

Native code đã đặt đèn xanh phía Town, đèn ấm phía Dungeon; core hình cầu, turret hình trụ, cache màu vàng. Vòng destination/hover/target và burst hit có màu riêng. M_DemoTint dành cho hình khối; khi gắn skeletal mesh, giữ material riêng của model.

Có thể thêm Post Process Volume, bật Infinite Extent, Bloom vừa phải (bắt đầu 0.3), giữ exposure ổn định. Project đã tắt auto exposure và motion blur. Dùng cửa sổ 1280×720 trở lên để đọc HUD; thử 1920×1080 khi quay demo. Ưu tiên silhouette, tương phản và animation đúng trước hiệu ứng nặng. Hình thức cuối cùng cần bạn xem trong PIE vì chưa có render review từ engine.

## 5. Gắn mesh và locomotion

1. Import mesh cùng animation bạn có quyền dùng, cùng skeleton tương thích.
2. BP_DemoPlayer → Mesh: gán Skeletal Mesh, chỉnh relative transform để chân trên sàn. Mannequin thường thử Z=-88, yaw=-90; kiểm tra model thực tế.
3. Tạo AnimBP cùng skeleton, Parent Class = **HBAnimInstance** (Class Settings nếu cần).
4. AnimGraph: locomotion Blend Space/State Machine → **Slot DefaultGroup.DefaultSlot** → Output Pose.
5. Parent cung cấp `fSpeed`, `fForwardSpeed`, `fRightSpeed`, `bFalling`, `bAttacking`, `bDodging`. Speed lấy từ velocity thực. Dải đi bộ mặc định 0–450 cm/s. Có thể dùng forward/right cho strafe độc lập hướng aim.
6. Gán Anim Class vào Mesh. Placeholder và mũi chỉ hướng hình khối tự ẩn khi có skeletal mesh.
7. Nếu thêm animation dodge, dùng bDodging trong state machine; gameplay dodge đã có dù chưa gắn animation. Không thêm damage/timer gameplay vào AnimBP.

Demo điều khiển displacement bằng CharacterMovement; đặt Root Motion Mode = **Ignore Root Motion**. Enemy có thể giữ hình khối ở lần demo đầu. Nếu thay enemy mesh, bạn tự tạo AnimBP phù hợp; HBAnimInstance hiện đọc player.

## 6. Montage và hit notify

1. Tạo attack montage cùng skeleton, đòn đánh tại chỗ; slot **DefaultGroup.DefaultSlot**.
2. Đặt notify **HB Attack Hit trực tiếp trên notify track của montage**, đúng frame chạm mục tiêu. Chỉ đặt trong sequence nguồn không đáp ứng kiểm tra asset hiện tại.
3. BP_DemoPlayer → Combat → Attack Montage = montage vừa tạo.
4. Play, F2/LT chuyển State → Events → Config Checks → ẩn. Kiểm tra playback, outcome, hit count và HP enemy.

**Không gán montage**: timed fallback để test hình khối. **Đã gán nhưng playback lỗi**: hủy swing, báo lỗi; không âm thầm gây damage bằng timer. **Montage phát nhưng thiếu notify**: không damage, outcome ghi thiếu hit. Duplicate notify tối đa một hit/swing. Watchdog chỉ mở lock, không gây hit.

Click lệnh mới giữa swing: giữ một buffer, lệnh sau thay lệnh trước; chỉ bắt đầu khi đòn hoàn tất. Interrupt/cancel/death/reset xóa buffer. Damage không gây chết không tự ngắt đòn. Kiểm tra cả complete, interrupted và cancel; không dùng mỗi dòng “assigned” làm bằng chứng montage hoạt động.

## 7. Điều khiển và thay slot

| Chức năng | Keyboard / mouse | Gamepad kiểu Xbox |
| --- | --- | --- |
| Move | LMB sàn (slot 0 đang Basic), WASD | Left stick |
| Aim | Cursor | Right stick; thả giữ hướng cuối |
| Slot 0: Basic / confirm build | LMB | A |
| Slot 1: Pulse | RMB | B |
| Slot 2: Dodge | 1 | X |
| Slot 3: Guard | 2 | Y |
| Slot 4: Mend | 3 | LB |
| Slot 5: Bolt | 4 | RB |
| Đánh tại chỗ | Shift + click enemy | Chưa có utility tương đương |
| Chuyển phase | Enter | D-pad Up |
| Build mode | B | D-pad Down |
| Chọn socket | Tab | D-pad Right |
| Repair Gate | R | D-pad Left |
| Lấy cache gần | E | L3 |
| Cancel / thoát build | Space | R3 |
| Pause / resume | P | Start |
| Restart map | F5 | Back |
| Debug: State → Events → Checks → ẩn | F2 | Left Trigger |

Sáu slot remap được; utility giữ cố định. Console lệnh dùng **index từ 0**:

```text
HBRebind 2 Q false
HBAssign 2 2
HBCameraYaw 90
```

Lệnh đầu chuyển phím slot 2 sang Q và lưu binding. Lệnh hai gán ability Dodge cho slot 2; ability index: 0 Basic, 1 Pulse, 2 Dodge, 3 Guard, 4 Mend, 5 Bolt. HBAssign chỉ đổi runtime; muốn mặc định lâu dài chỉnh Initial Slots trong DA_DemoLoadout. Camera yaw để kiểm tra movement tương đối camera; muốn mặc định chỉnh CameraBoom trong BP.

API Blueprint `RebindSlotKey`, `SaveBindings`, `AssignAbilityToSlot` có sẵn nếu muốn tự làm menu. Không có menu remap hoàn chỉnh trong bản này. Remap từ Blueprint cần gọi SaveBindings để lưu; lệnh console đã gọi sẵn. Duplicate/reserved/analog/wrong-device key bị từ chối. Save binding ở slot `HideInTheBox_Input_v1`; không phải save tiến trình run.

## 8. Vòng chơi và build bản demo

Hạ sáu enemy được **90 stones, Threat 6**. Ba cache tùy chọn ở phía Dungeon, đứng trong 180 cm rồi E/L3: mỗi cache **+30 stones, +3 Threat**, một lần duy nhất. Lấy hết thành **180 stones, Threat 15**. Enter về prep khi bạn muốn; không bắt buộc lấy cache.

B/D-pad Down bật build; Tab/D-pad Right chọn socket; LMB/A xây turret giá 30. Repair Gate giá 20. Enter bắt đầu Defense năm wave; không build/repair khi wave hoặc break đang diễn ra. Mỗi wave tối đa 40 giây, break 3 giây. Gate mất thì enemy chuyển sang Anima; Anima mất kết thúc run. Player chết trong Defense hồi sinh sau 3 giây; chết Dungeon kết thúc run. Sau wave 5 nhận thưởng, về Town.

Save All. Packaging: thêm DemoArena vào map cook; config đã bật Cook All cho demo nhỏ. Package Windows **Development** trước, chạy executable ngoài Editor, thử cả chuột và gamepad. Commit Content assets, map và Config đã chỉnh. Dùng checklist [DEMO_AND_TESTS_VI.md](DEMO_AND_TESTS_VI.md) để xác nhận. PIE pass không thay thế packaged pass.

## 9. Debug trong lúc lắp asset

Không cần thêm component hoặc Blueprint cho debug. Sau khi Play, mở Config Checks bằng F2 ba lần để xem thiếu cấu hình nào; sau khi sửa và vào PIE lại, kiểm tra Events lúc đánh. Dùng `HBDebugCheck` để refresh, `HBDebugDump` để in kết quả vào Output Log. Hướng dẫn các lệnh filter/freeze/reset và cách đọc chuỗi input → damage nằm trong [DEBUG_SYSTEM_VI.md](DEBUG_SYSTEM_VI.md). `HBDebugReset` reload toàn bộ map đã lưu, không reset riêng từng bài.

Trang State đã có FPS hiện tại/trung bình, frame time và biểu đồ tối đa240 mẫu, tự hoạt động không cần lắp thêm. Dùng `HBPerfClear` trước mỗi lượt đo; `HBDebugDump` in kèm performance summary. Đọc giới hạn phép đo HUD và hành vi pause trong hướng dẫn debug trước khi dùng số liệu để so sánh.
