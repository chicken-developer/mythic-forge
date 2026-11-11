<!-- Bản sao của tab Dev trong tài liệu Three Dream Golems — Game Design (2026-11-12). Khi thiết kế đổi, sửa tài liệu gốc trước rồi cập nhật file này. -->

# Three Dream Golems — Đặc tả cho dev

Tài liệu này mô tả những gì cần code cho bản Early Access của Three Dream Golems. Ý tưởng, cốt truyện và core loop nằm ở [GAME_DESIGN.md](GAME_DESIGN.md); ở đây chỉ nhắc lại khi cần cho việc code.

## 1. Cách đọc tài liệu

- **Con số là giá trị khởi điểm.** Mọi con số về máu, sát thương, thời gian, giá và tỉ lệ rơi đều là đề xuất để bắt đầu, chưa qua playtest. Tất cả phải nằm trong DataTable hoặc DataAsset để designer chỉnh mà không cần build lại code.
- **Tên trường dữ liệu viết bằng tiếng Anh** để dùng thẳng trong code. Tên hiển thị cho người chơi lấy từ bảng localization (EN, VI, zh-Hans).
- **Phần đối chiếu với code hiện có dựa trên báo cáo và cấu trúc repo**, chưa dựa trên việc đọc từng file nguồn. Việc đầu tiên của dev là xác nhận lại bảng ở mục 3 với code thật.
- **Host quyết định mọi thứ ảnh hưởng tới kết quả trận đấu.** Client chỉ dự đoán hiệu ứng hình ảnh và chuyển động của chính mình.

## 2. Luồng trạng thái của game

Một ngày-đêm đi qua các trạng thái dưới đây theo đúng thứ tự. Host giữ trạng thái hiện tại và báo cho client.

| # | Trạng thái (`EGamePhase`) | Thời lượng | Việc xảy ra | Điều kiện sang bước sau |
| --- | --- | --- | --- | --- |
| 1 | `Morning` | 30 giây | Thành phố hiện theo kết quả đêm trước. Little Princess gợi ý khu. Kẻ canh giữ của đêm nay được sinh ra từ seed. | Người chơi rời điểm thức dậy |
| 2 | `Day` | 2–4 phút | Đội tiêu 3 giờ tự do: dò manh mối, nâng vũ khí, trò chuyện. Gắn dream stone không tốn giờ. | Hết giờ, hoặc host chọn "Đi tới cổng mơ" |
| 3 | `DreamGate` | 20 giây | Đội theo Little Princess tới khu đã chọn, cô làm phép mở cổng. Game ghi cờ `bNightInProgress`. | Mọi người chơi bước qua cổng |
| 4 | `DreamHall` | Tự do | Cửa hàng của Princess phù thủy, lần 1. Mỗi Golem nhận một cuốn sách miễn phí. | Host xác nhận sẵn sàng |
| 5 | `Arena` × 3 | 3–5 phút mỗi sân | Mỗi sân có 1–3 Xenon bóng đêm. Hạ hết thì mở đường sang sân sau. | Mọi Xenon trong sân đã lấy lại màu |
| 6 | `RestPoint` | Tự do | Cửa hàng lần 2, ngay trước kẻ canh giữ. | Host xác nhận sẵn sàng |
| 7 | `Guardian` | 4–6 phút | Trận với kẻ canh giữ của khu. | Thắng, hoặc cả đội gục lần hai |
| 8 | `Wake` | 30 giây | Màn kết quả, trả DreamCoin, cập nhật trạng thái khu và sổ tay, lưu game. | Tự chuyển sang `Morning` của ngày kế |

Ba quy tắc chung:

- **Thua ở bất kỳ bước nào từ 5 đến 7** thì nhảy thẳng tới `Wake` với kết quả thua.
- **Thoát game giữa đêm** được tính là thua. Lần mở game sau, nếu thấy cờ `bNightInProgress` thì vào thẳng `Wake`, trả số DreamCoin đã kiếm được tới sân cuối cùng đã qua.
- **Người chơi chỉ vào phòng được ở `Day`, `DreamHall` và `RestPoint`.** Không cho vào giữa một sân đấu.

## 3. Đối chiếu với code hiện có

Nguyên tắc là giữ phần lõi đã chạy và đã có test, tắt những hệ thống quá nặng so với thiết kế mới, và viết thêm phần luật riêng của game. Những gì tắt đi thì để ngoài build, chưa xóa khỏi repo.

| Hệ thống trong repo | Xử lý | Ghi chú |
| --- | --- | --- |
| Lõi combat trên GAS (đòn đánh, sát thương, trạng thái) | Giữ | Thêm thuộc tính nguyên tố cho đòn đánh và hệ số sát thương theo vùng trúng |
| AI đồng đội và hệ thống threat | Giữ, thêm hành vi | Thêm phản ứng với nút ra hiệu và tự đặt màu, xem mục 12 |
| Gục và hồi sinh | Giữ | Chỉnh thời gian theo mục 10 |
| Khung boss 2 pha | Giữ | Pha 1 là các khoảng trống thường, pha 2 là lõi trắng |
| Cửa hàng | Giữ khung, đổi nội dung | Dùng cho cửa hàng phù thủy và lò rèn |
| Lưu game | Giữ khung, đổi cấu trúc | Tách dữ liệu theo người chơi và theo thế giới, xem mục 14 |
| Bộ test tự động (134–141 test) | Giữ | Test của hệ thống bị tắt thì tắt theo; mỗi hệ thống mới phải có test |
| 9 spec, 36 kỹ năng, mana (RuneEnergy) | Thay | Còn 3 class cố định, bộ động tác có sẵn và sách kỹ năng. Bỏ mana |
| 12 ô trang bị, 41 món đồ, bảng loot | Thay | Còn 1 vũ khí mỗi class và các ô gắn dream stone |
| Chỉ số kiểu MMO (Crit, Haste, Mastery, Versatility) | Thay | Còn 4 chỉ số: Máu, Sức đánh, Nhanh nhẹn, Giữ màu |
| Runestones | Thay | Dream stone dùng lại cấu trúc dữ liệu nếu khớp |
| Module `GolemOnline` (đang trống), plugin Steam đã bật | Viết vào | Phiên chơi Steam, mời bạn, xem mục 13 |
| Luật trộn màu và khoảng trống | Viết mới | Mục 5 |
| Bộ sinh kẻ canh giữ | Viết mới | Mục 7 |
| Sách kỹ năng, DreamCoin | Viết mới | Mục 8 và 11 |
| Tarot | Viết mới | Mục 9 |
| Giờ tự do ban ngày, manh mối, trạng thái khu | Viết mới | Mục 11 |
| Little Princess, nút ra hiệu | Viết mới | Mục 12 |

