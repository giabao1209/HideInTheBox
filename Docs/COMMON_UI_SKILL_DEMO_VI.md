# Demo skill HUD bằng Common UI

Demo dùng thanh menu skill luôn hiện trong gameplay. `UHBCommonGameplayHUDWidget` kế thừa `UCommonUserWidget`, mỗi ô dùng `UHBSkillButton` kế thừa `UCommonButtonBase`, cùng `UCommonBorder`, `UCommonTextBlock` và progress bar UMG. `AHBPlayerController` tự tạo HUD khi vào game; không cần tạo Widget Blueprint thủ công.

## HUD hiển thị

- Phase, cycle, wave, thời gian wave và số enemy còn sống.
- Magic Stones, Threat, số enemy wave kế tiếp, Gate và Anima.
- HP/MP hiện tại và thanh tiến độ.
- Sáu slot skill lấy trực tiếp từ `UHBLoadoutComponent`: tên ability, nút đang gán, mana cost, cooldown và trạng thái Ready/No Mana.
- Click lên ô skill để dùng; ô tối đi khi cooldown, thiếu mana hoặc nhân vật đang bị chặn. Số giây và thanh hồi chiêu cập nhật mỗi 0,1 giây, thanh đầy khi skill sẵn sàng.
- Kết quả lần dùng skill gần nhất, trạng thái build/run, điều khiển theo bàn phím/chuột hoặc gamepad, thông báo Pause/Game Over.

Luồng: **click CommonButton → `UseSkillFromUI(SlotIndex)` → `ActivateSkillSlot` → `TryActivateSlot` → đọc cooldown/mana thật để cập nhật HUD**. Phím Enhanced Input cũng đi qua `ActivateSkillSlot`. Chỉ gameplay trừ mana và ghi cooldown sau khi skill thực thi thành công; cooldown lưu theo ability, nên hai slot cùng ability dùng chung thời gian chờ.

Click ô skill dùng hướng aim gần nhất của nhân vật; đưa chuột lên ô giữ nguyên hướng aim. Basic/Bolt tìm enemy theo hướng đó; Pulse đánh một điểm phía trước. Với phím/chuột trong thế giới, cách aim cũ vẫn được giữ. Gamepad dùng các phím skill đã map, các ô không lấy focus. Sau khi dùng gamepad, nhấn WASD/phím skill bàn phím để quay lại chế độ click UI.

Canvas HUD cũ không còn vẽ lại các phần thông tin này để tránh hiện hai HUD chồng lên nhau. Các marker thế giới và bảng diagnostics vẫn do `AHBHUD` vẽ. Thanh skill tự thu nhỏ khi viewport hẹp; panel bên phải và thông báo giữa màn hình dùng kích thước dương, đúng quy tắc anchor của Canvas.

## Thử nhanh

1. Vào Play, click **Guard**: MP giảm 15, hiện `5.0s COOLDOWN`, thanh hồi dần về đầy. Click lại trong cooldown: hiện `On cooldown`, không trừ MP thêm.
2. Click **Dodge**: cooldown 2 giây, không tốn mana. Hoặc nhấn phím đang hiển thị trên ô.
3. Aim xuống sàn trong phạm vi rồi dùng **Pulse** qua phím; hoặc aim theo hướng muốn đánh rồi click ô **Pulse**: tốn 20 MP, cooldown 1,5 giây.
4. **Mend** chỉ dùng được khi thiếu HP; ở đầy HP hiện `Health already full`, không trừ MP và không bắt đầu cooldown. Basic/Bolt cần enemy hợp lệ.
5. Pause khi đang cooldown: số giây và mana ngừng chạy; resume tiếp tục. Build mode/Game Over chặn click skill.
6. Console `HBAssign 5 3` gán Guard vào slot 6. Dùng Guard ở một ô: cả hai ô hiển thị cùng cooldown. Lệnh dùng index từ 0.
7. Kiểm tra 720p/1080p: sáu ô nằm ở đáy, panel tài nguyên ở góc phải, không bị cắt hoặc hiện hai HUD.

Giá trị trên là mặc định native; nếu đã gán Loadout Data Asset thì UI dùng giá trị của asset đó.

## Cấu hình editor

CommonUI đã được bật trong `HideInTheBox.uproject`; module `CommonUI`, `UMG` và dependency private `SlateCore` đã được thêm vào `HideInTheBox.Build.cs`. `DefaultEngine.ini` đặt `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient` để Common UI nhận input đúng đường viewport, theo [Epic Common UI Quickstart](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine?application_version=5.7).

Controller trên nhánh này giữ Enhanced Input mappings native và API remap hiện có. Các phím mặc định là LMB/RMB và 1–4; gamepad A/B/X/Y/LB/RB. Không cần tạo IMC hoặc Input Action assets mới cho demo Common UI.

Trong GameMode của map demo, dùng `AHBPlayerController` hoặc Blueprint kế thừa class này và `AHBPlayerCharacter` hoặc Blueprint nhân vật hiện có. HUD tự được tạo trong BeginPlay.

## Build và chạy

Vì đây là thay đổi C++ có class UCLASS mới và dependency module mới, đóng Unreal Editor, build project bằng `Scripts/Build-Editor.ps1`, rồi mở lại project. Chạy `DemoArena` với `BP_DemoGameMode`. Nếu phase/nhân vật hoạt động nhưng HUD không xuất hiện, kiểm tra Player Controller Class và CommonUI plugin.

```powershell
powershell -File Scripts/Build-Editor.ps1 -EngineRoot 'C:\Lambro\UnrealEngine'
```

Lệnh trên dùng engine source đang mở trên máy. Nếu mở project bằng bản Epic Launcher, dùng `C:\Program Files\Epic Games\UE_5.7` thay cho EngineRoot. Lưu và đóng Editor trước khi build project thật; class native mới cần UHT/build và mở lại Editor.

Đây là demo skill bar trong game. Khi muốn thêm menu chọn/gán skill có focus và nút Back, có thể thêm `UCommonActivatableWidget` trên lớp menu riêng; đọc [Epic CommonUI Input Technical Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/commonui-input-technical-guide-for-unreal-engine?application_version=5.7) để cấu hình input cho lớp đó.

Build và test `HideInTheBox.Skills.CommonUIButton` đã PASS trên bản kiểm tra độc lập với UE 5.7.1. Test gọi callback CommonButton thật, kiểm tra mana/cooldown và hai slot chung ability; không mô phỏng click chuột vật lý. Ảnh runtime được lưu tại `Saved/SkillUIDemo-720p.png` và `Saved/SkillUIDemo-1080p.png`. Bản kiểm tra dùng map Engine Entry và các class native; Blueprint/map/art hiện có không nằm trong commit demo này.

Kết quả chi tiết nằm trong `VALIDATION.md`. Cần build lại project chính bằng engine đang dùng, rồi kiểm tra PIE với Blueprint/map thật và gamepad vật lý. Phần refactor sang IMC asset và các sửa đổi animation/asset khác vẫn là thay đổi local riêng, chưa nằm trong commit này.
