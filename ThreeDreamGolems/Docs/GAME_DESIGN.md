<!-- Bản sao của tab Overview trong tài liệu Three Dream Golems — Game Design (2026-11-13). Khi thiết kế đổi, sửa tài liệu gốc trước rồi cập nhật file này. -->

# Three Dream Golems — Game design

Three Dream Golems là game co-op hành động cho 1–3 người: ban ngày ba Golem học trò dò la và chuẩn bị trong thành, ban đêm cả ba trở về trận chiến ngàn năm trước trong giấc mơ để giữ cho thế giới hiện tại khỏi phai màu.

## Game trong một trang

| Hạng mục | Nội dung |
| --- | --- |
| Thể loại | Co-op action roguelite, góc nhìn thứ ba |
| Số người chơi | 1–3 online qua Steam; chơi đơn có 2 đồng đội AI |
| Một lượt chơi | Một ngày đêm: ban ngày 2–4 phút, ban đêm 15–20 phút |
| Nhân vật điều khiển | Ba Golem tí hon, mỗi Golem một class cố định, một nguyên tố và một màu: Mars là Warrior (Earth, đỏ), Jupiter là Hunter (Sky, vàng), Neptune là Mage (Sea, xanh dương) |
| Nhân vật biểu tượng | Little Princess, người ban sự sống, dẫn đường và hỗ trợ trong trận |
| Phản diện | Xeros, Xenon bị bỏ quên và bị bóng đêm lấp đầy |
| Bối cảnh | Thành Golem trên hành tinh Lithea |
| Giọng điệu | Ấm, hài, dễ thương, phiêu lưu |
| Nền tảng | PC (Steam), Unreal Engine 5 |
| Ngôn ngữ | Anh, Việt, Trung giản thể |
| Giá bán | 14,99 USD ở Early Access, 19,99 USD khi ra bản chính thức |

## Ý tưởng

Người chơi sống hai cuộc đời của cùng ba nhân vật: học trò vụng về lúc thức, chiến binh cổ đại trong mơ. Ba điều làm game khác với một game hành động co-op thông thường:

1. **Thua trong quá khứ thì hiện tại hỏng dần.** Giấc mơ là trận chiến ngàn năm trước đang diễn ra lại. Mỗi khu để mất trong mơ làm khu đó ngoài đời phai màu và lịm dần.
2. **Phải hiểu boss trước khi đánh.** Boss mỗi đêm một khác. Ban ngày là lúc đi dò manh mối và chuẩn bị; ngủ rồi lao vào đánh thì khó hơn nhiều.
3. **Một mình không thắng nổi.** Ba Golem nhỏ hơn hẳn kẻ địch. Muốn thắng, các Golem phải trộn đúng màu nguyên tố của mình lên điểm yếu của quái; đánh bừa thì thua.

## Cốt truyện và nhân vật

Thuở sơ khai, các Little Princess được giao nhiệm vụ ban sự sống cho các hành tinh. Ở hành tinh Lithea, một Little Princess nặn nên những Golem xám từ bột vũ trụ, rồi trao cho chúng ba nguyên tố để chúng sống và xây dựng thế giới. Một kẻ bị bỏ quên, và bóng đêm đã lấp vào chỗ trống trong hắn.

### Nguồn gốc

1. Little Princess dùng bột vũ trụ nặn nên các **Xenon**: những Golem xám xịt, không có linh hồn.
2. Cô thả vào các Xenon ba nguyên tố Earth, Sky và Sea. Chúng có sự sống, có màu, và bắt đầu xây dựng thế giới.
3. Tới lượt Xenon cuối cùng thì cô đã hết nguyên tố. Hắn bị bỏ quên, và khoảng trống trong tâm trí hắn bị bóng đêm lấp đầy. Đó là **Xeros**, kẻ tin rằng bóng đêm là nguồn gốc của vạn vật và muốn nuốt chửng tất cả.
4. Ngàn năm trước, Xeros truyền bóng đêm cho những Xenon chưa kịp nhận nguyên tố và kéo quân đánh tòa thành. Ba Golem đầu tiên, mỗi kẻ mang trọn một nguyên tố thuần khiết, đã thắng và giam hắn vào nơi bóng đêm trú ngụ: giấc ngủ.
5. Giờ đây, từ trong giấc ngủ, Xeros kéo trận chiến xưa vào giấc mơ và đánh lại. Lần này hắn thắng, Little Princess bị bắt, và hiện tại bắt đầu phai màu.