## 4. Ba Golem: chỉ số và động tác có sẵn

### Chỉ số

Game chỉ có 4 chỉ số. Mỗi chỉ số phải thấy được tác dụng ngay trong trận.

| Chỉ số | Trường dữ liệu | Tác dụng |
| --- | --- | --- |
| Máu | `MaxHealth` | Lượng sát thương chịu được trước khi gục |
| Sức đánh | `Power` | Nhân vào sát thương của mọi đòn đánh và kỹ năng |
| Nhanh nhẹn | `Agility` | Tốc độ di chuyển và tốc độ hồi né, tính theo phần trăm, 100 là chuẩn |
| Giữ màu | `ColorHold` | Số giây cộng thêm vào cửa sổ trộn màu của khoảng trống do Golem này đặt màu đầu tiên |

Chỉ số gốc ở vũ khí bậc 1:

| Golem | Class | Nguyên tố | Vũ khí | Máu | Sức đánh | Nhanh nhẹn | Giữ màu |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Mars | Warrior | Earth, đỏ | Cận chiến nặng | 140 | 12 | 90 | 0 |
| Jupiter | Hunter | Sky, vàng | Cung | 90 | 10 | 110 | 0 |
| Neptune | Mage | Sea, xanh dương | Trượng | 80 | 9 | 100 | 0,5 |

### Động tác có sẵn

Cả ba class đều có bốn động tác, không tốn tài nguyên. Luật trộn màu phải chơi được trọn vẹn chỉ bằng bộ này.

| Động tác | Mô tả | Giá trị khởi điểm |
| --- | --- | --- |
| Đòn nhẹ | Chuỗi 3 đòn. Không đặt màu. | Mỗi đòn gây 1,0 × Sức đánh |
| Đòn nặng | Có thời gian lấy đà. Đây là đòn **đặt màu** của Golem lên khoảng trống. | 2,5 × Sức đánh; lấy đà 0,8 giây (Mars 1,0 giây) |
| Né | Lướt một đoạn ngắn, miễn sát thương trong lúc lướt. | Miễn sát thương 0,35 giây; hồi 1,5 giây ở Nhanh nhẹn 100 |
| Động tác class | Một động tác riêng, có thời gian hồi. | Xem bảng dưới |

| Class | Động tác class | Tác dụng | Hồi |
| --- | --- | --- | --- |
| Warrior | Đỡ | Giữ nút để giảm 80% sát thương từ phía trước, tối đa 3 giây | 6 giây |
| Hunter | Lộn lùi bắn | Lộn lùi 5 m và bắn một phát gây 1,5 × Sức đánh, không đặt màu | 8 giây |
| Mage | Lá chắn ngắn | Tạo lá chắn chặn một đòn cho mọi đồng đội trong bán kính 4 m, tồn tại 2 giây | 12 giây |

Đòn nặng của Mars là cận chiến, của Jupiter và Neptune là đạn bay thẳng tầm 25 m. Đạn phải trúng đúng vùng khoảng trống mới tính là đặt màu.

## 5. Luật trộn màu

Đây là hệ thống quan trọng nhất của game và là thứ cần làm đầu tiên. Mọi hệ thống khác chỉ có nghĩa khi luật này chơi vui.

### Khái niệm

- **Nguyên tố (`EElement`):** `Earth` (đỏ), `Sky` (vàng), `Sea` (xanh dương). Mỗi Golem có đúng một nguyên tố. Nên dùng GameplayTag `Element.Earth`, `Element.Sky`, `Element.Sea`.
- **Khoảng trống (`UColorVoidComponent`):** một vùng va chạm phát sáng gắn trên thân Xenon. Mỗi khoảng trống có một tập nguyên tố cần có (`RequiredElements`), và màu hiển thị suy ra từ tập đó.
- **Thân xám:** mọi phần còn lại của Xenon. Thân xám chỉ nhận 10% sát thương.

| `RequiredElements` | Màu hiển thị | Cần ai |
| --- | --- | --- |
| Earth | Đỏ | Mars |
| Sky | Vàng | Jupiter |
| Sea | Xanh dương | Neptune |
| Earth + Sky | Cam | Mars và Jupiter |
| Sky + Sea | Xanh lá | Jupiter và Neptune |
| Earth + Sea | Tím | Mars và Neptune |
| Earth + Sky + Sea | Trắng sáng | Cả ba |

### Trạng thái của một khoảng trống

| Trạng thái (`EVoidState`) | Ý nghĩa | Hiển thị |
| --- | --- | --- |
| `Hidden` | Chưa lộ ra, không trúng được | Không thấy |
| `Open` | Đang chờ màu | Phát sáng theo màu cần, kèm ký hiệu nguyên tố |
| `Primed` | Đã nhận ít nhất một màu đúng, đang đếm giờ | Phần màu đã có sáng rực, vòng đếm giờ co lại |
| `Dulled` | Vừa bị đánh sai màu, tạm khóa | Xỉn màu, không nhận đòn |
| `Filled` | Đã lấp xong, không đổi nữa | Màu đặc, hết phát sáng |

### Luật chuyển trạng thái

Chỉ những đòn có cờ `bAppliesColor` mới tác động lên khoảng trống: đòn nặng, và một số sách kỹ năng. Khi một đòn như vậy của nguyên tố `E` trúng một khoảng trống đang `Open` hoặc `Primed`:

1. **Nếu `E` không nằm trong `RequiredElements`:** khoảng trống chuyển sang `Dulled` trong 3 giây, xóa mọi màu đã đặt, rồi quay về `Open`.
2. **Nếu `E` đúng và khoảng trống đang `Open`:** ghi nhận `E`, chuyển sang `Primed`, bắt đầu đếm giờ. Thời gian đếm bằng 4 giây cộng với Giữ màu của Golem vừa đặt.
3. **Nếu `E` đúng và khoảng trống đang `Primed`:** ghi nhận thêm `E`. Đặt lại một màu đã có thì không có tác dụng và không làm mới đồng hồ.
4. **Khi đã đủ mọi nguyên tố trong `RequiredElements`:** khoảng trống chuyển sang `Filled`. Khoảng trống một màu thì lấp ngay ở đòn đúng đầu tiên.
5. **Khi đồng hồ chạy hết mà chưa đủ:** xóa màu đã đặt, quay về `Open`. Không bị phạt thêm.

Đòn nhẹ và những đòn không đặt màu trúng khoảng trống thì tính như trúng thân xám.

### Kết quả khi lấp một khoảng trống

