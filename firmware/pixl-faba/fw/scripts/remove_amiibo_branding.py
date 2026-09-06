#!/usr/bin/env python3
"""Remove user-facing Amiibo branding from firmware strings."""

from __future__ import annotations

import csv
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def should_skip_text(text: str) -> bool:
    lowered = text.lower()
    return lowered.startswith("http") or "amiibo.xyz" in lowered


def remove_amiibo_branding(text: str) -> str:
    if not text or should_skip_text(text):
        return text

    text = text.replace("AmiiboLink", "AmiiLink")
    text = re.sub(r"[Aa][Mm][Ii][Ii][Bb][Oo]", "", text)
    text = re.sub(r"[ \t]{2,}", " ", text)
    text = re.sub(r"\s+([,.!?;:])", r"\1", text)
    text = re.sub(r"^[\s\-]+", "", text)
    text = re.sub(r"[\s\-]+$", "", text)
    return text.strip()


def update_i18n_csv() -> int:
    csv_path = os.path.join(ROOT, "data", "i18n.csv")
    rows: list[list[str]] = []
    changed = 0

    with open(csv_path, "r", encoding="utf-8", newline="") as handle:
        reader = csv.reader(handle)
        for row in reader:
            if not row:
                rows.append(row)
                continue
            new_row = [row[0]]
            for value in row[1:]:
                updated = remove_amiibo_branding(value)
                if updated != value:
                    changed += 1
                new_row.append(updated)
            rows.append(new_row)

    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerows(rows)

    return changed


def replace_in_file(path: str, replacements: list[tuple[str, str]]) -> int:
    with open(path, "r", encoding="utf-8") as handle:
        content = handle.read()

    original = content
    for old, new in replacements:
        content = content.replace(old, new)

    if content != original:
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(content)
        return 1
    return 0


def update_hardcoded_sources() -> int:
    changed = 0
    file_replacements = {
        os.path.join(ROOT, "application", "src", "app", "amiibo", "app_amiibo.c"): [
            ('.name = "Amiibo模拟器"', '.name = "模拟器"'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiidb", "app_amiidb.c"): [
            ('.name = "Amiibo数据库"', '.name = "数据库"'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiibolink", "app_amiibolink.c"): [
            ('.name = "AmiiboLink"', '.name = "AmiiLink"'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiibo", "view", "amiibo_detail_view.c"): [
            ('mui_canvas_draw_utf8(p_canvas, 0, y += 15, "Amiibo");\n        sprintf(buff, "[%08x:%08x]", head, tail);',
             'sprintf(buff, "[%08x:%08x]", head, tail);'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiidb", "view", "amiibo_view.c"): [
            ('mui_canvas_draw_utf8(p_canvas, 0, y += 15, "Amiibo");\n            sprintf(buff, "[%08x:%08x]", head, tail);',
             'sprintf(buff, "[%08x:%08x]", head, tail);'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiibolink", "view", "amiibolink_view.c"): [
            ('mui_canvas_draw_utf8(p_canvas, 5, y += 15, "Amiibo");\n            sprintf(buff, "[%08x:%08x]", head, tail);',
             'sprintf(buff, "[%08x:%08x]", head, tail);'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiidb", "scene", "amiidb_scene_data_list.c"): [
            ('sprintf(txt, "Amiibo[%08x:%08x]"', 'sprintf(txt, "[%08x:%08x]"'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiidb", "scene", "amiidb_scene_fav_list.c"): [
            ('sprintf(txt, "Amiibo[%08x:%08x]"', 'sprintf(txt, "[%08x:%08x]"'),
        ],
        os.path.join(ROOT, "application", "src", "app", "amiidb", "scene", "amiidb_scene_game_list.c"): [
            ('sprintf(txt, "Amiibo[%08x:%08x]"', 'sprintf(txt, "[%08x:%08x]"'),
        ],
        os.path.join(ROOT, "data", "amiidb_game.csv"): [
            ("Amiibo Cards", "Cards"),
            ("Amiibo Figures", "Figures"),
            ("Amiibo Festival", "Festival"),
            ("Welcome Amiibo", "Welcome"),
        ],
    }

    for path, replacements in file_replacements.items():
        changed += replace_in_file(path, replacements)

    return changed


def run_generators() -> None:
    scripts_dir = os.path.join(ROOT, "scripts")
    subprocess.check_call([sys.executable, os.path.join(scripts_dir, "i18n_gen.py")], cwd=scripts_dir)
    subprocess.check_call([sys.executable, os.path.join(scripts_dir, "amiibo_db_gen.py")], cwd=scripts_dir)


def main() -> int:
    csv_changes = update_i18n_csv()
    source_changes = update_hardcoded_sources()
    run_generators()
    print(f"Updated i18n.csv cells: {csv_changes}")
    print(f"Updated source files: {source_changes}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