### Mở đầu game

1. Game bắt đầu ngay trong mơ: ba Golem giữ thành trước đạo quân xám của Xeros. Trận này không thể thắng, và Little Princess bị bắt đi.
2. Ba Golem học trò tỉnh dậy. Chỉ là giấc mơ, nhưng cả ba cùng mơ y hệt nhau, và Little Princess vẫn ở đó dưới dạng một tinh linh chỉ ba đứa nhìn thấy.
3. Một góc thành đã mất màu, và người sống ở đó lờ đờ như sắp trở lại thành Xenon. Little Princess giải thích: trận đêm qua là trận chiến ngàn năm trước, và kết cục của nó đang đổi hiện tại.
4. Muốn giữ cuộc sống như bây giờ, ba Golem phải quay lại giấc mơ, giành lại từng khu của thành và tìm cách cứu cô.

### Nhân vật

| Nhân vật | Là ai | Vai trò trong game |
| --- | --- | --- |
| Ba Golem | Mars, Jupiter và Neptune: ba học trò lò rèn ở Thành Golem, nhỏ bé cả lúc thức lẫn trong mơ. Chúng là ba đứa hiếm hoi sinh ra với một nguyên tố thuần khiết, giống ba Golem đầu tiên | Nhân vật điều khiển; mỗi Golem một class cố định, một nguyên tố và một màu |
| Little Princess | Người ban sự sống cho Lithea, một trong nhiều Little Princess của vũ trụ. Ở hiện tại cô chỉ còn là một tinh linh nhỏ, nhớ lõm bõm | Người dẫn đường ban ngày; hỗ trợ trong trận ban đêm; gương mặt của game |
| Xeros | Xenon bị bỏ quên vì Little Princess đã hết nguyên tố. Bóng đêm lấp vào chỗ trống trong hắn | Phản diện chính |
| Xenon bóng đêm | Những Xenon xám đã nhận bóng đêm của Xeros, to gấp 2–3 lần ba Golem | Quái thường và kẻ canh giữ từng khu |
| Golem lai màu | Dân trong thành. Qua nhiều thế hệ, ba nguyên tố hòa lẫn nên họ có đủ màu cam, xanh lá, tím, nâu | Người dân, người bán hàng, nguồn manh mối ban ngày |
| Princess phù thủy | Một Princess sống trong giấc mơ, giữ những cuốn sách kỹ năng chỉ dùng được ở đó | Chủ cửa hàng trong mơ: bán sách kỹ năng và dream stone lấy DreamCoin |
| Thầy Boro | Thợ rèn già, thầy của ba Golem | Nâng vũ khí ban ngày |

Đánh thắng một Xenon bóng đêm là trao cho nó nguyên tố nó còn thiếu, không phải phá hủy nó. Cái kết của cả câu chuyện đi theo đúng luật đó: ba Golem cùng trao cho Xeros ba nguyên tố hắn chưa bao giờ nhận được.

Bản Early Access chỉ kể phần diễn ra trong Thành Golem. Nó kết thúc ở cung điện: ba Golem gặp Xeros lần đầu và cứu được Little Princess, nhưng ngay sau đó một Xeros phiên bản Fey xuất hiện, mở ra phần tiếp theo. Những vùng đất khác của Lithea chưa được nhắc tới; chúng, cùng tộc Fey và các Little Princess khác, để dành cho các bản sau.

## Core loop

Một lượt chơi là một ngày đêm dài khoảng 20–25 phút, gồm bốn bước lặp lại.