- Xenon mất một phần máu bằng `MaxHealth / số khoảng trống`. Lấp hết mọi khoảng trống thì máu về 0.
- Xenon bị choáng 2,5 giây. Kẻ canh giữ chỉ choáng 1,5 giây.
- Khi máu về 0, Xenon chuyển sang trạng thái `Restored`: lấy lại màu, ngừng chiến đấu và đứng yên tới hết sân. Xenon không bị xóa khỏi màn chơi.
- Sát thương vào thân xám cũng có thể đưa máu về 0. Đây là đường lui cho đội thiếu người hoặc phối hợp kém: vẫn thắng được, nhưng chậm hơn khoảng 10 lần.

### Lõi trắng

Lõi trắng cần cả ba nguyên tố và chỉ có ở kẻ canh giữ và ở Xeros. Lõi ở trạng thái `Hidden` cho tới khi mọi khoảng trống khác của mục tiêu đã `Filled`. Cửa sổ đếm giờ của lõi trắng là 5 giây thay vì 4.

### Quyền quyết định và đồng bộ mạng

- Host xác nhận đòn trúng, giữ trạng thái khoảng trống và đồng hồ. Client gửi yêu cầu đánh và nhận trạng thái về.
- Trạng thái khoảng trống được replicate dưới dạng: `State`, `AppliedElements` (bitmask 3 bit), `TimerEndServerTime`.
- Client được phép hiện hiệu ứng trúng đòn ngay, nhưng màu của khoảng trống chỉ đổi khi host xác nhận.
- Hai đòn tới trong cùng một tick thì xử lý theo thứ tự host nhận được.

### Test bắt buộc

Mỗi luật ở trên cần ít nhất một test tự động: sai màu thì `Dulled` đúng 3 giây; đủ màu trong cửa sổ thì `Filled`; hết giờ thì về `Open`; Giữ màu kéo dài đúng số giây; lõi trắng không trúng được khi còn khoảng trống khác; lấp hết thì Xenon chuyển `Restored`.

## 6. Xenon bóng đêm

Xenon bóng đêm cao gấp 2–3 lần Golem. Mỗi sân chỉ có 1–3 con, nên mỗi con phải là một trận đấu nhỏ dài 30–60 giây, có đòn đánh đọc được và có lúc lộ điểm yếu.

### Bốn loại trong bản Early Access

Tên dưới đây là tên làm việc, chưa phải tên chính thức.

| Loại | Hành vi | Khoảng trống | Lúc lộ điểm yếu |
| --- | --- | --- | --- |
| Lính (`Grunt`) | Đánh cận chiến chậm, quét rộng | 1 cái, màu cơ bản, ở ngực | Luôn `Open` |
| Khiên (`Shield`) | Giơ khiên che phía trước, đẩy lùi | 2 cái: một ở khiên, một ở lưng | Cái ở lưng chỉ trúng được từ phía sau |
| Ném (`Thrower`) | Đứng xa ném đá, lùi khi bị áp sát | 1 cái hai màu, ở tay ném | `Open` trong 3 giây sau mỗi lần ném |
| Húc (`Charger`) | Lao thẳng vào một Golem | 1 cái hai màu, ở đầu | `Open` trong 4 giây sau khi húc trúng tường |

### Tinh anh

Tinh anh là một trong bốn loại trên với ba thay đổi: thêm một khoảng trống, mọi khoảng trống đều là màu trộn, và có một khoảng trống đổi `RequiredElements` sau mỗi 10 giây. Kích thước lớn hơn 20%.

### Dữ liệu của một loại Xenon (`FXenonArchetypeRow`)

| Trường | Kiểu | Ý nghĩa |
| --- | --- | --- |
| `ArchetypeId` | Name | Khóa |
| `MaxHealth`, `ContactDamage` | float | Chỉ số gốc ở độ khó 1 |
| `VoidSlots` | mảng | Mỗi phần tử: socket trên skeleton, điều kiện lộ ra, loại màu cho phép (cơ bản, trộn hai, bất kỳ) |
| `Moves` | mảng | Danh sách đòn đánh, mỗi đòn có thời gian báo trước tối thiểu 0,6 giây |
| `StaggerDuration` | float | Thời gian choáng khi bị lấp một khoảng trống |

Màu cụ thể của từng khoảng trống được chọn lúc sinh quái, từ seed của đêm, trong phạm vi `VoidSlots` cho phép. Khi đội thiếu người và không có AI thay thế, bộ sinh không được chọn màu cần Golem vắng mặt.

### Thành phần mỗi sân

| Sân | Độ khó 1 | Độ khó 2 | Độ khó 3 |
| --- | --- | --- | --- |
| Sân 1 | 1 thường | 2 thường | 2 thường |
| Sân 2 | 2 thường | 2 thường | 1 tinh anh, 1 thường |
| Sân 3 | 1 tinh anh | 1 tinh anh, 1 thường | 1 tinh anh, 2 thường |

| Hệ số theo độ khó | Độ khó 1 | Độ khó 2 | Độ khó 3 |
| --- | --- | --- | --- |
| Máu của Xenon | × 1,0 | × 1,3 | × 1,6 |
| Sát thương của Xenon | × 1,0 | × 1,2 | × 1,4 |
| Tỉ lệ khoảng trống màu trộn ở quái thường | 30% | 60% | 100% |

## 7. Kẻ canh giữ

Mỗi khu có một kẻ canh giữ ở cuối đêm. Bản Early Access có 3 loại khung, dùng lại cho 5 khu. Riêng trận ở cung điện là trận gặp Xeros, được dựng cố định và không qua bộ sinh.

### Ba phần được ghép ngẫu nhiên

| Phần | Số lựa chọn | Nội dung |
| --- | --- | --- |
| Bộ màu (`ColorSet`) | 4 | Tập màu của 4 khoảng trống thường. Ví dụ: "cam, cam, tím, đỏ" |
| Bộ chiêu (`MoveSet`) | 6 | Chọn 2 trong 4 chiêu của loại khung đó |
| Vị trí điểm yếu (`WeakLayout`) | 3 | Chọn 1 trong 3 cách đặt khoảng trống trên thân |

Mỗi loại khung có 4 × 6 × 3 = 72 tổ hợp. Lõi trắng luôn có và luôn nằm ở một chỗ cố định của loại khung.

### Khi nào sinh và khi nào đổi

- Kẻ canh giữ của mỗi khu đang mở được sinh ở trạng thái `Morning`, từ `Hash(WorldSeed, DayNumber, DistrictId)`. Cùng seed phải cho cùng kết quả trên mọi máy.
- Kết quả được lưu trong dữ liệu thế giới, để manh mối ban ngày và trận ban đêm khớp nhau.
- Sang ngày mới, kẻ canh giữ của mọi khu đều được sinh lại. Manh mối của ngày cũ hết giá trị.
- Bộ sinh không được ra hai ngày liên tiếp cùng một tổ hợp cho cùng một khu.

