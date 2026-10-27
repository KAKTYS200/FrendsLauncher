#!/usr/bin/env python3
"""Генератор иконок и логотипа Frends Launcher.

Единственный источник геометрии — POLYGONS ниже. Из него собираются и
векторные SVG (Linux/desktop, qrc), и растры: PNG 256, многоразмерный .ico
(Windows, подключается через .rc) и .icns (macOS).

Палитра взята из docs/frends-ui.md, чтобы иконка не выбивалась из интерфейса.

Запуск:
    python scripts/make_frends_icons.py                # пишет в program_info/
    python scripts/make_frends_icons.py --out ./tmp    # пишет в другую папку

Требуется Pillow. Скрипт идемпотентен: перезапись без вопросов.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

# --- Брендинг -------------------------------------------------------------

APP_ID = "org.frends.FrendsLauncher"
BINARY_NAME = "frendslauncher"

# --- Геометрия (система координат 512x512) --------------------------------

CANVAS = 512
CORNER_RADIUS = 112

BG_TOP = "#1F6FA8"
BG_BOTTOM = "#12507B"

# Буква F из фасеток. Порядок важен: стойка рисуется первой, перекладины поверх.
# Каждый прямоугольник разбит по диагонали на два треугольника с разной
# светлотой — это даёт низкополигональный вид, как в FrendsBackdrop.
STEM_DARK = "#1B7BB0"
STEM_LIGHT = "#2E9FD8"
BAR_TOP_LIGHT = "#55C0F0"
BAR_TOP = "#3CA6DA"
ACCENT_LIGHT = "#FFD65C"
ACCENT = "#F5C33B"

POLYGONS: list[tuple[list[tuple[int, int]], str]] = [
    # стойка F: вертикальный раскол на две фасетки (диагональ через всю высоту
    # визуально заваливала букву, поэтому здесь именно вертикаль)
    ([(128, 128), (164, 128), (164, 384), (128, 384)], STEM_DARK),
    ([(164, 128), (200, 128), (200, 384), (164, 384)], STEM_LIGHT),
    # верхняя перекладина
    ([(200, 128), (368, 128), (200, 200)], BAR_TOP_LIGHT),
    ([(200, 200), (368, 128), (368, 200)], BAR_TOP),
    # средняя перекладина — жёлтый акцент из палитры
    ([(200, 236), (320, 236), (200, 300)], ACCENT_LIGHT),
    ([(200, 300), (320, 236), (320, 300)], ACCENT),
]

# Тёмная тема: подложки нет, поэтому синие фасетки светлее, иначе тонут на фоне.
DARKMODE_COLORS = {
    STEM_DARK: "#1B7BB0",
    STEM_LIGHT: "#3CA6DA",
    BAR_TOP_LIGHT: "#7FD1F0",
    BAR_TOP: "#55C0F0",
}

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
PNG_SIZES = [256, 128, 64]


def _svg_body(colors: dict[str, str]) -> str:
    return "\n".join(
        '  <polygon points="{pts}" fill="{fill}"/>'.format(
            pts=" ".join(f"{x},{y}" for x, y in points),
            fill=colors.get(color, color),
        )
        for points, color in POLYGONS
    )


def build_svg(app_icon: bool, colors: dict[str, str] | None = None) -> str:
    """Собирает SVG. app_icon=True — с подложкой-скруглённым квадратом."""
    colors = colors or {}
    body = _svg_body(colors)
    if not app_icon:
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {CANVAS} {CANVAS}" '
            f'width="{CANVAS}" height="{CANVAS}">\n{body}\n</svg>\n'
        )

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {CANVAS} {CANVAS}" '
        f'width="{CANVAS}" height="{CANVAS}">\n'
        "  <defs>\n"
        '    <linearGradient id="frends-bg" x1="0" y1="0" x2="0" y2="1">\n'
        f'      <stop offset="0" stop-color="{BG_TOP}"/>\n'
        f'      <stop offset="1" stop-color="{BG_BOTTOM}"/>\n'
        "    </linearGradient>\n"
        "  </defs>\n"
        f'  <rect x="0" y="0" width="{CANVAS}" height="{CANVAS}" '
        f'rx="{CORNER_RADIUS}" ry="{CORNER_RADIUS}" fill="url(#frends-bg)"/>\n'
        f"{body}\n"
        "</svg>\n"
    )


def render_master(size: int = 2048, app_icon: bool = True, colors: dict[str, str] | None = None) -> Image.Image:
    """Растр высокого разрешения; дальше только уменьшаем — так чище края."""
    colors = colors or {}
    scale = size / CANVAS
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    if app_icon:
        # вертикальный градиент: linear_gradient даёт 0 сверху -> 255 снизу
        gradient = Image.linear_gradient("L").resize((size, size), Image.Resampling.BILINEAR)
        top = Image.new("RGBA", (size, size), BG_TOP)
        bottom = Image.new("RGBA", (size, size), BG_BOTTOM)
        background = Image.composite(bottom, top, gradient)

        mask = Image.new("L", (size, size), 0)
        ImageDraw.Draw(mask).rounded_rectangle(
            [0, 0, size - 1, size - 1], radius=int(CORNER_RADIUS * scale), fill=255
        )
        out.paste(background, (0, 0), mask)

    draw = ImageDraw.Draw(out)
    for points, color in POLYGONS:
        draw.polygon([(x * scale, y * scale) for x, y in points], fill=colors.get(color, color))
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description="Иконки и логотип Frends Launcher")
    parser.add_argument("--out", default="program_info", help="каталог назначения (по умолчанию program_info)")
    args = parser.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # --- вектор ---
    (out / f"{APP_ID}.svg").write_text(build_svg(app_icon=True), encoding="utf-8")
    (out / f"{APP_ID}.bigsur.svg").write_text(build_svg(app_icon=True), encoding="utf-8")
    (out / f"{APP_ID}.logo.svg").write_text(build_svg(app_icon=False), encoding="utf-8")
    (out / f"{APP_ID}.logo-darkmode.svg").write_text(
        build_svg(app_icon=False, colors=DARKMODE_COLORS), encoding="utf-8"
    )

    # --- растр ---
    master = render_master(app_icon=True)

    # CMake ждёт именно _256, остальные держим для hicolor-темы Linux
    for size in PNG_SIZES:
        master.resize((size, size), Image.Resampling.LANCZOS).save(out / f"{APP_ID}_{size}.png")

    icon_256 = master.resize((256, 256), Image.Resampling.LANCZOS)
    icon_256.save(out / f"{BINARY_NAME}.ico", format="ICO", sizes=[(s, s) for s in ICO_SIZES])

    master.resize((512, 512), Image.Resampling.LANCZOS).save(out / f"{BINARY_NAME}.icns", format="ICNS")

    print(f"Готово, каталог: {out.resolve()}")
    for name in sorted(p.name for p in out.iterdir() if p.suffix in {".svg", ".png", ".ico", ".icns"}):
        print(f"  {name}")


if __name__ == "__main__":
    main()
