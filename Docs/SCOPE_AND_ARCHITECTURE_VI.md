# Phạm vi và kiến trúc

Demo cá nhân độc lập, namespace HB; không tích hợp source Vault. Mục tiêu là trình bày luồng Character → Controller → Input → Animation → Feedback cùng một vòng gameplay có quyết định greed. Ưu tiên section GDD sáu slot tự gán, không khóa class.

| Yêu cầu | Phần triển khai | Giới hạn |
| --- | --- | --- |
| Town/Dungeon/Return/Defense | HBGameMode, năm wave, Gate/Anima | Một arena tái sử dụng, không load nhiều map |
| Greed | Ba cache +30 stones/+3 threat mỗi cache | Không procedural loot; kill cũng tăng threat |
| Sáu action | HBLoadout, DataAsset, cooldown theo ability | Utility bindings cố định, chưa có menu remap |
| Dodge | 0 MP, movement, miễn damage 0.15s đầu | Animation do Editor gắn |
| Combat | Basic melee, Pulse AoE, Guard, Mend, Bolt | Bolt là hit trực tiếp có kiểm tra tầm/visibility, chưa có projectile |
| Input | Enhanced Input, radial dead zone, camera relative | Click/enemy direct steering; không tìm đường vòng |
| Animation | HBAnimInstance, montage notify, lifecycle | Asset/skeleton/AnimBP do Editor gắn |
| Feedback | Rings, hit bursts, HUD, runtime identity | Arena phẳng; không decal terrain/shader toon hoàn chỉnh |
| Persistence | Lưu binding sáu slot hai device families | Không lưu run hoặc runtime loadout |

## Ownership

- Controller sở hữu mapping và binding handles, đọc input, chọn target, chuyển thành yêu cầu. Chỉ gỡ mapping/binding do nó tạo.
- Player Character sở hữu command hiện hành, một buffered command, attack lock, dodge và guard. Nonlethal hit không cancel attack. Death, unpossess, phase reset và explicit cancel dọn trạng thái.
- Loadout component sở hữu slot, MP, cooldown; kiểm tra điều kiện rồi chỉ trừ MP/commit cooldown khi hành động được nhận. Hai slot cùng ability dùng chung cooldown.
- Animation cung cấp pose và thời điểm melee hit qua notify. Gameplay không dựa vào tên animation node. Montage assigned nhưng phát lỗi bị hủy; fallback timer chỉ dùng nếu chưa gán montage.
- Feedback component chỉ trình bày trạng thái đã được nhận, không tự sở hữu gameplay command. Rings không collision; transient bursts tự hết lifetime.
- GameMode sở hữu phase/economy/wave/objectives. Enemy registry dùng weak pointer; snapshot khi AoE có thể làm enemy bị hủy. Timer và device delegate được gỡ khi EndPlay.
- HBRuleCore là phần C++ độc lập thực sự dùng bởi runtime: dead zone, wave count, ability budget, greed ledger. Tests biên dịch header này trực tiếp.

Không dùng GAS, NavMesh, replication hoặc plugin runtime ngoài Enhanced Input. Không tuyên bố hoàn tất toàn bộ GDD. Balance và hình thức vẫn cần playtest/Editor review; không có console SDK hoặc platform được chỉ định để xác nhận console.
