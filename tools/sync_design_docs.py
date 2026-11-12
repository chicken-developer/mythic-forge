"""Copy the design drafts at the repository root into ThreeDreamGolems/Docs.

Dev.md and Overview.md are exports of the owner's design document and change often. The copies in
Docs/ are what the code follows, so run this before starting milestone work:

    python tools/sync_design_docs.py            # rewrite DEV_SPEC.md and GAME_DESIGN.md
    python tools/sync_design_docs.py --check    # exit 1 when the copies are out of date

Only repository-specific wording is adapted (module prefix, links between the two files). Design
content is never edited here.
"""
from __future__ import annotations

import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "ThreeDreamGolems" / "Docs"
COPYRIGHT_HEADING = "## Lưu ý về bản quyền"
COPYRIGHT_NOTE = (
    COPYRIGHT_HEADING + "\n\n"
    "Game chỉ lấy ý tưởng chung \"trải nghiệm lại cuộc chiến trong giấc mơ\". "
    "Không dùng nhân vật, tên riêng hay tình tiết của tác phẩm khác.\n"
)
OLD_PREFIX = "Kni" + "ght"  # spelled in two parts so a project-wide rename never rewrites it


def banner(tab: str, stamp: str) -> str:
    return (f"<!-- Bản sao của tab {tab} trong tài liệu Three Dream Golems — Game Design ({stamp}). "
            "Khi thiết kế đổi, sửa tài liệu gốc trước rồi cập nhật file này. -->\n\n")


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def body_without_banner(text: str) -> str:
    return text.split("-->\n\n", 1)[1] if text.startswith("<!--") and "-->\n\n" in text else text


def dev_spec(source: str) -> str:
    text = source.replace("# Tài liệu cho dev\n", "# Three Dream Golems — Đặc tả cho dev\n", 1)
    for old, new in (
        ("nằm ở tab Overview", "nằm ở [GAME_DESIGN.md](GAME_DESIGN.md)"),
        ("tab Overview", "GAME_DESIGN.md"),
        ("theo tab Art", "theo tài liệu art"),
        ("`Docs/MILESTONES.md` của project", "[MILESTONES.md](MILESTONES.md)"),
    ):
        text = text.replace(old, new)
    # The repository finished milestone R0, so module names carry the Golem prefix.
    lines = []
    for line in text.split("\n"):
        if line.startswith("Tên module trong tài liệu này dùng tiền tố"):
            line = ("Tên module trong bản sao này dùng tiền tố `Golem`, vì repo đã xong mốc R0 "
                    "(2026-11-12). Tài liệu gốc còn ghi tiền tố cũ.")
        else:
            line = line.replace("`" + OLD_PREFIX, "`Golem")
        lines.append(line)
    return "\n".join(lines)


def game_design(source: str) -> str:
    lines = source.split("\n")
    lines[0] = "# Three Dream Golems — Game design"
    # Drop the export byline and embedded-content placeholders, which carry no design content.
    kept = [line for index, line in enumerate(lines)
            if not (index == 2 and "·" in line and "@" in line) and "embedded content:" not in line]
    text = "\n".join(kept)
    while "\n\n\n" in text:
        text = text.replace("\n\n\n", "\n\n")
    for old, new in (
        ("khớp với tab Dev", "khớp với [DEV_SPEC.md](DEV_SPEC.md)"),
        ("của tab Dev", "của [DEV_SPEC.md](DEV_SPEC.md)"),
        ("Tài liệu chi tiết cho dev: Dev\n", "Tài liệu chi tiết cho dev: [DEV_SPEC.md](DEV_SPEC.md).\n"),
    ):
        text = text.replace(old, new)
    if COPYRIGHT_HEADING not in text:
        text = text.rstrip("\n") + "\n\n" + COPYRIGHT_NOTE
    return text


def main() -> int:
    check = "--check" in sys.argv[1:]
    stamp = date.today().isoformat()
    stale = []
    for source_name, target_name, tab, convert in (
        ("Dev.md", "DEV_SPEC.md", "Dev", dev_spec),
        ("Overview.md", "GAME_DESIGN.md", "Overview", game_design),
    ):
        source_path, target_path = ROOT / source_name, DOCS / target_name
        if not source_path.is_file():
            print(f"skip {target_name}: {source_name} is not at the repository root")
            continue
        wanted = convert(read(source_path))
        current = body_without_banner(read(target_path)) if target_path.is_file() else ""
        if wanted == current:
            print(f"up to date: {target_name}")
            continue
        stale.append(target_name)
        if not check:
            target_path.write_text(banner(tab, stamp) + wanted, encoding="utf-8", newline="\n")
            print(f"updated: {target_name} from {source_name}")
    if check and stale:
        print("out of date: " + ", ".join(stale))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