### Hai pha của trận

1. **Pha 1:** 4 khoảng trống thường. Kẻ canh giữ dùng 2 chiêu đã chọn. Mỗi khoảng trống được lấp làm nó nhanh hơn 10%.
2. **Pha 2:** khi cả 4 đã `Filled`, lõi trắng lộ ra. Kẻ canh giữ dùng thêm một chiêu diện rộng cố định của loại khung. Lấp lõi trắng là thắng.

### Manh mối ban ngày

Mỗi lần dò manh mối ở một khu, người chơi biết một trong ba phần của kẻ canh giữ khu đó. Thứ tự lộ ra là bộ màu, rồi vị trí điểm yếu, rồi bộ chiêu. Muốn biết cả ba thì tốn hết 3 giờ của ngày và không còn giờ nâng vũ khí.

| Phần đã biết | Người chơi thấy gì ban ngày | Tác dụng trong trận |
| --- | --- | --- |
| Bộ màu | Vệt màu lạ trên chỗ đã phai xám; sổ tay ghi 4 màu | Biết nên mua sách của class nào |
| Vị trí điểm yếu | Hình vẽ trên tường; sổ tay vẽ sơ đồ thân | Khoảng trống được đánh dấu sẵn từ đầu trận |
| Bộ chiêu | Lời kể của một Golem trong khu | Chiêu đó có vệt báo trước dài hơn 0,3 giây |

Không dò manh mối nào thì vẫn đánh được. Khi đó khoảng trống không được đánh dấu sẵn và đội phải tự tìm trong trận.

## 8. Sách kỹ năng

Kỹ năng trong mơ phải học lại mỗi đêm. Mỗi cuốn sách cho dùng một kỹ năng đúng một lần.

### Luật

- Mỗi Golem mang tối đa 4 cuốn. Hai cuốn cùng loại chiếm hai ô.
- Sách mua bằng DreamCoin ở cửa hàng của Princess phù thủy, mở ở `DreamHall` và `RestPoint`.
- Mỗi lần mở, cửa hàng bày 6 cuốn cho mỗi Golem: 3 cuốn của class, 2 cuốn chung, 1 cuốn bất kỳ. Danh sách sinh từ seed của đêm.
- Ở `DreamHall`, mỗi Golem nhận miễn phí một cuốn bậc 1 của class mình.
- Dùng sách là tức thì, không có thời gian hồi chung. Golem đang gục không dùng được sách.
- Sách chưa dùng mất khi tỉnh dậy, không hoàn tiền.
- Host kiểm tra mọi giao dịch mua và mọi lần dùng sách.

### Giá

| Bậc | Giá (DreamCoin) | Số loại |
| --- | --- | --- |
| 1 | 10 | 11 |
| 2 | 25 | 7 |
| 3 | 60 | 4 |

### Danh sách 22 loại sách

Tên là tên làm việc. Cột "Đặt màu" cho biết kỹ năng có cờ `bAppliesColor` hay không.

| # | Tên | Class | Bậc | Tác dụng | Đặt màu |
| --- | --- | --- | --- | --- | --- |
| 1 | Dậm Đất | Warrior | 1 | Sóng chấn bán kính 4 m quanh Mars | Có, mọi khoảng trống trong tầm |
| 2 | Khiêu Khích | Warrior | 1 | Mọi kẻ địch nhắm vào Mars trong 6 giây | Không |
| 3 | Da Đá | Warrior | 1 | Mars giảm 50% sát thương nhận vào trong 8 giây | Không |
| 4 | Lao Tới | Warrior | 2 | Lao 10 m, làm choáng mục tiêu đầu tiên 1,5 giây | Không |
| 5 | Ghìm Chân | Warrior | 2 | Giữ một kẻ địch đứng yên 4 giây | Không |
| 6 | Núi Lở | Warrior | 3 | Đòn lớn gây 5 × Sức đánh; khoảng trống trúng đòn được thêm 4 giây đếm giờ | Có |
| 7 | Mũi Tên Gió | Hunter | 1 | Mũi tên bay 40 m, xuyên qua kẻ địch | Có, khoảng trống đầu tiên trúng |
| 8 | Bẫy Dây | Hunter | 1 | Đặt bẫy làm chậm 50% trong 5 giây | Không |
| 9 | Mắt Đại Bàng | Hunter | 1 | Làm lộ mọi khoảng trống `Hidden` của một kẻ địch trong 10 giây, trừ lõi trắng | Không |
| 10 | Mưa Tên | Hunter | 2 | Vùng bán kính 5 m | Có, mọi khoảng trống trong vùng |
| 11 | Lướt Gió | Hunter | 2 | Né không cần hồi trong 8 giây | Không |
| 12 | Sấm Trời | Hunter | 3 | Sét đánh một mục tiêu, gây 4 × Sức đánh và choáng 2 giây | Có |
| 13 | Bóng Nước | Mage | 1 | Đạn tự tìm khoảng trống gần nhất đang cần Sea | Có |
| 14 | Sương Mù | Mage | 1 | Vùng bán kính 6 m, kẻ địch bên trong đánh chậm 30% trong 6 giây | Không |
| 15 | Giọt Hồi | Mage | 1 | Hồi 30% máu cho một đồng đội | Không |
| 16 | Dòng Chảy | Mage | 2 | Nối hai khoảng trống trong 6 giây: màu đặt lên cái này cũng đặt lên cái kia | Không trực tiếp |
| 17 | Bong Bóng | Mage | 2 | Nhốt một Xenon thường trong 4 giây; không có tác dụng với tinh anh và kẻ canh giữ | Không |
| 18 | Sóng Lớn | Mage | 3 | Sóng quét rộng 8 m, đẩy lùi | Có, mọi khoảng trống trúng sóng |
| 19 | Bình Hồi Sức | Chung | 1 | Hồi 40% máu cho bản thân | Không |
| 20 | Dậy Đi | Chung | 1 | Dựng một đồng đội đang gục dậy ngay, với 30% máu | Không |
| 21 | Giữ Màu | Chung | 2 | Trong 15 giây, mọi cửa sổ trộn màu của đội dài thêm 3 giây | Không |
| 22 | Mượn Màu | Chung | 3 | Đòn nặng kế tiếp đặt màu của một Golem khác do người chơi chọn | Có, màu mượn |

Kỹ năng đặt màu chỉ đặt màu của Golem dùng sách, nên nếu trúng khoảng trống không cần màu đó thì vẫn bị phạt `Dulled` như đòn nặng. Kỹ năng diện rộng vì vậy cần được dùng có tính toán.

