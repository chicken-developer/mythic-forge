# Three Dream Golems — Game design

Cập nhật 2026-11-12. Đây là bản tóm tắt thiết kế đã được chủ dự án duyệt. Con số và cấu trúc dữ liệu chi tiết nằm ở [DEV_SPEC.md](DEV_SPEC.md).

## Game trong một trang

| Hạng mục | Nội dung |
|---|---|
| Thể loại | Co-op action roguelite, góc nhìn thứ ba |
| Số người chơi | 1–3 online qua Steam; chơi đơn có 2 đồng đội AI |
| Một lượt chơi | Một ngày-đêm, khoảng 20–25 phút: ngày 2–4 phút, đêm 15–20 phút |
| Nhân vật điều khiển | Ba Golem tí hon: Mars (Warrior, Earth, đỏ), Jupiter (Hunter, Sky, vàng), Neptune (Mage, Sea, xanh dương) |
| Nhân vật biểu tượng | Little Princess: người dẫn đường, hỗ trợ trong trận |
| Phản diện | Xeros, Xenon bị bỏ quên và bị bóng đêm lấp đầy |
| Bối cảnh | Thành Golem trên hành tinh Lithea |
| Giọng điệu | Ấm, hài, dễ thương, phiêu lưu |
| Nền tảng | PC (Steam), Unreal Engine 5 |
| Ngôn ngữ | Anh, Việt, Trung giản thể |
| Giá | 14,99 USD ở Early Access, 19,99 USD khi ra bản chính thức |

## Ba ý tưởng chính

1. **Thua trong quá khứ thì hiện tại hỏng dần.** Giấc mơ là trận chiến ngàn năm trước đang diễn ra lại. Mỗi khu để mất trong mơ làm khu đó ngoài đời phai màu.
2. **Phải hiểu boss trước khi đánh.** Kẻ canh giữ mỗi đêm một khác. Ban ngày là lúc dò manh mối và chuẩn bị.
3. **Một mình không thắng nổi.** Ba Golem nhỏ hơn hẳn kẻ địch. Muốn thắng phải trộn đúng màu nguyên tố lên điểm yếu của quái.

## Cốt truyện

### Nguồn gốc

1. Các Little Princess được giao nhiệm vụ ban sự sống cho các hành tinh. Ở Lithea, một Little Princess dùng bột vũ trụ nặn nên các **Xenon**: những Golem xám xịt, không có linh hồn.
2. Cô thả vào các Xenon ba nguyên tố Earth, Sky và Sea. Chúng có sự sống, có màu, và bắt đầu xây dựng thế giới.
3. Tới lượt Xenon cuối cùng thì cô đã hết nguyên tố. Hắn bị bỏ quên, và khoảng trống trong tâm trí hắn bị bóng đêm lấp đầy. Đó là **Xeros**, kẻ tin rằng bóng đêm là nguồn gốc vạn vật và muốn nuốt chửng tất cả.
4. Ngàn năm trước, ba Golem đầu tiên, mỗi kẻ mang một nguyên tố thuần khiết, đã thắng Xeros và giam hắn vào giấc ngủ.
5. Giờ đây Xeros kéo trận chiến xưa vào giấc mơ và đánh lại. Lần này hắn thắng, Little Princess bị bắt, và hiện tại bắt đầu phai màu.

### Mở đầu game

1. Game bắt đầu ngay trong mơ: ba Golem giữ thành và chắc chắn thua. Little Princess bị bắt đi.
2. Ba Golem tỉnh dậy. Little Princess vẫn ở đó dưới dạng một tinh linh chỉ ba đứa nhìn thấy.
3. Một góc thành đã mất màu. Cô giải thích: trận đêm qua là trận chiến ngàn năm trước, và kết cục của nó đang đổi hiện tại.
4. Ba Golem phải quay lại giấc mơ, giành lại từng khu của thành và cứu cô.

### Kết

- **Cả câu chuyện:** ba Golem cùng trao cho Xeros ba nguyên tố hắn chưa bao giờ nhận được.
- **Bản Early Access:** kết ở cung điện. Ba Golem gặp Xeros lần đầu và cứu được Little Princess, ngay sau đó một Xeros phiên bản Fey xuất hiện. Các vùng đất khác, tộc Fey và các Little Princess khác để dành cho bản sau.

