# Kịch bản demo và bằng chứng kiểm tra

## Demo khoảng 5–8 phút

1. **Điều khiển và command (1 phút):** chỉ ra PlayerController nhận input; click hai đích liên tiếp; vòng cập nhật; đến nơi vòng tắt. WASD/stick thay click movement. Cho xem tốc độ analog trên HUD.
2. **Target feedback (1 phút):** hover enemy A, click A ngoài tầm, hover B trong lúc tiếp cận. A có vòng đậm, B có vòng mảnh; hover lại A không có hai vòng chồng. Click sàn thay attack.
3. **Animation/buffer (1 phút):** xem AnimBP velocity, montage Slot và HB Attack Hit; hit counter tăng tại notify. Click sàn trong swing: buffer có nhưng move ring chưa hiện. Đòn kết thúc thì command move mới chạy.
4. **Game loop (2–4 phút):** clear dungeon, stones/threat tăng, Return xây turret, sang Defense. Chỉ ra build bị khóa, enemy đánh Gate rồi Anima, defense kill không cho loot. Có thể tua phần giải thích bằng recording sau khi test đủ năm wave; không khai đã chạy hết khi chỉ xem wave đầu.
5. **Recovery (1 phút):** pause khi đang di chuyển, neutral/resume, đổi thiết bị. Nêu phần chưa test: console, packaged hoặc device khác.

Trước buổi demo, chạy test thật và ghi commit. Nếu montage chưa được ráp, giới thiệu đúng là timed-attack prototype; không dùng hình khối để tuyên bố animation đã xong.

## Acceptance matrix

Mọi dòng dưới đây khởi tạo **Not run**. Lập kết quả riêng cho PIE, Standalone và Windows packaged; pad ghi model + wired/Bluetooth + remapping layer.

| ID | Thao tác | Expected | Status |
| --- | --- | --- | --- |
| B01 | Generate project và build Editor target UE5.7 | UHT/UBT hoàn tất, không lỗi | Not run |
| W01 | Empty map + PlayerStart + HBGameMode, Play | Arena/HUD/player spawn đúng, không pawn thừa | Not run |
| M01 | Click sàn A rồi B khi đang đi | Một vòng ở B, biến mất khi tới ngưỡng | Not run |
| M02 | Click miss ngoài sàn | Không tạo/sửa command hiện tại | Not run |
| M03 | Đi bằng keyboard/pad từ click move | Không cộng hai movement owner, vòng cũ tắt | Not run |
| M04 | Stick neutral, half/full, diagonal | Dead zone không drift, analog phân biệt, magnitude <=1 | Not run |
| M05 | Đi vào tường | Không xuyên biên; command bị kẹt kết thúc sau timeout | Not run |
| T01 | Hover A, click A, hover B, hover A | Target/hover tách đúng, không double-ring cùng actor | Not run |
| T02 | Enemy chết trong chase hoặc swing | Không crash, vòng biến mất, không hit target cũ | Not run |
| A01 | Timed fallback attack | Một hit trong range, khóa rồi mở movement | Not run |
| A02 | Montage + notify trực tiếp montage | Hit đúng notify, <=1 hit/swing | Not run |
| A03 | Montage playback hợp lệ nhưng bỏ notify | Không timed damage thay thế; cuối montage mở lock | Not run |
| A04 | Attack rồi click move A rồi B | Chỉ B được buffer; vòng move chỉ hiện sau unlock | Not run |
| A05 | Interrupt/cancel/unpossess khi attack | Timer/delegate/buffer/ring cleanup, không late hit | Not run |
| A06 | Shift+click enemy ngoài tầm | Không chase; không hit | Not run |
| L01 | Idle/half/full move/dừng/đâm tường | AnimBP đọc velocity thật | Not run |
| G01 | Clear 6 dungeon enemy rồi Return | 90 stones, Threat 6, HP/MP hồi | Not run |
| G02 | Xây socket trống/đã có/không đủ tiền | Trừ đúng 30 khi thành công; fail không trừ | Not run |
| G03 | Build click / analog khi build | Không tạo move/attack ring; chỉ confirm socket | Not run |
| G04 | Repair Gate full/damaged/broken | Chỉ khi damaged + đủ 20; phase hợp lệ | Not run |
| G05 | Năm wave gồm break, thử build/repair | Khóa liên tục; timer hết clear wave không cấp kill reward | Not run |
| G06 | Gate chết rồi Anima chết | Target đổi; Anima chết game over, clear timer/enemy | Not run |
| G07 | Player chết solo dungeon/defense | Game over / respawn 3s; không respawn sau game over | Not run |
| G08 | Hết defense, sang cycle 2 | Reward cuối phase; Gate damage và turret giữ lại | Not run |
| D01 | Pause, giữ input, resume, thả rồi nhấn lại | Không replay command/attack cũ; neutral gate hoạt động | Not run |
| D02 | Alt-tab đang move/attack rồi quay lại | Reset khi focus flush, không input kẹt; ghi OS behavior | Not run |
| D03 | Rút pad khi đang move rồi reconnect | Pause/reset; không stale input; resume bằng P/Start | Not run |
| D04 | Chỉ dùng pad hết vòng gồm build/phase/pause | Không cần chuột; target chọn theo contract | Not run |
| D05 | Stop PIE/restart nhiều lần | Không actor vòng/timer/delegate bị bỏ lại | Not run |
| P01 | Package Windows Development, chạy ngoài Editor | Map/cooked assets/input/animation hoạt động | Not run |
| C01 | Console thật | Chưa có target/SDK/device | Blocked |

## Mẫu kết quả

```text
Date / tester:
Commit / engine version / build:
Environment (PIE/Standalone/Packaged):
Map / GameMode / Pawn / Controller / AnimBP:
Device / transport / remapping software:
Test ID:
Steps:
Expected:
Actual:
Result (Pass/Fail/Blocked):
Log / screenshot / video timestamp:
Follow-up:
```

## Validation lúc bàn giao

Đã kiểm tra tĩnh cấu trúc source/config, generated-header order, module/target naming, local Markdown links, không commit generated build artifacts. Kết quả cụ thể ghi trong `VALIDATION.md` sau khi chạy script. Không có UE installed: **build, UHT, unit tests trong engine, PIE, animation, controller hardware, cook và package đều chưa chạy**.