### Dữ liệu của một loại sách (`FSkillBookRow`)

| Trường | Kiểu | Ý nghĩa |
| --- | --- | --- |
| `BookId` | Name | Khóa |
| `ClassTag` | GameplayTag | `Class.Warrior`, `Class.Hunter`, `Class.Mage` hoặc `Class.Any` |
| `Tier` | int | 1 đến 3; giá lấy từ bảng giá theo bậc |
| `Ability` | class GameplayAbility | Kỹ năng được cấp khi mua và gỡ sau khi dùng |
| `bAppliesColor` | bool | Có tác động lên khoảng trống hay không |
| `UnlockCondition` | Name | Điều kiện để sách xuất hiện trong cửa hàng; trống là có từ đầu |

## 9. Tarot

Tarot là phần ngẫu nhiên nhẹ của mỗi đêm. Luật giữ đơn giản: nhặt được lá bài, giữ trong túi hoặc bật lên để nhận một lợi thế có thời hạn.

### Luật

- Bài rơi khi một Xenon chuyển sang `Restored`: 35% với quái thường, 100% với tinh anh. Little Princess mang lá bài tới cho một Golem còn ô trống, ưu tiên Golem đang giữ ít bài nhất.
- Mỗi Golem giữ tối đa 3 lá. Khi cả đội đã đầy thì lá mới không rơi.
- Bật một lá thì nhận lợi thế trong 30 giây, sau đó lá bài biến mất. Mỗi Golem chỉ có một lá đang bật tại một thời điểm.
- Lá bài chỉ tác dụng lên Golem bật nó, trừ khi mô tả ghi khác.
- Tỉnh dậy là mất hết bài.
- Bộ bài có 22 lá. Người chơi bắt đầu với 10 lá trong danh sách rơi. Mỗi lần thắng một khu lần đầu mở thêm 2 lá, và 2 lá cuối mở khi thắng một khu bất kỳ ở độ khó 3.

### Mười lá khởi đầu

| Lá | Lợi thế trong 30 giây |
| --- | --- |
| The Fool | Né không cần hồi |
| The Magician | Đòn nặng không cần lấy đà |
| The High Priestess | Nhìn thấy mọi khoảng trống `Hidden` của kẻ địch |
| The Empress | Hồi 2% máu mỗi giây |
| The Emperor | Không bị đẩy lùi, không bị ngắt đòn |
| The Chariot | Di chuyển nhanh hơn 30% |
| Strength | Sức đánh tăng 50% |
| The Hermit | Kẻ địch không chọn Golem này làm mục tiêu |
| The Star | Giữ màu tăng thêm 2 giây |
| The Sun | Đánh sai màu không làm khoảng trống bị `Dulled` |

Mười hai lá còn lại được thiết kế sau khi bản thử core loop chạy được. Về code, mỗi lá là một dòng `FTarotCardRow` gồm `CardId`, một GameplayEffect có thời hạn, và `UnlockCondition`, nên thêm lá mới không cần đổi code.

## 10. Một đêm: sân đấu, gục và kết quả

### Sân đấu

- Giấc mơ dùng lại hình khối của chính khu đó ngoài đời, với bộ ánh sáng và vật liệu của ngàn năm trước. Không dựng map riêng cho giấc mơ.
- Mỗi khu đánh dấu sẵn 5 vị trí sân đấu (`ArenaVolume`). Mỗi đêm chọn 3 trong 5 theo seed và nối chúng thành một đường đi.
- Khi đội vào sân, lối ra vào bị chặn bằng rào bóng đêm. Rào mở khi mọi Xenon trong sân đã `Restored`.
- Trận với kẻ canh giữ diễn ra ở một vị trí cố định của khu.

### Độ khó

Mỗi khu có 3 độ khó. Thắng độ khó n thì mở độ khó n + 1 của khu đó. Host chọn độ khó ở cổng mơ, trong số những độ khó host đã mở. Để mở khu kế tiếp chỉ cần thắng độ khó 1.

### Gục và hồi sinh

| Tình huống | Luật |
| --- | --- |
| Một Golem hết máu | Chuyển sang trạng thái gục, có 20 giây chờ cứu |
| Đồng đội cứu | Đứng cạnh và giữ nút 3 giây; bị đánh trúng thì phải giữ lại từ đầu. Golem dậy với 30% máu |
| Hết 20 giây mà không ai cứu | Golem nghỉ tới hết sân hiện tại, rồi trở lại ở sân sau với 30% máu |
| Cả ba cùng gục, lần đầu trong đêm | Little Princess kéo cả đội dậy với 50% máu; kẻ địch bị đẩy lùi và choáng 2 giây |
| Cả ba cùng gục, lần thứ hai | Thua, chuyển sang `Wake` |

Giữa các sân, máu không tự hồi. Ở `RestPoint`, mọi Golem hồi về ít nhất 50% máu.

### Kết quả đêm

Màn kết quả ở `Wake` hiện: thắng hay thua, số sân đã qua, DreamCoin nhận được, thay đổi trạng thái của khu, những gì mới mở (khu, độ khó, lá tarot, dream stone) và trang sổ tay mới về kẻ canh giữ vừa gặp.

## 11. Ban ngày và kinh tế

### Giờ tự do

Mỗi ngày đội có chung 3 giờ. Mỗi hành động dưới đây tốn 1 giờ, ai trong đội làm cũng trừ vào quỹ chung.

| Hành động | Ở đâu | Kết quả |
| --- | --- | --- |
| Dò manh mối | Điểm manh mối trong một khu đang mở | Lộ một phần của kẻ canh giữ khu đó, ghi vào sổ tay của cả đội |
| Nâng vũ khí | Lò rèn của thầy Boro | Nâng một bậc cho một vũ khí, trả bằng DreamCoin |
| Trò chuyện | NPC có dấu hiệu cốt truyện | Mở một đoạn cốt truyện; một số đoạn mở thêm sách hoặc lá tarot |

Gắn và tháo dream stone, xem sổ tay, đi lại trong thành đều không tốn giờ.

### Trạng thái khu

| Trạng thái (`EDistrictState`) | Độ bão hòa màu | Ý nghĩa |
| --- | --- | --- |
| `Locked` | Xám hoàn toàn | Chưa vào được, chưa mơ được |
| `Normal` | 100% | Khu hoạt động đầy đủ |
| `Weakened` | 60% | Chức năng ban ngày của khu bị giảm |
| `Critical` | 25% | Chức năng ban ngày của khu tạm ngừng |

