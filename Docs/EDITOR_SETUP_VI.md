# Lắp demo trong Unreal Editor

Hướng dẫn này dành cho **UE 5.7, C++, single-player**. Gameplay loop và luật chơi nằm trong C++; phần bạn làm trong Editor là một phần thật của game: dựng map, chọn/đặt asset, tạo các Blueprint con để gán mesh và camera, rồi nối Animation Blueprint và montage. Map không tự sinh sàn/tường/đèn/objective bằng hình khối lúc Play.

Source handoff không chứa Unreal Engine nên chưa thể chạy UHT/UBT, PIE hay packaging tại đây. Chưa có map/asset `.uasset`/`.umap` trong repo. Sau khi lấy bản code mới, bạn sẽ build một lần rồi làm các bước Editor bên dưới.

## 1. Lấy code và mở project

Trên máy không dùng Git, tải ZIP của nhánh demo mới nhất, giải nén cả thư mục rồi mở `HideInTheBox.uproject`. Đóng Unreal Editor trước khi compile C++.

```powershell
powershell -File Scripts/Build-Editor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
```

Hoặc chuột phải `.uproject` → **Generate Visual Studio project files**, mở `.sln`, chọn **Development Editor / Win64**, Build `HideInTheBox`. `.sln` là file sinh ra ở máy bạn nên không nằm trong ZIP. Không dùng log build của bản ZIP cũ để xác nhận source mới.

Nếu thấy `Unable to build while Live Coding is active`, Save All, đóng Editor/game hoàn toàn rồi Build lại. `MSB3073 ... code 6` là mã tổng kết; lỗi gốc nằm ở dòng đầu tiên trong Output → Build hoặc `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`.

## 2. Tạo map và dựng không gian chơi

1. Tạo **Empty Level**, lưu thành `/Game/Maps/DemoArena`.
2. Dựng sàn, tường, lối đi, vật chắn, cổng và vùng dungeon bằng mesh thật/asset bạn được phép dùng. Tạo hai vùng dễ đọc: khu Town/Defense và khu Dungeon; có thể dùng cổng, ánh sáng, màu sàn hoặc đạo cụ để chỉ lối đi.
3. Đặt Directional Light, Sky Light, Post Process Volume và accent lights bằng Editor. Đặt camera nhìn rõ player, Gate và Anima ở vùng Defense.
4. Thêm **PlayerStart** để Unreal tạo player lúc bắt đầu. Thêm anchor actor tại Town, Dungeon và Defense; chọn từng actor, Details → Actor → Tags → **+**, nhập đúng tag ở bảng dưới.
5. World Settings → GameMode Override = `BP_DemoGameMode`. Project Settings → Maps & Modes → đặt DemoArena làm Editor Startup Map và Game Default Map.

Code chỉ đọc các actor đã đặt trong map qua Actor Tags. Dùng `TargetPoint` hoặc Empty Actor làm anchor. Đặt tag không có tiền tố `HB_` nào khác hoặc khoảng trắng; tag phân biệt chính tả. Để setup dễ kiểm tra, đặt tên actor dễ nhận biết như `SP_Town`, `SP_Dungeon`, `Socket_01`.

| Số lượng tối thiểu | Actor trong map | Actor Tag | Dùng cho |
| ---: | --- | --- | --- |
| 1 | PlayerStart | — | Unreal spawn player ban đầu |
| 1 | TargetPoint/Empty Actor | `HB_TownSpawn` | Town và sau khi thắng defense |
| 1 | TargetPoint/Empty Actor | `HB_DungeonPlayerSpawn` | Điểm vào dungeon |
| 1 | TargetPoint/Empty Actor | `HB_DefensePlayerSpawn` | Điểm hồi sinh trong defense |
| 1 | BP_Gate | `HB_Gate` | Mục tiêu đợt địch ưu tiên phá |
| 1 | BP_Anima | `HB_Anima` | Mục tiêu cuối khi Gate bị phá |
| 4 | TargetPoint/Empty Actor | `HB_BuildSocket` | Vị trí preview/xây turret; có thể đặt nhiều hơn |
| 3 | Mesh Actor hoặc Blueprint cache | `HB_GreedCache` | Cache đặt trực tiếp trong Dungeon |
| 6 | TargetPoint/Empty Actor | `HB_DungeonSpawn` | Điểm xuất hiện địch trong Dungeon |
| 4+ | TargetPoint/Empty Actor | `HB_WaveSpawn` | Điểm xuất hiện địch theo wave |