## Core loop

| Bước | Thời lượng | Việc xảy ra |
|---|---|---|
| Sáng | 30 giây | Thành phố đổi khác theo kết quả đêm qua. Little Princess gợi ý khu cần giúp. |
| Ngày | 2–4 phút | 3 giờ tự do, mỗi việc tốn một giờ: dò manh mối về boss, nâng vũ khí ở lò rèn, trò chuyện để mở cốt truyện. |
| Đêm | 15–20 phút | Theo Little Princess tới khu đã chọn, cô mở cổng giấc mơ. Mua sách kỹ năng, qua 3 sân đấu, đánh kẻ canh giữ của khu. |
| Tỉnh dậy | 30 giây | Thắng thì khu lấy lại màu và mở khu kế. Thắng hay thua đều nhận DreamCoin và hiểu biết về boss. |

## Luật trộn màu

Xenon bóng đêm to gấp 2–3 lần Golem, thân xám gần như không thể bị thương. Điểm yếu là những **khoảng trống** phát sáng, mang màu của nguyên tố chúng đang thiếu.

| Màu khoảng trống | Cần ai |
|---|---|
| Đỏ | Mars |
| Vàng | Jupiter |
| Xanh dương | Neptune |
| Cam | Mars và Jupiter |
| Xanh lá | Jupiter và Neptune |
| Tím | Mars và Neptune |
| Trắng sáng | Cả ba; chỉ có ở kẻ canh giữ và Xeros |

1. Các Golem đúng màu đánh đòn nặng vào khoảng trống trong cùng một cửa sổ khoảng 4 giây.
2. Đủ màu thì khoảng trống được lấp: quái mất nhiều máu và khựng lại.
3. Đánh sai màu thì khoảng trống xỉn đi khoảng 3 giây.
4. Lấp hết mọi khoảng trống thì Xenon lấy lại màu và thôi chiến đấu. Nó không bị phá huỷ.

Mỗi nguyên tố có thêm một ký hiệu riêng để người mù màu chơi được. Chơi đơn: nhắm vào khoảng trống và bấm nút ra hiệu, Golem AI có màu phù hợp sẽ cùng đánh.

## Bốn hệ thống của người chơi

| Hệ thống | Tóm tắt | Giữ sau đêm? |
|---|---|---|
| Vũ khí | Mỗi class một vũ khí, nâng 5 bậc ở lò rèn | Có |
| Dream stone | Gắn vào ô trên vũ khí, mỗi viên tăng một hoặc hai chỉ số; 15 loại | Có |
| Sách kỹ năng | Mua bằng DreamCoin ở cửa hàng của Princess phù thủy trong mơ; mỗi cuốn dùng một lần; mỗi Golem mang 4 cuốn; 22 loại | Không |
| Tarot | Nhặt ngẫu nhiên khi hạ quái; giữ 3 lá; bật một lá để có lợi thế 30 giây; 22 lá | Không |

Chỉ số: Máu, Sức đánh, Nhanh nhẹn, Giữ màu. Tiền: DreamCoin, loại duy nhất, nhận sau mỗi đêm dù thắng hay thua.

## Thế giới của bản Early Access

Một tòa thành, 5 khu, rộng khoảng 290 × 220 m, tổng chênh cao 8–12 m. Thứ tự mở đề xuất: khu chợ, quảng trường, khu lò rèn, bến cảng, cung điện.

Mỗi khu có ba mức: bình thường, suy yếu, nguy kịch. Thắng một đêm thì khu tốt lên một mức, thua thì xấu đi một mức. Khu không bao giờ mất hẳn. Mỗi khu có 3 độ khó để chơi lại.

## Chưa chốt

1. Chức năng ban ngày của từng khu và hậu quả khi khu phai màu.
2. Mini game ban ngày cho phần thưởng mang vào giấc mơ: ý để dành, chưa làm.
3. Các đề xuất đang chờ duyệt, liệt kê ở mục 18 của [DEV_SPEC.md](DEV_SPEC.md).

## Lưu ý về bản quyền

Game chỉ lấy ý tưởng chung "trải nghiệm lại cuộc chiến trong giấc mơ". Không dùng nhân vật, tên riêng hay tình tiết của tác phẩm khác.
