# Phạm vi và kiến trúc prototype

## Nguồn và thứ tự ưu tiên

Dựa trên GDD Project Vaultbreaker, Unreal Developer Technical Guide, PC/Gamepad/Console Test Plan, Player Feedback Roadmap và Move Destination Indicator Task do người dùng cung cấp. Bản này là repo mới, không phải patch lên checkout Vault `4a0a0b0` hoặc các nhánh đồng nghiệp. Không đưa các tài liệu nội bộ gốc lên repo; chỉ ghi quyết định implementation cần thiết.

GDD phần Character Building nói không có class cố định; vài phần sau vẫn nhắc “four classes”. Prototype theo thiết kế mới: một player với basic attack/Pulse, chưa đóng khung hệ class. GDD còn nhiều giá trị pending, nên tất cả damage/range/cost/duration trong code là tuning demo.

## Chia trách nhiệm

| Lớp | Sở hữu | Không làm |
| --- | --- | --- |
| HBPlayerController | Mapping/action, cursor trace, input family, stick, pause, lifecycle input | Không giữ command combat riêng |
| HBPlayerCharacter | Command, target, một buffer, movement, attack lock và hit guard | Không tự vẽ HUD |
| HBFeedbackComponent | Ba actor vòng; nhận snapshot command/destination/target | Không sửa destination/velocity/ability |
| HBIndicator | Instanced mesh presentation, tint, thickness | Không collision, không gameplay tick |
| HBAnimInstance / Notify | Velocity cho locomotion; chuyển notify tới đòn đang chạy | Không tự chọn enemy |
| HBGameMode | Phase, tài nguyên, registry enemy, wave và spawn arena | Không phải multiplayer authority design đầy đủ |
| HBEnemyCharacter | Direct chase, player proximity, Gate → Anima | Không weighted A* |
| HBStructure | HP Gate/Anima, turret target/timer | Không tạo economy ngoài GameMode |

Enemy registry dùng weak references, unregister trong EndPlay. Pulse/cleanup lấy snapshot vì damage có thể destroy và sửa registry. Turret chọn target mỗi 0.7 giây khi cần, không world search từng frame. Enemy steering chạy mỗi frame vì CharacterMovement tiêu thụ movement input mỗi frame. Timer và device delegate có cleanup.

## Command và input contract

- None, Move, Attack là state chính. Đánh trong tầm bật `bAttacking`, khóa movement và facing tới hết swing.
- Click trong attack thay thế một buffered command. Indicator vẫn phản ánh command đang active; không hiển thị ý định chưa chạy.
- Direct movement không đi vào discrete buffer. Giữ stick lúc unlock sẽ tiếp tục direct movement và có thể thay thế buffered move. Đây là lựa chọn demo cần nói rõ.
- Chuột click sàn được chấp nhận từ trace `WalkableFloor`; click miss không tự đổi command. Enemy trace là Visibility trước floor trace.
- Arrival 30 cm; direct steering bị đứng yên 1.5 giây thì bỏ command. Không có tìm đường vòng hoặc xác minh reachability.
- Gamepad attack chọn enemy gần nhất trong 750 cm; khi right-stick hoạt động, lọc cone theo dot >= 0.4. Không right-stick thì chọn nearest. Đây là quy tắc prototype vì GDD chưa chốt targeting gamepad.
- Camera yaw cố định 0; screen up tương ứng +X, right +Y. Nếu xoay camera phải đổi input basis, không chỉ đổi boom rotation.
- Pause/focus flush/phase change xóa input tạm và command; resume yêu cầu neutral. OS/controller disconnect behavior phải được test trên thiết bị thật. Disconnect callback pause khi đang dùng pad.
- Chưa có asset IMC editable, remap UI hay persistent user bindings. Actions/IMC được tạo runtime bởi controller.

## Đối chiếu GDD

| Hạng mục | Prototype |
| --- | --- |
| Town → Dungeon → Return → Defense | Có state loop; cùng arena, chưa map travel |
| Loot hỗ trợ defense | Dungeon kill +15 stones; turret -30; Gate repair -20 |
| Greed/retaliation | Mỗi kill +1 Threat; wave = clamp(2 + wave + threat/3, 1, 16); chưa có optional greed choice |
| Năm wave, timer clear, break | Có; 40s/wave, 3s break là tuning demo |
| Defense không build/repair | Khóa cả break |
| Gate trước, Anima sau | Có objective targeting; cấu trúc không chặn đường |
| Player chết dungeon solo | Game over; không snapshot |
| Player chết defense | Respawn 3s tạm, chưa gear penalty vì GDD còn pending |
| Dungeon loot vs defense loot | Defense kill không cho tiền; reward cuối phase được credit trực tiếp thay chest |
| Animation | Native data/notify bridge; assets cần Editor |
| Destination/hover/attack feedback | Có mesh rings; chưa fade, chưa support slope/capsule khác |
| GAS, ability cards, stats, race, inventory, gear | Chưa làm |
| Barricade, siege, saboteur, boss, companion | Chưa làm |
| Save/load, procgen, campaign/endless hoàn chỉnh | Chưa làm |
| Six slots/rebinding và console | Chưa làm / chưa kiểm tra |

Prototype tập trung chứng minh đường input → command → movement/attack → feedback, kèm vòng game có thể trình diễn. Không đáp ứng toàn bộ GDD.

## Khi đem tính năng về Vault

Bản task gốc yêu cầu subclass mỏng của AVaultCharacter và component hiển thị, không viết lại navigation/combat. Trong repo độc lập chưa có class đó. Khi tích hợp: port component/ring, đọc command của đúng branch đang dùng; giữ nguyên command owner của branch đó. Nếu controller mới sở hữu command, adapter đọc controller. Không copy toàn bộ HBPlayerCharacter/GameMode vào production để tránh tạo hai nguồn state.

## API tham khảo

Đã đối chiếu API công khai của Epic; trang có thể mặc định phiên bản mới hơn target. Chưa có source engine 5.7 tại máy tạo code nên UHT/UBT vẫn là gate bắt buộc.

- [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine)
- [PlayerController](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APlayerController)
- [Input device mapper](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/ApplicationCore/IPlatformInputDeviceMapper)
- [AnimNotify Notify](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Animation/AnimNotifies/UAnimNotify/Notify/2?application_version=5.5)