1. **Sáng, khoảng 30 giây.** Người chơi thấy thành phố đổi khác theo kết quả đêm qua. Little Princess gợi ý khu đang cần giúp nhất.
2. **Ngày, 2–4 phút.** Mỗi ngày có 3 giờ rảnh, mỗi việc tốn một giờ: dò một manh mối về boss, nâng vũ khí ở lò rèn, hoặc trò chuyện để mở cốt truyện. Không đủ giờ để làm hết, nên mỗi ngày là một lần chọn ưu tiên.
3. **Đêm, 15–20 phút.** Ba Golem theo Little Princess tới khu đã chọn; cô làm phép mở cánh cổng giấc mơ và cả ba bước vào. Trong mơ, đội mua sách kỹ năng ở cửa hàng của Princess phù thủy, qua 3 sân đấu, rồi đánh kẻ canh giữ của khu.
4. **Tỉnh dậy, khoảng 30 giây.** Thắng thì khu đó lấy lại màu ngoài đời và mở khu kế tiếp. Thắng hay thua, đội đều nhận DreamCoin và sổ tay ghi thêm hiểu biết về boss.

### Vì sao người chơi quay lại

- **Đêm nào cũng khác:** boss đổi bộ màu, chiêu và vị trí điểm yếu; bài nhặt được cũng khác.
- **Kiến thức tích lũy:** càng chơi lâu càng đọc manh mối nhanh.
- **Thành phố thay đổi nhìn thấy được:** màu trở về từng khu, lò rèn đỏ lửa, chợ đông lên.

## Một đêm trong mơ

Ban đêm là phần hành động của game. Ba Golem tí hon đối đầu những Xenon bóng đêm to gấp 2–3 lần mình: một mình thì không thắng nổi, phối hợp đúng màu thì thắng.

### Luật trộn màu

Mỗi Golem có một class cố định, mang một nguyên tố và một màu cơ bản. Xenon bóng đêm có thân xám gần như không thể làm bị thương; điểm yếu của chúng là những khoảng trống phát sáng trên thân, mang màu của nguyên tố chúng đang thiếu.

| Golem | Class | Nguyên tố | Màu | Tầm đánh | Việc chính trong trận |
| --- | --- | --- | --- | --- | --- |
| Mars | Warrior | Earth | Đỏ | Cận chiến | Đứng trước mặt quái, đỡ và khóa đòn, giữ nó quay về phía mình |
| Jupiter | Hunter | Sky | Vàng | Tầm xa, cơ động | Đưa màu tới khoảng trống trên cao hoặc sau lưng, đặt bẫy |
| Neptune | Mage | Sea | Xanh dương | Tầm trung, diện rộng | Phủ màu lên nhiều khoảng trống cùng lúc, giữ màu lâu hơn, hỗ trợ đồng đội |

| Màu của khoảng trống | Cần ai cùng đánh | Gặp ở đâu |
| --- | --- | --- |
| Đỏ, vàng hoặc xanh dương | Một Golem đúng màu | Quái yếu, dùng để dạy luật |
| Cam | Mars và Jupiter | Quái thường và tinh anh |
| Xanh lá | Jupiter và Neptune | Quái thường và tinh anh |
| Tím | Mars và Neptune | Quái thường và tinh anh |
| Trắng sáng | Cả ba, cùng lúc | Lõi của kẻ canh giữ và của Xeros |

1. Nhìn màu của khoảng trống để biết cần những Golem nào.
2. Các Golem đúng màu đánh đòn nặng vào đó trong cùng một khoảng 4 giây.
3. Màu khớp thì khoảng trống được lấp đầy: quái mất nhiều máu và khựng lại.
4. Đánh sai màu thì khoảng trống xỉn đi và phải chờ mới sáng lại. Đánh bừa vì vậy có hại chứ không chỉ vô ích.
5. Lấp đủ mọi khoảng trống thì Xenon nhận được sự sống: nó bừng lên màu sắc và thôi chiến đấu.

Golem đứng ngoài cặp đang phối hợp có việc riêng: thu hút con quái, che cho hai bạn, hoặc lo một khoảng trống khác.

### Các phần còn lại của một trận

