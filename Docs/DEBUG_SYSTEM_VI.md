# Debug system — đọc luồng xử lý khi demo

Dùng bản **Development / Development Editor**. Không cần tạo Blueprint hay thêm component: `UHBDebugSubsystem` được tạo theo World. F2 trên bàn phím hoặc Left Trigger trên gamepad chuyển **State → Events → Config Checks → ẩn**; vẫn dùng được khi pause hoặc Game Over. Diagnostics mặc định ẩn, lịch sử vẫn ghi. Shipping không ghi history, không vẽ debug overlay, không chạy check/reset debug.

## Ba trang

| Trang | Nội dung | Cách đọc |
| --- | --- | --- |
| State | FPS/frame time/biểu đồ, Map/GameMode/controller/pawn/Anim class, playback, outcome, command, buffer, target, tốc độ và sai lệch aim | Đây là trạng thái hiện tại, kể cả khi history bị freeze |
| Events | 9 sự kiện gần nhất khớp filter, cũ trên/mới dưới | Mỗi dòng có sequence, thời gian game, category và thông điệp |
| Config Checks | Kết quả lần kiểm tra cấu hình gần nhất | OK=kiểm tra cụ thể đạt; INFO=chế độ dự phòng; WARN/ERROR=cần xem lại; CHECK=cần thao tác xác nhận |

History giữ tối đa **128 sự kiện**, mỗi message tối đa 240 ký tự. Dòng HUD dài được rút gọn; dump để đọc đủ. Không giữ tham chiếu tới enemy/pawn đã hủy. Không ghi analog mỗi frame. Game time dừng khi pause; sequence vẫn phân biệt được các sự kiện cùng thời điểm. History thuộc một World: reload map/thoát PIE xóa history. Cửa sổ PIE khác không dùng chung history.

## Console commands

Mở console bằng phím console đã cấu hình của Unreal (mặc định thường là `~`), hoặc dùng ô command trong Output Log.

| Lệnh đầy đủ | Tác dụng |
| --- | --- |
| `HBDebug` | Chuyển trang như F2 |
| `HBDebugCheck` | Chạy lại kiểm tra cấu hình, thay snapshot cũ |
| `HBPerfClear` | Xóa cửa sổ FPS/frame time và bắt đầu lấy mẫu lại; không xóa event history |
| `HBDebugClear` | Xóa history; giữ checks/filter/freeze và số sequence tiếp tục tăng |
| `HBDebugFreeze` | Bật/tắt dừng ghi history; không pause game, không đóng băng trang State |
| `HBDebugFilter All` | Xem mọi category |
| `HBDebugFilter Animation` | Chỉ xem animation; cũng nhận Input, Command, Combat, System |
| `HBDebugDump` | In FPS/frame-time summary, checks và tối đa 128 sự kiện khớp filter ra `LogHideInTheBox` trong Output Log |
| `HBDebugReset` | Reload toàn bộ map hiện tại, về trạng thái khởi đầu, giống restart; cần save map trước |

Filter chỉ đổi cách xem/dump, không ngừng lưu category khác. Freeze chỉ ngừng thu thập event history, FPS vẫn được lấy mẫu: sự kiện xảy ra lúc freeze **không được ghi bù**. Dump không tự tạo file riêng; log dùng cơ chế logging bình thường của Unreal, thường nằm trong `Saved/Logs`. Muốn giữ dấu vết trước reset, dump trước. Không có reset riêng từng bài lab trong bản này.

## Một lượt kiểm tra cụ thể

1. F2 hai lần để mở Events. Đặt `HBDebugFilter All`; nếu thấy FROZEN, gọi HBDebugFreeze để trở lại RECORDING. Gọi HBDebugClear.
2. Click enemy: thấy Input slot → Command attack accepted → Animation started. Ability result có thể xuất hiện sau Animation started vì activate được xử lý đồng bộ; không giả định đó là lỗi thứ tự.
3. Click sàn giữa swing: thấy Move buffered. Khi kết thúc: outcome → Swing released → Move accepted. Nếu spam click, buffer cuối cùng thắng.
4. Khi notify đến: thấy Notify received → Hit event accepted → Combat damage. Hit event accepted chỉ nghĩa là sự kiện hit được xử lý, **chưa đảm bảo damage**; target mất/ngoài tầm/bị che sẽ báo Melee missed.
5. Thử thiếu notify, duplicate notify, sai asset, interrupt và cancel. Các nhánh có thông điệp riêng. Nếu chưa gán montage thì Timed fallback là hành vi chủ động.
6. Gọi HBDebugFreeze rồi HBDebugDump để giữ lại tình huống, xem Output Log; gọi Freeze lần nữa trước lượt thử tiếp.

Category Input ghi slot/utility và reset input; Command ghi accepted/buffered/cancel; Animation ghi playback/notify/outcome; Combat ghi kết quả slot, damage player/enemy/structure và dodge immunity; System ghi chuyển phase/checks. Chưa có correlation ID riêng cho từng input/swing: đọc theo sequence và tên target, không coi đây là distributed trace.

## Kiểm tra cấu hình làm được tới đâu?