- Sau màn mở đầu, khu chợ ở `Weakened`, bốn khu còn lại ở `Locked`.
- Thắng một đêm ở khu nào thì khu đó tốt lên một bậc, tối đa tới `Normal`. Lần thắng đầu tiên còn mở khu kế tiếp ở trạng thái `Weakened`.
- Thua một đêm ở khu nào thì khu đó xấu đi một bậc, tối đa tới `Critical`. Khu không bao giờ mất hẳn.
- Thứ tự mở đề xuất: khu chợ, quảng trường, khu lò rèn, bến cảng, cung điện.
- Độ bão hòa đổi qua một tham số vật liệu chung theo từng khu, chuyển mượt trong 3 giây ở `Morning`.

**Chức năng ban ngày của từng khu chưa chốt.** Về code, mỗi khu có một `DayFunction` và hai bộ hệ số cho `Weakened` và `Critical`, tất cả nằm trong dữ liệu. Ví dụ để hình dung: khu lò rèn ở `Weakened` thì nâng vũ khí đắt hơn 25%, ở `Critical` thì lò rèn đóng cửa.

### Vũ khí

Mỗi class có một vũ khí, nâng được 5 bậc, giữ vĩnh viễn.

| Bậc | Giá nâng lên bậc này (DreamCoin) | Sức đánh | Máu | Số ô dream stone |
| --- | --- | --- | --- | --- |
| 1 | Có sẵn | 100% | 100% | 1 |
| 2 | 80 | 110% | 108% | 1 |
| 3 | 160 | 120% | 116% | 2 |
| 4 | 280 | 130% | 124% | 2 |
| 5 | 450 | 140% | 132% | 3 |

Nâng một vũ khí từ bậc 1 lên bậc 5 tốn tổng cộng 970 DreamCoin. Đề xuất: trước khi khu lò rèn mở, thầy Boro rèn tạm ở khu chợ và chỉ nâng được tới bậc 3; bậc 4 và 5 cần khu lò rèn.

### Dream stone

Dream stone gắn vào ô trên vũ khí và tăng chỉ số cố định. Có 15 loại, chia 4 nhóm.

| Nhóm | Số loại | Tác dụng | Giá mua |
| --- | --- | --- | --- |
| Đơn nhỏ | 4 | Một chỉ số +8% (Giữ màu: +0,4 giây) | 120 |
| Đôi | 6 | Hai chỉ số, mỗi cái +8% (Giữ màu: +0,4 giây); đủ 6 cặp từ 4 chỉ số | 200 |
| Đơn lớn | 4 | Một chỉ số +15% (Giữ màu: +0,8 giây) | 260 |
| Đặc biệt | 1 | Máu, Sức đánh, Nhanh nhẹn +5%, Giữ màu +0,3 giây | Không bán |

- **Nguồn:** mỗi lần thắng lần đầu một khu ở một độ khó, mỗi Golem nhận một viên ngẫu nhiên mình chưa có. Princess phù thủy bán 2 viên mỗi đêm ở `DreamHall`. Viên đặc biệt chỉ rơi từ kẻ canh giữ ở độ khó 3.
- **Gắn:** mỗi viên thuộc về một Golem. Một vũ khí không gắn hai viên cùng loại. Tháo lắp miễn phí vào ban ngày, không đổi được trong mơ.
- **Cách cộng:** các phần trăm cộng vào nhau rồi nhân với chỉ số sau khi đã tính bậc vũ khí.

### DreamCoin

DreamCoin là loại tiền duy nhất. Phần thưởng tính theo từng Golem.

| Nguồn | DreamCoin cho mỗi Golem, ở độ khó 1 |
| --- | --- |
| Bước qua cổng mơ | 15 |
| Qua mỗi sân đấu | 7 |
| Thắng kẻ canh giữ | 70 |

| Kết quả đêm | DreamCoin | So với một bộ sách trung bình (70) |
| --- | --- | --- |
| Thắng | 15 + 21 + 70 = 106 | 1,5 lần |
| Thua ở kẻ canh giữ | 15 + 21 = 36 | 0,5 lần |
| Thua ở sân 1 | 15 | 0,2 lần |

- **Bộ sách trung bình** là 2 cuốn bậc 1 và 2 cuốn bậc 2: 10 + 10 + 25 + 25 = 70 DreamCoin.
- **Độ khó** nhân phần thưởng: × 1,0, × 1,25, × 1,5.
- **Ví tiền** thuộc về người chơi. Người chơi nhận phần của Golem mình điều khiển. Phần của Golem do AI điều khiển về ví của host, vì host trả tiền sách và nâng vũ khí cho Golem đó.
- **Chỗ tiêu:** sách kỹ năng (mất sau đêm), nâng vũ khí và mua dream stone (giữ vĩnh viễn).

Với các số này, một Golem thắng đều và mua bộ sách trung bình mỗi đêm sẽ dư 36 DreamCoin một đêm, tức cần khoảng 27 đêm để nâng vũ khí lên bậc 5; không mua sách thì cần khoảng 9 đêm. Khoảng cách này là lựa chọn chính của người chơi, và là thứ cần đo ở bản thử core loop.

## 12. Little Princess, AI đồng đội và nút ra hiệu

### Nút ra hiệu

Người chơi nhắm vào một khoảng trống và bấm nút ra hiệu. Host tạo một dấu hiệu trên khoảng trống đó, mọi người chơi đều thấy, tồn tại 6 giây. Dấu hiệu hiện ký hiệu của những nguyên tố còn thiếu. Đây vừa là cách gọi AI khi chơi đơn, vừa là cách nói chuyện nhanh khi chơi co-op không dùng mic.

### AI đồng đội

Dùng lại AI đồng đội đang có và thêm các hành vi sau, theo thứ tự ưu tiên từ trên xuống:

1. **Cứu đồng đội đang gục** nếu không có người chơi nào đang cứu.
2. **Đáp lại dấu ra hiệu:** nếu khoảng trống được ra hiệu cần màu của mình, AI đánh đòn nặng vào đó sau 0,5–1,0 giây.
3. **Tự bồi màu:** nếu một khoảng trống đang `Primed` và còn thiếu màu của mình, AI đánh đòn nặng vào đó sau 1,5 giây.
4. **Đánh thường:** dùng đòn nhẹ vào thân xám, né những đòn có vệt báo trước.

Ba ràng buộc:

- AI không bao giờ đánh đòn đặt màu vào khoảng trống không cần màu của mình.
- AI không tự mở đầu một khoảng trống màu trộn. Người chơi phải là người quyết định đánh vào đâu, để chơi đơn vẫn có cảm giác chỉ huy.
- Về sách, AI chỉ tự dùng sách hồi máu và sách cứu đồng đội. Những sách khác AI chỉ dùng khi người chơi ra hiệu hai lần liên tiếp vào cùng một mục tiêu.