- **Động tác có sẵn:** đòn nhẹ, đòn nặng để đặt màu, né, và một động tác riêng của class. Luật trộn màu chạy hoàn toàn trên bộ này, nên đội không có đồng nào vẫn đánh được.
- **Sách kỹ năng:** kỹ năng trong mơ phải học lại mỗi đêm. Đội mua sách bằng DreamCoin ở cửa hàng của một Princess phù thủy; mỗi cuốn cho dùng một kỹ năng một lần, mỗi Golem mang tối đa 4 cuốn, và sách chưa dùng sẽ mất khi tỉnh dậy.
- **Vũ khí và dream stone:** mỗi class một vũ khí nâng được 5 bậc. Dream stone gắn lên vũ khí, mỗi viên tăng một hoặc hai chỉ số. Cả hai giữ vĩnh viễn.
- **DreamCoin:** loại tiền duy nhất, nhận sau mỗi đêm dù thắng hay thua. Người chơi cân giữa mua sách để mạnh ngay đêm nay và để dành nâng vũ khí, mua dream stone.
- **Số quái:** mỗi sân đấu chỉ có 1–3 con, mỗi con là một trận đấu nhỏ dài 30–60 giây, phải lấp lần lượt 3–4 khoảng trống.
- **Little Princess:** bay theo đội, trao bài tarot, pha vào màu còn thiếu một lần mỗi trận, và mỗi đêm một lần kéo cả đội lại khi cả ba cùng gục.
- **Bài tarot:** những lá bài tinh tú, nhặt ngẫu nhiên khi hạ quái. Mỗi Golem giữ tối đa 3 lá, bật một lá để nhận một lợi thế trong 30 giây. Bài chỉ có trong mơ, tỉnh dậy là mất.
- **Kẻ canh giữ:** mỗi đêm mang một bộ màu, bộ chiêu và vị trí điểm yếu khác nhau. Biết trước thì lợi thế rõ rệt; đánh mù vẫn thắng được nhưng khó hơn nhiều.
- **Chơi đơn:** người chơi nhắm vào một khoảng trống và bấm nút ra hiệu, Golem AI có màu phù hợp sẽ cùng đánh.
- **Đọc được mà khỏi cần màu:** mỗi nguyên tố có thêm một ký hiệu riêng, để người mù màu vẫn chơi được.
- **Thua:** cả ba gục thì tỉnh dậy. Đêm sau có thể mơ lại khu đó, với một kẻ canh giữ đã đổi khác.

## Ngày và đêm ảnh hưởng lẫn nhau

Mỗi khu trong mơ là một trận của cuộc chiến xưa. Khu nào rơi vào tay bóng đêm thì ngoài đời khu đó phai màu, nên người chơi luôn nhìn thấy lý do để quay lại giấc mơ.

| Khu trong mơ | Khi đang thua, ngoài đời | Ảnh hưởng tới người chơi |
| --- | --- | --- |
| Quảng trường | Đài phun nước và quảng trường xám lại, người qua lại thưa dần | Ít người để hỏi manh mối |
| Khu lò rèn | Lửa lò mất màu và nguội dần | Không nâng cấp vũ khí được |
| Khu chợ | Hàng hóa bạc màu, người bán lờ đờ, sạp đóng dần | Sách kỹ năng trong mơ đắt hơn |
| Bến cảng | Biển và trời ở cảng xám xịt | Không mở được lá bài tarot mới |
| Cung điện | Little Princess mờ dần | Cô gợi ý ít dần và hỗ trợ yếu đi |

Mỗi khu có ba mức: bình thường, suy yếu, nguy kịch. Không khu nào mất hẳn, và thắng một đêm là hồi một mức, nên người chơi yếu không bị dồn vào đường cùng.

Chiều ngược lại, ban ngày quyết định đêm. Manh mối là những vệt màu lạ còn sót lại trên các chỗ đã xám, cho biết đêm nay cặp Golem nào phải phối hợp nhiều nhất. Thành phố càng nhiều màu thì Little Princess hỗ trợ càng mạnh.

## Thế giới

Bản Early Access diễn ra trọn trong một nơi: Thành Golem trên hành tinh Lithea. Lúc thức, thành là nơi sinh hoạt ban ngày; trong mơ, năm khu của nó là năm chặng của trận chiến xưa. Thành rộng khoảng 290 × 220 m, tổng chênh cao 8–12 m.