Tự chạy sau BeginPlay khoảng 0.2 giây; `HBDebugCheck` để chạy lại. Kiểm tra Enhanced Input component/mapping objects, pawn được possess, mesh/AnimInstance, montage và skeleton khác nhau, loadout fallback/giá trị số, material hình khối và ring được gán.

Skeleton khác nhau chỉ là WARN vì có thể được cấu hình tương thích. Có AnimInstance hoặc montage assigned không chứng minh Slot nối ra Output Pose hoặc notify nằm đúng track: phải đánh thử và xem Events. Material assigned không chứng minh parameter Color/Opacity đúng tên. Native placeholder/no montage là INFO, không phải lỗi build. Nếu không có pawn, HUD gameplay chưa vẽ được: dùng HBDebugCheck rồi HBDebugDump trong Output Log.

## Checklist nghiệm thu trong Unreal (chưa chạy)

- F2/LT qua ba trang rồi ẩn, kể cả lúc pause/Game Over; không đổi gameplay.
- Tạo hơn 128 sự kiện: history chỉ giữ phần mới; không tăng bộ nhớ vô hạn.
- Filter Animation không hiện Combat; All khôi phục những dòng còn trong history.
- Freeze rồi thực hiện action: State/FPS vẫn đổi, event history không đổi; unfreeze tiếp tục ghi.
- Clear khi frozen: history trống, vẫn frozen; sequence không quay về 1.
- Dump đúng filter; giữ đủ message chưa bị rút gọn bởi HUD.
- Bỏ Anim Class khi đã có montage: check báo ERROR; bỏ Ring Material: báo WARN; native placeholder vẫn dùng được.
- Restart: history world cũ không xuất hiện, timer check cũ được dọn; run/phase được reset theo đường reload bình thường.
- Shipping: không history/overlay/check/reset debug. Các utility gameplay như F5 vẫn giữ hành vi của game.

Các kiểm tra source không xác nhận UHT/UBT hoặc giao diện thực tế. Xem [VALIDATION.md](VALIDATION.md) để biết phần đã chạy.

## FPS và frame time trên trang State

Tự hoạt động khi HUD được vẽ, kể cả khi ẩn bảng debug; không cần asset hoặc component mới. Mỗi World giữ một cửa sổ tối đa **240 khoảng frame**, không cấp phát thêm cho từng mẫu. Mở F2 một lần để xem:

- **FPS**: nghịch đảo thời lượng frame mới nhất; số này có thể dao động nhanh.
- **Average FPS**: số khoảng frame chia tổng thời gian đã lấy mẫu, không lấy trung bình cộng của FPS tức thời.
- **Frame / Average / Worst (ms)**: khoảng frame mới nhất, trung bình và dài nhất trong cửa sổ hiện tại.
- **Biểu đồ**: mẫu cũ bên trái, mới bên phải; trục ngang là frame, không phải giây cố định. Cửa sổ 240 frame khoảng4s ở60FPS hoặc2s ở120FPS. Hiển thị cả số mẫu và tổng thời lượng.
- Đường tham chiếu **16.67ms (60FPS)** và **33.33ms (30FPS)**; đoạn vượt33.33ms màu vàng. Trục dọc tự tăng theo frame tệ nhất, tối thiểu50ms; không cắt bỏ spike dài.

Cách đo: thời gian thực `FPlatformTime::Seconds()` giữa các lần HUD được vẽ ở hai engine frame liên tiếp; `GFrameCounter` tránh ghi trùng cùng frame. Đây là **nhịp frame quan sát tại HUD trên game thread**, không phải GPU time, GPU utilization, input latency hoặc số frame thực sự được màn hình trình bày. Không dùng gameplay DeltaSeconds nên time dilation không trực tiếp đổi thang đo. PIE có chi phí Editor; so sánh hiệu năng bằng cùng map, resolution, cấu hình cap/VSync và bản Development executable.

**Pause**: ngừng thêm mẫu, giữ giá trị cũ và hiện nhãn PAUSED; frame đầu khi resume chỉ đặt lại mốc thời gian. Khoảng bị bỏ qua do HUD không được vẽ nhiều engine frame cũng chỉ đặt lại mốc, không được coi là hitch. Lịch sử trước khoảng ngắt vẫn giữ, biểu đồ ghép các mẫu hợp lệ liên tiếp trong bộ nhớ. Hitch thật giữa hai engine frame liên tiếp vẫn được giữ, kể cả lớn hơn1 giây. Chưa tự loại toàn bộ mẫu alt-tab/background; dùng HBPerfClear sau khi lấy lại focus để đo một lượt sạch.

`HBPerfClear` chỉ xóa performance stats. `HBDebugClear` chỉ xóa event history. `HBDebugFreeze` chỉ dừng event history. Reload map xóa cả hai. Shipping không lấy mẫu hoặc vẽ performance overlay.

**Test trong Unreal cần chạy:** kiểm tra warm-up sau HBPerfClear, so sánh vùng FPS ổn định với bộ đếm engine, thử giới hạn30/60FPS nếu môi trường hỗ trợ, pause/resume không thêm spike giả, event freeze vẫn cho FPS cập nhật, thu hơn240 mẫu không tăng dung lượng, nhìn biểu đồ ở1280×720 và1920×1080. Kết quả Unreal hiện chưa được xác nhận; test C++ cho công thức và bộ đệm đã có trong Tests/rules_test.cpp.
