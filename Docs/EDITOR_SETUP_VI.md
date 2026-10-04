# Checklist Unreal Editor cho ngày mai

Mục tiêu đầu tiên là chạy arena bằng native classes, sau đó mới thêm mesh/animation. Target được viết cho UE 5.7 theo tài liệu Vault đã cung cấp; chưa có build xác nhận. Không cần custom engine, Wwise, GAS hay asset từ repo công ty cho bản độc lập này.

## 1. Build source trước

- Cài UE 5.7 và C++ toolchain mà bản engine của bạn yêu cầu. Dùng engine đã có nếu đúng 5.7; nếu dùng source build, chọn lại Engine Version để `.uproject` trỏ đúng engine.
- Clone repo, checkout branch prototype, mở `HideInTheBox.uproject`; cho phép generate project files/build module. Hoặc dùng `Scripts/Build-Editor.ps1` với đường dẫn engine của bạn.
- Target là `HideInTheBoxEditor`, configuration Development Editor, platform Win64. Đóng Editor khi build lần đầu và khi đổi reflected headers; tránh dùng Live Coding làm bằng chứng UHT pass.
- Nếu báo lỗi compile, giữ nguyên log lỗi đầu tiên, file và dòng. Không đánh dấu checklist build pass chỉ vì project files generate thành công.

## 2. Tạo map tối thiểu

1. File → New Level → **Empty Level**. Không dùng template map có floor/lighting sẵn vì code tự tạo arena.
2. Save thành `Content/Maps/DemoArena.umap`, tương ứng `/Game/Maps/DemoArena`.
3. Thêm **PlayerStart** ở `X=-400, Y=0, Z=100`.
4. World Settings → GameMode Override → **HBGameMode**.
5. Project Settings → Maps & Modes: Editor Startup Map và Game Default Map cùng là DemoArena; Default GameMode là HBGameMode.
6. Save All. Nhấn Play; click viewport để có focus. Nhân vật mặc định là hình hộp xanh, enemy là hộp đỏ nếu material engine hỗ trợ parameter Color.

Kết quả cần thấy: sàn chữ nhật và tường biên, HUD, Player, Gate và Anima. Nếu thấy DefaultPawn bay, kiểm tra GameMode override và Default Pawn Class. Đừng đặt thêm player pawn Auto Possess vào level.

Native GameMode tạo arena khi StartPlay, vì vậy viewport trước Play có thể gần như trống. Đây là bình thường. Sau khi tạo map, commit `.umap` và thay đổi Maps & Modes cùng các asset mới; repo không bỏ qua Content.

## 3. Chạy một vòng gameplay với hình khối

- Enter vào dungeon; đi tới nhóm enemy, LMB enemy để đánh. Một press = một đòn; nhấn lại hoặc buffer đòn kế. RMB dùng Pulse ở bán kính 350.
- Dungeon enemy chỉ bắt đầu đuổi trong khoảng 700; kéo từng nhóm để thử input trước.
- Hạ sáu enemy: dự kiến có 90 stones, Threat 6. Enter về Return/prep; nhân vật hồi HP/MP.
- B bật build, Tab chọn socket, LMB xây (30 stones). Chọn ba socket khác nhau để đặt ba turret. B thoát build.
- Enter thủ năm wave. Enemy ưu tiên Gate, đánh player nếu ở gần; Gate hết HP chuyển sang Anima. Turret chỉ bắn trong Defense.
- Wave hết enemy hoặc hết 40 giây thì qua break ba giây; không build/repair trong break. Hết wave 5 nhận reward trực tiếp rồi về Town.
- P pause; F5 restart level đã lưu. Nếu chưa save map, restart không phải đường kiểm tra hợp lệ.

Đây là kết quả mong đợi để test, chưa phải báo cáo chạy thành công. Balance chưa được playtest.

## 4. Gắn nhân vật và locomotion