Tag Gate/Anima lên **Actor Blueprint instance ở level**, không chỉ lên Static Mesh Component. Cache nào cũng được miễn actor có mesh nhìn thấy được và tag đúng. Nếu thiếu actor bắt buộc, log sẽ ghi rõ tag cần bổ sung; map không tự dựng hình thay thế.

## 3. Tạo Blueprint asset để gắn mesh/camera

Tạo thư mục `/Game/HideInTheBox/Blueprints`, `/Game/HideInTheBox/Maps`, `/Game/HideInTheBox/Animation`, `/Game/HideInTheBox/Materials`, `/Game/HideInTheBox/Data`.

| Asset | Parent / loại | Việc làm trong asset |
| --- | --- | --- |
| `BP_DemoPlayer` | `HBPlayerCharacter` | Gán Skeletal Mesh, AnimBP, montage, camera và loadout |
| `BP_DemoEnemy` | `HBEnemyCharacter` | Gán mesh/material enemy |
| `BP_DemoController` | `HBPlayerController` | Có thể dùng nguyên; không cần Event Graph cho input |
| `BP_DemoGameMode` | `HBGameMode` | Default Pawn = BP_DemoPlayer; Player Controller = BP_DemoController; Enemy Class = BP_DemoEnemy; Turret Class = BP_Turret |
| `BP_Gate` | `HBStructure` | Gán mesh/material Gate |
| `BP_Anima` | `HBStructure` | Gán mesh/material Anima |
| `BP_Turret` | `HBStructure` | Gán mesh/material turret; class này được spawn khi bạn xây |
| `DA_DemoLoadout` | Data Asset `HBLoadoutData` | Tùy chỉnh sáu ability/slot nếu muốn |
| `ABP_DemoPlayer` | AnimBP parent `HBAnimInstance` | Locomotion State Machine và slot montage |

Trong `BP_DemoGameMode`, chỉnh **Class Defaults**. Field `Enemy Class` và `Turret Class` nằm trong nhóm Demo/Classes. Cài `BP_DemoGameMode` cho level qua World Settings để Default Pawn và Player Controller class cũng đến từ BP. HUD vẫn dùng `HBHUD`.

### Camera top-down trong BP_DemoPlayer

`HBPlayerCharacter` đã có sẵn component `CameraBoom` và `Camera`. Không tạo camera thứ hai. Mở `BP_DemoPlayer` → Components → chọn **CameraBoom**: chỉnh Target Arm Length (điểm bắt đầu 1800), Relative Rotation (X=-60, Z=0), bật/tắt Do Collision Test tùy map. Chọn **Camera** để chỉnh Field of View (thử 45–55) và relative transform nếu cần. Compile, đặt BP trong GameMode rồi thử PIE; chỉnh arm length/FOV để thấy trọn khu chơi mà nhân vật vẫn đủ lớn.

Trong BP_DemoPlayer, chọn **Mesh** component, gán Skeletal Mesh tương thích với bộ animation; căn Relative Location/Rotation để bàn chân chạm sàn. Tìm component **Loadout** và gán `DA_DemoLoadout` vào Definition nếu bạn tạo Data Asset. Các component Feedback/Loadout đã do C++ tạo, không cần tạo trùng.

## 4. Animation Blueprint và montage