### Little Princess

Little Princess là một actor không tham chiến: không có máu, không bị nhắm, không chặn đường. Mọi tác động của cô lên trận đấu do host kích hoạt.

| Lúc | Hành vi | Giới hạn |
| --- | --- | --- |
| Sáng | Gợi ý một khu nên mơ đêm nay: khu đang ở trạng thái xấu nhất, nếu bằng nhau thì khu mới mở nhất | Chỉ là gợi ý, người chơi tự chọn |
| Ngày | Bay theo host, chỉ về điểm manh mối và NPC có cốt truyện | Không có |
| Cổng mơ | Dẫn đội tới khu đã chọn và làm phép mở cổng | Cần mọi người chơi đứng trong vòng phép |
| Trong trận | Mang lá tarot vừa rơi tới cho một Golem | Theo luật rơi bài ở mục 9 |
| Trong trận | **Pha màu:** khi một khoảng trống `Primed` chỉ còn thiếu đúng một màu và đồng hồ còn dưới 1 giây, cô đặt màu còn thiếu | 1 lần mỗi sân và 1 lần trong trận kẻ canh giữ; không dùng cho lõi trắng |
| Trong trận | **Kéo đội dậy** khi cả ba cùng gục | 1 lần mỗi đêm |
| Trong trận | Nhắc bằng lời khi đội đánh sai màu 2 lần liên tiếp vào cùng một khoảng trống | Không quá 1 lần mỗi 20 giây |

## 13. Co-op và mạng

### Mô hình

- **Listen server:** một người chơi làm host, không có dedicated server. Tối đa 3 người.
- **Phiên chơi qua Steam:** tạo phòng, mời bạn, vào phòng qua danh sách bạn bè Steam. Code đặt trong module `GolemOnline`. Bản Early Access chưa có ghép ngẫu nhiên với người lạ.
- **Số Golem luôn là 3.** Chỗ trống do AI điều khiển. Khi vào phòng, người chơi chọn một Golem chưa có người điều khiển.

### Ai giữ dữ liệu gì

| Dữ liệu | Thuộc về | Ghi chú |
| --- | --- | --- |
| Ngày thứ mấy, trạng thái các khu, khu đã mở, cờ cốt truyện, seed | Host | Khách chơi trong thế giới của host |
| Ví DreamCoin, bậc vũ khí, dream stone, lá tarot đã mở, sổ tay | Từng người chơi | Mang theo khi sang phòng khác |
| Sách, bài tarot đang giữ, máu, trạng thái khoảng trống | Host, trong đêm | Mất khi tỉnh dậy |

Khách thắng một khu trong thế giới của host thì nhận DreamCoin, dream stone và trang sổ tay, nhưng thế giới riêng của khách không đổi.

### Rời phòng và mất kết nối

- **Khách rời giữa đêm:** AI nhận Golem đó ngay, giữ nguyên máu, sách và bài. Khách nhận DreamCoin tính tới sân cuối cùng đã qua. Khách có thể vào lại ở `RestPoint`.
- **Host rời:** đêm kết thúc với mọi người. Khách nhận DreamCoin tính tới sân cuối cùng đã qua. Bản Early Access không chuyển quyền host.

### Những thứ cần đồng bộ

- `EGamePhase` và thời điểm chuyển trạng thái.
- Seed của đêm, gửi một lần ở `DreamGate`. Client tự dựng kẻ canh giữ và danh sách cửa hàng từ seed; host vẫn là bên kiểm tra.
- Trạng thái khoảng trống, theo mục 5.
- Túi sách, túi bài, lá bài đang bật của từng Golem.
- Dấu ra hiệu.
- Số giờ tự do còn lại và manh mối đã dò.

Mỗi sân chỉ có 3 Golem và tối đa 3 Xenon, nên băng thông không phải vấn đề. Chỗ cần để ý là độ trễ: client phải thấy đòn của mình ra ngay, và cửa sổ 4 giây phải tính theo đồng hồ của host. Mục tiêu là chơi được bình thường ở độ trễ 150 ms.

## 14. Lưu dữ liệu

| File | Nội dung | Lưu khi nào |
| --- | --- | --- |
| `PlayerSave` | `DreamCoin`, `WeaponLevel[3]`, `OwnedStones[3]`, `SocketedStones[3]`, `UnlockedTarot`, `UnlockedBooks`, `Notebook`, cài đặt | Sau mỗi giao dịch ban ngày, và ở `Wake` |
| `WorldSave` | `WorldSeed`, `DayNumber`, `DistrictState[5]`, `ClearedDifficulty[5]`, `StoryFlags`, `HoursLeft`, `TodayGuardians`, `CluesFound`, `bNightInProgress`, `NightEarnings` | Ở `Morning`, sau mỗi hành động ban ngày, ở `DreamGate`, sau mỗi sân, và ở `Wake` |

- Mảng `[3]` là theo ba Golem: mỗi người chơi có tiến trình riêng cho cả Mars, Jupiter và Neptune, vì họ có thể chơi Golem khác nhau ở mỗi phòng.
- Không lưu trạng thái giữa một sân đấu. Thoát giữa đêm được xử lý bằng cờ `bNightInProgress` như ở mục 2.
- Cả hai file đồng bộ qua Steam Cloud. Mỗi file có số phiên bản để chuyển đổi khi cấu trúc thay đổi trong giai đoạn Early Access.
- Game co-op PvE nên không cần chống gian lận ở file lưu.

## 15. Giao diện

| Màn hình | Nội dung | Ghi chú |
| --- | --- | --- |
| HUD trong mơ | Máu của ba Golem, 4 ô sách, 3 ô tarot, thời gian hồi của né và động tác class | Không có thanh máu của Xenon; tiến độ đọc qua số khoảng trống đã lấp |
| Chỉ báo khoảng trống | Màu, ký hiệu nguyên tố còn thiếu, vòng đếm giờ | Gắn trong thế giới 3D, trên chính khoảng trống |
| Dấu ra hiệu | Vị trí và ký hiệu nguyên tố đang cần | Có mũi tên ở mép màn hình khi mục tiêu ngoài khung nhìn |
| Cửa hàng phù thủy | 6 cuốn sách, 2 dream stone, ví tiền, túi sách hiện tại | Chơi đơn: chuyển qua lại giữa ba Golem để mua |
| Lò rèn | Bậc vũ khí, giá nâng, các ô dream stone | Dùng chung cho gắn đá |
| Sổ tay | Manh mối của hôm nay, những kẻ canh giữ đã gặp, các đoạn cốt truyện đã mở | Manh mối của ngày cũ chuyển sang mục "đã qua" |
| Bản đồ thành | 5 khu, trạng thái từng khu, độ khó đã thắng, khu Little Princess gợi ý |  |
| Giờ tự do | 3 ô giờ ở góc màn hình, tắt dần khi dùng | Chỉ hiện ban ngày |
| Kết quả đêm | Theo mục 10 |  |
| Phòng chơi | Tạo phòng, mời bạn Steam, chọn Golem, trạng thái sẵn sàng |  |