1. Import hoặc thêm skeletal mesh và animation mà bạn có quyền dùng, cùng skeleton tương thích. Repo không kèm mannequin hoặc asset thương mại.
2. Tạo Blueprint subclass của **HBPlayerCharacter**, đặt `BP_DemoPlayer`.
3. Ở Mesh: gán skeletal mesh, sửa relative location/rotation cho chân nằm trên sàn. Với mannequin thông dụng thường cần Z khoảng -88 và yaw khoảng -90; quan sát skeleton thực tế trước khi chốt.
4. Tạo Animation Blueprint từ cùng skeleton, Parent Class = **HBAnimInstance**. Nếu wizard không cho chọn, mở Class Settings → Parent Class.
5. AnimGraph: State Machine Idle/Move hoặc Blend Space; đọc `fSpeed` (cm/s) từ parent. Input của locomotion là velocity thực, không phải key held. MaxWalkSpeed hiện 450; Blend Space từ 0 tới 450 là mốc thử.
6. Thêm **Slot DefaultSlot** giữa locomotion và Output Pose để montage có đường output.
7. Gán Anim Class này cho Mesh trong BP_DemoPlayer.
8. Tạo Blueprint subclass **HBGameMode**, đặt `BP_DemoGameMode`; Default Pawn Class = BP_DemoPlayer. Giữ Controller/HUD là các lớp HB mặc định. Đổi World Settings và Default GameMode sang BP_DemoGameMode.

Placeholder được tự ẩn trong BeginPlay nếu Skeletal Mesh đã được gán. Không cần tự xóa component. Thử idle, đi chậm/full-stick, dừng, đâm tường: đứng kẹt thì tốc độ animation phải giảm theo velocity.

## 5. Nối một đòn đánh qua montage và notify

1. Tạo montage cùng skeleton từ animation đánh tại chỗ. Dùng Slot **DefaultGroup.DefaultSlot**.
2. Trong AnimBP đặt Root Motion Mode **Ignore Root Motion** cho demo này. Đừng dùng root-motion displacement để tránh tranh quyền với direct movement.
3. Đặt **HB Attack Hit** notify **trực tiếp trên notify track của montage**, đúng frame tiếp xúc. Không chỉ đặt notify trong animation sequence nguồn: implementation kiểm tra asset montage đang đánh.
4. Gán montage vào `Attack Montage` trên BP_DemoPlayer (Class Defaults → Combat).
5. Chạy: HUD có “MONTAGE ASSIGNED”; hit counter và HP enemy phải đổi tại notify. Dòng này chỉ chứng minh property được gán, chưa chứng minh montage playback hoặc notify đúng.
6. Thử bỏ notify: với montage chạy được, không được có damage tự xuất hiện bằng timer. Nếu montage không phát được, code chuyển sang timed fallback; kiểm tra mesh, skeleton, Anim Class và Slot.
7. Thử spam/duplicate notify: tối đa một melee hit mỗi swing. Thử enemy chết trước notify: không dereference target đã hủy.
8. Click sàn lúc đang đánh: buffer hiện trên HUD; indicator move chỉ xuất hiện sau khi montage kết thúc và command được nhận.

Đòn đánh bị hủy bởi cancel/unpossess/phase reset: xóa buffer và timer. Montage interrupted tự xóa buffered command. Timeout chỉ giải phóng lock, không gây timed damage trong montage mode.

## 6. Visual feedback

Vòng có sẵn bằng 32 cube instances, không phải DrawDebug. Bề dày và màu phân biệt hover/command. Material mặc định lấy từ Engine BasicShapes; kiểm tra màu và ánh sáng trên bản engine của bạn. Các vòng vẫn có hình dạng nếu Color parameter không có hiệu lực.

Phiên bản này chưa có opacity fade hoặc decal bám mặt dốc. Arena phẳng z=0 và capsule enemy nửa cao 88 được dùng để đặt vòng dưới chân. Khi thay capsule/terrain, chỉnh adapter `ShowActorRing()` hoặc trace xuống sàn trước khi port.

## 7. Đóng gói Windows

- Save All; đặt startup maps trước khi cook. Build Development trước.
- Project Settings → Packaging: bảo đảm DemoArena được cook; config hiện dùng Cook All để giảm rủi ro thiếu asset trong demo nhỏ.
- Platforms → Windows → Package Project. Chạy executable ngoài Editor.
- Test lại chuột và gamepad thật, ghi model/connection/Steam Input, commit, engine version và log. Không coi PIE pass là packaged pass.

Sau khi build được, có thể chạy automation từ Session Frontend → Automation → `HideInTheBox.Rules`. Hoặc Output Log: `Automation RunTests HideInTheBox.Rules`. Đây là unit test cho rule thuần, không test asset wiring/gameplay.