| Thứ tự đề xuất | Khu | Đặc điểm |
| --- | --- | --- |
| 1 | Khu chợ | Gần Cổng Bắc, nhiều sạp hàng và vật cản; khu để học luật |
| 2 | Quảng trường | Sân tròn rộng quanh đài phun nước, trung tâm của thành |
| 3 | Khu lò rèn | Xưởng, lò và ống khói; địa hình nhiều tầng |
| 4 | Bến cảng | Cầu tàu và kho hàng ngoài tường thành, sát biển |
| 5 | Cung điện | Vườn và sân trước cung điện; chặng cuối của bản Early Access |

Mỗi đêm diễn ra trong một khu. Thắng kẻ canh giữ thì mở khu kế tiếp, và khu đã qua vẫn quay lại được ở độ khó cao hơn, với một kẻ canh giữ ghép khác đi.

## Phạm vi bản Early Access

Các con số dưới đây khớp với [DEV_SPEC.md](DEV_SPEC.md). Giá bán đã chốt: 14,99 USD ở Early Access, 19,99 USD khi ra bản chính thức.

| Hạng mục | Dự kiến |
| --- | --- |
| Map | 1: Thành Golem, gồm 5 khu |
| Golem điều khiển | 3, mỗi Golem một class cố định: Warrior, Hunter, Mage |
| Quái thường | 4 loại Xenon bóng đêm, kèm bản tinh anh |
| Kẻ canh giữ | 3 loại, mỗi loại ghép ngẫu nhiên từ bộ màu, 4 chiêu và vị trí điểm yếu |
| Bài tarot | 22 lá, bắt đầu với 10 lá và mở dần. Ngoài ra có 22 loại sách kỹ năng và khoảng 15 loại dream stone |
| Nhân vật ban ngày | Little Princess, Princess phù thủy, thầy Boro, vài Golem lai màu |
| Co-op | 1–3 người online qua Steam |
| Kể chuyện | Thoại ngắn ban ngày, cảnh mở đầu, 4 đoạn cảnh ngắn kể nguồn gốc Lithea và cảnh kết; không có cinematic dài |

Combat, AI đồng đội, gục và hồi sinh, khung boss hai pha, cửa hàng và lưu game đã có trong mã nguồn hiện tại; hệ kỹ năng và trang bị cũ được thay bằng bốn hệ thống mới. Map Thành Golem và bộ sinh nhà cũng được dùng tiếp; không phần nào phải làm lại từ đầu.

## Những điểm còn mở

Còn hai nhóm điểm chưa chốt và một ý để dành.

1. **Bảng khu và hậu quả khi phai màu.** Đang theo concept, chưa chốt. Bảng ở mục "Ngày và đêm ảnh hưởng lẫn nhau" là minh họa.
2. **Các đề xuất về con số và luật chi tiết.** Liệt kê ở mục 18 của [DEV_SPEC.md](DEV_SPEC.md), chờ duyệt.
3. **Mini game ban ngày.** Ý để dành: các trò chơi nhỏ ban ngày cho phần thưởng mang vào giấc mơ. Chưa làm ở giai đoạn này.

### Hai bước kiểm chứng trước khi sản xuất

1. **Chơi thử luật trộn màu:** một sân đấu, ba Golem, một Xenon bóng đêm. 5 nhóm ba người chơi 15 phút, để biết họ có tự hiểu ra phải phối hợp hay chưa, và có thấy đã khi hạ được quái hay chưa.
2. **Bản thử core loop:** 1 khu, 1 kẻ canh giữ ghép ngẫu nhiên, 3 giờ rảnh ban ngày, 6 lá bài. 5 người chơi 5 ngày đêm liên tiếp, để biết họ có chịu dò manh mối và có muốn ngủ lại sau khi thua hay chưa.

Tài liệu chi tiết cho dev: [DEV_SPEC.md](DEV_SPEC.md).

## Lưu ý về bản quyền

Game chỉ lấy ý tưởng chung "trải nghiệm lại cuộc chiến trong giấc mơ". Không dùng nhân vật, tên riêng hay tình tiết của tác phẩm khác.