**Ký hiệu nguyên tố** để người mù màu chơi được: Earth là hình tam giác, Sky là hình tròn, Sea là hình sóng. Khoảng trống màu trộn hiện cả hai ký hiệu cạnh nhau, lõi trắng hiện cả ba. Ký hiệu phải luôn hiện, không nằm sau một tùy chọn. Hình dạng cuối cùng do art quyết định.

Game hỗ trợ tay cầm và chuột-phím từ đầu. Mọi chữ lấy từ bảng localization.

## 16. Mốc cốt truyện cần code

| Mốc | Khi nào | Cần gì về code |
| --- | --- | --- |
| Mở đầu trong mơ | Lần đầu vào game | Một đêm dựng sẵn, không qua bộ sinh: dạy đòn nhẹ, đòn nặng, né, rồi một Xenon có khoảng trống một màu. Sau đó Xeros xuất hiện và đội chắc chắn thua sau 60–90 giây. Không có Little Princess kéo dậy, không trả DreamCoin |
| Tỉnh dậy lần đầu | Ngay sau mở đầu | Little Princess xuất hiện dưới dạng tinh linh. Ngày đầu tiên được dẫn từng bước: một lần dò manh mối, một lần trò chuyện, rồi cổng mơ |
| Đêm đầu ở khu chợ | Đêm 1 | Dạy khoảng trống màu trộn ở sân 1, cửa hàng phù thủy và cuốn sách miễn phí ở `DreamHall`, lá tarot đầu tiên luôn rơi ở sân 1 |
| Thắng lần đầu mỗi khu | 4 khu đầu | Một đoạn cảnh kể một phần nguồn gốc của Lithea, theo đúng thứ tự ở GAME_DESIGN.md |
| Gặp Xeros | Đêm ở cung điện | Trận dựng sẵn thay cho kẻ canh giữ. Thắng thì Little Princess được cứu |
| Kết bản Early Access | Ngay sau trận Xeros | Đoạn cảnh: một Xeros phiên bản Fey xuất hiện. Sau đó người chơi vẫn chơi tiếp được các độ khó 2 và 3 |

Mỗi mốc là một cờ trong `StoryFlags`. Đoạn cảnh chạy trên máy host và phát cho mọi client; host được bỏ qua, khách thì không.

## 17. Thứ tự làm và tiêu chí nghiệm thu

Thứ tự dưới đây đặt hai bước kiểm chứng lên trước, để biết game có vui không trước khi đầu tư vào nội dung. Ngưỡng đạt là đề xuất.

| Mốc | Nội dung | Đạt khi |
| --- | --- | --- |
| **M0. Luật trộn màu** | Một sân đấu hình khối xám, ba Golem dùng Mannequin với bộ động tác có sẵn, một Xenon loại Lính và một loại Khiên, mục 5 đầy đủ, nút ra hiệu, AI bồi màu, chơi 3 người qua listen server | 5 nhóm ba người chơi 15 phút. Ít nhất 4 nhóm tự hiểu luật trong 5 phút đầu mà không cần giải thích, và ít nhất 4 nhóm muốn chơi tiếp |
| **M1. Core loop** | Một khu, đủ 8 trạng thái ở mục 2, một loại khung kẻ canh giữ có bộ sinh, 3 giờ tự do và manh mối, 6 lá tarot, 11 sách bậc 1, DreamCoin, nâng vũ khí tới bậc 3, lưu game | 5 người, mỗi người chơi 5 ngày-đêm. Ít nhất 3 người có dò manh mối từ ngày thứ 3 trở đi mà không bị nhắc, và ít nhất 3 người muốn chơi ngày thứ 6 |
| **M2. Một khu hoàn chỉnh** | Khu chợ với art thật, ba Golem và Little Princess có model, phiên chơi qua Steam, giao diện đầy đủ, màn mở đầu | Ba người ở ba nơi khác nhau chơi trọn 3 ngày-đêm qua Steam không lỗi chặn; dùng được làm trailer và trang Steam |
| **M3. Đủ nội dung** | 5 khu, 3 loại khung kẻ canh giữ, 4 loại Xenon và tinh anh, 22 sách, 15 dream stone, 22 lá tarot, 3 độ khó, trận Xeros | Chơi được từ mở đầu tới kết bản Early Access |
| **M4. Phát hành Early Access** | Cân bằng, ba ngôn ngữ, Steam Cloud, thành tựu, tối ưu, sửa lỗi | Không còn lỗi chặn; một người chơi mới đi hết bản Early Access mà không cần hỗ trợ |

Hai nguyên tắc khi làm:

- **Không làm M2 khi M0 và M1 chưa đạt.** Nếu luật trộn màu không vui ở M0, sửa luật trước; thêm nội dung không cứu được nó.
- **M0 và M1 không cần art.** Art làm song song theo tài liệu art, và ghép vào từ M2.

## 18. Những điểm còn mở

Các điểm dưới đây đang dùng đề xuất của tài liệu này. Code nên viết sao cho đổi chúng chỉ là đổi dữ liệu.

1. **Chức năng ban ngày của từng khu và hậu quả khi phai màu.** Chưa chốt, đang theo concept.
2. **Quỹ 3 giờ dùng chung khi chơi co-op.** Lựa chọn khác là mỗi người chơi có 3 giờ riêng, khi đó đội ba người mạnh hơn chơi đơn rõ rệt.
3. **Lò rèn tạm ở khu chợ.** Cần vì khu lò rèn mở thứ ba, trong khi nâng vũ khí phải có từ ngày đầu.
4. **Đổi Golem khi chơi đơn.** Bản này người chơi đơn giữ một Golem suốt đêm và chỉ huy hai Golem còn lại bằng nút ra hiệu.
5. **Little Princess pha màu tự động.** Lựa chọn khác là người chơi tự bấm để gọi cô.
6. **Thiết kế trận Xeros** và **12 lá tarot còn lại.** Làm sau M1.
7. **Tên chính thức** của các loại Xenon, kẻ canh giữ, sách và dream stone. Tài liệu đang dùng tên làm việc.
8. **Mini game ban ngày** cho phần thưởng mang vào giấc mơ. Để dành, chưa làm ở bản này.
