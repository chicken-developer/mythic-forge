# Cơ sở thiết kế

| Reference mới | Đặc điểm quan sát | Quy tắc triển khai | Phần suy luận |
|---|---|---|---|
| new_reference_01 | Tháp đô thị cao; chóp có đầu hồi nhỏ; phân cấp chân/thân/đỉnh | Thân tháp chia tầng, cửa hẹp tầng giữa, tầng chuông cửa đôi, trụ giật cấp và crown canopied | Mặt khuất, kích thước, đường vào |
| new_reference_02 | Hai tháp, portal nhiều lớp, cửa lớn trung tâm, trang trí phân tầng | Tracery front, ba portal khi đủ rộng, blind arcade, trụ ghép, cửa đôi tầng chuông | Không mô phỏng mọi điêu khắc; cửa và mặt khuất được suy dẫn |
| new_reference_03 | Vòm cổng xuyên; đồng hồ; tháp tròn lớn một bên; turret nhỏ; chóp mái kín | Urban asymmetric gate; tháp 12 mặt, lỗ cửa thật, crown mái kín; pediment và khung đồng hồ | 12 mặt là approximation của mặt tròn; cầu thang chưa dựng |
| new_reference_04 | Fantasy Gothic có lõi nghi lễ cao, nhiều tầng khối và chi tiết đặc biệt | Cathedral chữ thập, facade nhiều lớp, crossing tower và chóp mái kín; phân biệt vùng nhấn và vùng nền | Không tái dựng sân/mái mở, tượng và kết cấu độc bản trong ảnh |

Nhịp kiến trúc được điều khiển ở ba cấp: footprint/tổ hợp khối; loại mặt đứng/tầng tháp; chi tiết cục bộ. Hệ sử dụng kích thước riêng của cửa, porch và tầng chức năng thay vì scale một building hoàn chỉnh. Decoration seed chỉ tạo khác biệt màu nhỏ; biến thể kiến trúc xuất phát từ control có nghĩa, không phải nhiễu ngẫu nhiên.

Mái thân chính giữ cao độ tựa tường; cửa được bố trí ngoài vùng interface. Porch có cột và dầm đỡ, mái thấp hơn cửa trên mặt tiền. Phần thân tháp gặp mái là vùng chịu lực đặc; openwork phải nằm trên mái. Các mối ghép pier, moulding, cap và canopy có giao vật liệu có chủ đích. Kiểm tra envelope không thay thế kiểm định mọi self-intersection hoặc tính kết cấu.

Quy tắc chóp 2.1: `openwork` giữ nguyên ID để nhận preset cũ, nay chỉ mô tả chi tiết Gothic ở chân chóp; vỏ mái luôn kín. Vai mái rộng chuyển sang thân chóp hẹp và cao, gable nhỏ tựa trên vai mái. Tháp tròn dùng chân mái 12 cạnh; tháp vuông dùng chân mái bát giác trên cornice trong phạm vi tường. Không có giằng X xuyên suốt chiều cao chóp. Envelope mái tham gia cắt giao khối. Đây là diễn giải hình học từ reference, không phải hồ sơ tính kết cấu.
