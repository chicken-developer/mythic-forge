# Three Dream Golems

Cập nhật 2026-11-13 (Asia/Saigon).

Game co-op hành động cho 1–3 người, góc nhìn thứ ba, Unreal Engine 5.8, phát hành trên Steam (PC). Ban ngày ba Golem học trò dò la và chuẩn bị trong Thành Golem; ban đêm cả ba trở về trận chiến ngàn năm trước trong giấc mơ để giữ cho thế giới hiện tại khỏi phai màu.

Tên cũ của project là Three Dream Knights (`ThreeDreamingKnights`). Thiết kế cũ (action RPG có campaign, 9 spec, 12 ô trang bị) đã được thay bằng thiết kế trong thư mục `Docs/`. Mã nguồn cũ được dùng lại làm nền, không viết lại từ đầu.

## Đọc gì trước

| Tài liệu | Dùng để |
|---|---|
| [Docs/GAME_DESIGN.md](Docs/GAME_DESIGN.md) | Hiểu game: ý tưởng, cốt truyện, core loop, luật trộn màu |
| [Docs/DEV_SPEC.md](Docs/DEV_SPEC.md) | Đặc tả chi tiết để code: trạng thái game, dữ liệu, con số khởi điểm |
| [Docs/MILESTONES.md](Docs/MILESTONES.md) | Thứ tự làm, phạm vi và tiêu chí nghiệm thu của từng mốc |
| [Docs/CODE_BASELINE.md](Docs/CODE_BASELINE.md) | Mã nguồn hiện có gì, phần nào giữ, phần nào thay |
| [Docs/REFACTOR_REPORT.md](Docs/REFACTOR_REPORT.md), [Docs/R1_REPORT.md](Docs/R1_REPORT.md) | Báo cáo mốc R0 (đổi tên) và R1 (xoá hệ thống cũ) |
| [CODING_RULES.md](CODING_RULES.md) | Quy tắc code, bắt buộc đọc trước khi sửa mã nguồn |

Khi hai tài liệu mâu thuẫn: chỉ dẫn trực tiếp của chủ dự án đứng trên hết, rồi tới `GAME_DESIGN.md`, `DEV_SPEC.md`, `MILESTONES.md`. `CODING_RULES.md` quyết định phong cách code.

## Trạng thái hiện tại

- **Mốc đang làm:** M0, luật trộn màu. R0 và R1 đã đạt, xem [Docs/REFACTOR_REPORT.md](Docs/REFACTOR_REPORT.md) và [Docs/R1_REPORT.md](Docs/R1_REPORT.md). Thứ tự các mốc ở [Docs/MILESTONES.md](Docs/MILESTONES.md).
- **Mã nguồn:** 14 module C++ trên GAS, StateTree, Enhanced Input. Có đội ba Golem (Mars, Jupiter, Neptune), combat cận chiến, AI đồng đội, gục và hồi sinh, boss hai pha và 38 test tự động. Chi tiết ở [Docs/CODE_BASELINE.md](Docs/CODE_BASELINE.md).
- **Chưa có:** luật trộn màu, Xenon, vòng ngày-đêm, bộ sinh kẻ canh giữ, sách kỹ năng, tarot, DreamCoin, lưu game, giao diện trong trận, phiên chơi qua Steam.

## Nguyên tắc làm việc

1. **Chứng minh game vui trước khi làm nội dung.** Mốc M0 (luật trộn màu) và M1 (core loop một khu) phải đạt trước khi làm M2.
2. **Mọi con số nằm trong dữ liệu.** Máu, sát thương, thời gian, giá và tỉ lệ rơi đặt trong DataTable hoặc DataAsset.
3. **Host quyết định kết quả trận đấu.** Client chỉ dự đoán hiệu ứng hình ảnh và chuyển động của chính mình.
4. **Hệ thống cũ không dùng nữa thì xoá hẳn.** Không giữ thư mục lưu hay bản sao; bản cũ nằm trong lịch sử git (chỉ dẫn của chủ dự án, 2026-11-13).
5. **Mỗi hệ thống mới có test tự động.** Không ghi PASS cho thứ chưa chạy.
6. **Không tạo file `.uasset` hoặc `.umap` giả bằng text.**

## Chạy project

- Engine: Unreal Engine 5.8 (bản đã cài theo báo cáo cũ là 5.8.3). Không tự nâng engine.
- Mở file `.uproject` trong thư mục này. Sau khi đổi tên hoặc thêm module, tạo lại project file rồi build cấu hình Development Editor, Win64.
- Bản đồ mặc định và phím điều khiển hiện tại ghi ở [Docs/CODE_BASELINE.md](Docs/CODE_BASELINE.md).
