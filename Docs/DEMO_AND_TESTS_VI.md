# Kịch bản demo và nghiệm thu

Tất cả mục Editor/runtime dưới đây là **chưa chạy**, đánh dấu sau khi quan sát trên máy có UE. Ghi engine version, commit, map, model gamepad và loại kết nối; ghi riêng PIE/Standalone/packaged.

## Kịch bản trình bày khoảng 5–8 phút

1. Town: mở F2 trang State, chỉ map/GameMode/controller/pawn/Anim class thực tế. Di chuyển chậm/full stick, đổi aim; thử camera yaw 90 rồi trả 0.
2. Dungeon: click tới vị trí, hover enemy, chọn attack; click lệnh khác giữa swing để thấy buffer chỉ xuất hiện thành lệnh sau khi đòn kết thúc.
3. Montage: chỉ HP/hit count đổi ở HB Attack Hit, phân biệt playback và assigned. Chịu một đòn không chết trong lúc đánh để thấy đòn không bị ngắt.
4. Sáu kỹ năng: Pulse, dodge không tốn MP, Guard, Mend khi mất HP, Bolt có tầm. Cho thấy cooldown/MP trên HUD.
5. Greed: lấy một cache, giải thích +30 stones đổi +3 Threat. Về prep, xây turret/repair rồi bắt đầu Defense.
6. Defense: enemy chuyển từ Gate sang Anima nếu Gate bị phá; trình bày death/respawn. Chạy đủ năm wave trong buổi kiểm tra, không cần chờ hết trong bài trình bày ngắn.
7. Gamepad: pause/resume, rút/cắm lại, thả input về neutral rồi tiếp tục. Restart bằng Back.

## Checklist bắt buộc

| Nhóm | Thao tác / kết quả cần đạt |
| --- | --- |
| Build | Full Editor build UHT/UBT pass, mở project không missing module |
| Movement | WASD đối nhau triệt tiêu; đường chéo không nhanh hơn; partial stick có tốc độ nhỏ hơn; yaw 0/90 vẫn đi theo camera |
| Aim | Right stick độc lập move; thả giữ hướng cuối; đổi mouse/pad không tạo movement cũ |
| Buffer | Nhiều click lúc swing chỉ giữ lệnh cuối; cancel/interruption xóa buffer; không hiện move destination trước khi command được nhận |
| Melee | Notify đúng gây một hit; duplicate notify không gây hit thứ hai; target chết trước notify không crash |
| Montage lỗi | Bỏ notify: không timed damage; gán montage không phát được: báo lỗi/hủy, không fallback hit |
| Lifecycle | Complete/interrupted/cancel khác nhau trong diagnostics; chết/reset/unpossess không để timer hit muộn |
| Skills | Dodge 0MP; cửa sổ miễn damage đầu; Guard giảm damage; Mend đầy HP không mất MP; Bolt ngoài tầm/tường không mất MP |
| Cooldown | Gán cùng ability hai slot không bypass cooldown; thiếu MP/thất bại không tiêu tài nguyên; MP hồi theo thời gian |
| Remap | HBRebind 2 Q false → Q hoạt động và tồn tại sau restart; key trùng/reserved/analog/wrong-device bị từ chối |
| Greed | Kill sáu enemy:90/6; lấy đủ ba cache:180/15; bấm lại không nhận thêm; rời dungeon xóa cache |
| Waves | Threat6: wave1=5, wave5=9; Threat15: wave1=8, wave5=12; timeout/clear qua break 3s; năm wave về Town |
| Economy | Turret30, repair20; thiếu tiền không xây/sửa; socket occupied không trừ tiền; không build/repair trong Defense kể cả break |
| Death | Dungeon chết kết thúc; Defense chết hồi sinh3s; Anima chết kết thúc, không respawn muộn |
| Input reset | Pause lúc giữ input, resume, alt-tab, disconnect/reconnect không stuck movement/attack; gamepad-only có đường resume/restart |
| Cleanup | Restart/đổi phase nhiều lần không nhân đôi binding, ring hoặc timer; burst fade rồi biến mất |
| Visual | Màu dễ phân biệt, ring không chặn trace/collision, chân mesh chạm sàn, text HUD đọc được ở720p/1080p |
| Packaging | Save map, cook, chạy Development executable ngoài Editor; lặp controls/phase/animation trên bản đóng gói |

## Debug system

Thực hiện [checklist debug](DEBUG_SYSTEM_VI.md): ba trang, filter/freeze/clear/dump, cap128, cảnh báo cấu hình, reset và Shipping. Dùng Events để chỉ ra command buffered/notify/damage thật, không chỉ mô tả sơ đồ. Các mục runtime này chưa chạy trong môi trường tạo source.

## Kiểm tra tự động

`python Scripts/validate_source.py` kiểm tra cấu trúc. `python Scripts/test_rules.py` compile/chạy logic thực từ HBRuleCore, không dùng Unreal stubs. Trong Unreal có nhóm `HideInTheBox.Rules`; chạy qua Automation hoặc `Automation RunTests HideInTheBox.Rules` sau khi build.

Không dùng số lượng sample trong dead-zone test để suy ra gameplay coverage. Không coi screenshot/PIE pass là packaged hoặc console pass. Xem [VALIDATION.md](VALIDATION.md) để biết kết quả đã thực sự chạy.