1. Tạo Animation Blueprint dùng cùng Skeleton của player, đặt tên `ABP_DemoPlayer`; Class Settings → Parent Class = `HBAnimInstance`.
2. Trong AnimGraph, tạo locomotion State Machine với idle/walk/run. Từ state machine nối qua node **Slot** chọn `DefaultGroup.DefaultSlot`, rồi nối sang **Output Pose**.
3. Parent class cập nhật `fSpeed`, `fForwardSpeed`, `fRightSpeed`, `bFalling`, `bAttacking`, `bDodging`. Dùng speed để blend idle/walk/run, forward/right cho blend-space strafe nếu bộ animation hỗ trợ.
4. Tạo Attack Montage cùng Skeleton. Đặt slot `DefaultGroup.DefaultSlot`; trên notify track của montage, thêm notify **HB Attack Hit** đúng frame vũ khí chạm mục tiêu.
5. Gán Anim Class vào Mesh trong BP_DemoPlayer; gán montage trong nhóm Combat → Attack Montage. Thêm dodge animation/state theo `bDodging` nếu có.

Đây là graph thiết yếu cho animation/gameplay timing. C++ bắt đầu và kết thúc đòn; notify trong montage báo frame gây hit. Không chuyển tính toán damage, tài nguyên hay phase loop sang AnimBP. Nếu chưa có animation, gameplay có fallback để kiểm thử, nhưng montage đã gán mà thiếu notify sẽ không tự gây damage.

## 5. Hoàn thiện hình ảnh và dựng map

Ưu tiên asset có silhouette rõ, cùng scale/skeleton và giấy phép dùng phù hợp. Có thể dùng mannequin cùng animation tương thích cho player/enemy, nhưng hãy đặt mesh và animation vào đúng BP để kiểm tra pipeline thật.

Tạo material riêng cho sàn/tường và chọn material trong từng Static Mesh Actor/BP; chỉnh ánh sáng theo hai vùng. Dùng mesh đạo cụ/cổng/turret/cache thay hình khối tạm. Bố trí bốn socket quanh Gate ở vị trí turret có góc bắn thoáng. Đặt cache trong nhánh dungeon để người xem thấy chúng là mục tiêu phụ; spawn anchors đặt ngoài vật cản và không chồng sát nhau. Dùng label/Decal/mesh signage trong level nếu cần dẫn mắt; code không tạo layout hoặc biển báo.

Trong World Outliner, nhóm actor thành folder `Town`, `Dungeon`, `Defense`, `Gameplay Anchors`, `Lighting`. Đặt actor name theo mục đích và lưu map thường xuyên. Vẻ đẹp cuối cùng cần kiểm tra bằng PIE trong Editor vì source handoff chưa có render từ Unreal.

## 6. Kiểm tra vòng chơi

1. Save All, compile các BP, chọn DemoArena và Play.
2. Kiểm tra player xuất hiện trong Town, camera thấy map và mesh/animation đúng.
3. Enter/D-pad Up để vào Dungeon; hạ 6 enemy. Mỗi kill được +15 stones và +1 Threat.
4. Lấy tối đa 3 cache: đứng cách actor không quá 180 cm rồi E/L3. Mỗi cache +30 stones và +3 Threat. Đợi hạ hết enemy rồi Enter về prep.
5. B/D-pad Down bật build, Tab/D-pad Right đổi socket, LMB/A đặt turret (30 stones); R/D-pad Left sửa Gate (20 stones). Vào defense bằng Enter.
6. Thử năm wave, Gate rồi Anima, death/respawn trong defense và thắng cycle. Xem lỗi/tag/config bằng F2 debug panel; xem [DEBUG_SYSTEM_VI.md](DEBUG_SYSTEM_VI.md).

Các control đầy đủ, input remap và debug ở README / debug guide. Khi PIE pass, thêm DemoArena vào danh sách map cook, Save All rồi package Windows Development. Chạy executable bên ngoài Editor và làm checklist [DEMO_AND_TESTS_VI.md](DEMO_AND_TESTS_VI.md). Build thành công chưa chứng minh animation, tag hoặc layout đã được lắp đúng.
