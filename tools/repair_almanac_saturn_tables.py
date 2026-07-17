#!/usr/bin/env python3
import re
from pathlib import Path


MARKDOWN = Path("docs/Explanatory Supplement to the Astronomical Almanac.md")
TEX_DIR = Path("src/almanac/Explanatory Supplement Astronomical Almanac TeX/tex")


TABLE_7531 = r"""**Table 7.53.1**  
Rotation Parameters for Saturn's Satellites

| No. | Satellite | Rotation parameters |
|:---:|:---|:---|
| XVIII | Pan | $\alpha_0=40^\circ.6-0^\circ.036\,T$<br>$\delta_0=83^\circ.53-0^\circ.004\,T$<br>$W=48^\circ.8+626^\circ.0440000\,t$ |
| XV | Atlas | $\alpha_0=40^\circ.58-0^\circ.036\,T$<br>$\delta_0=83^\circ.53-0^\circ.004\,T$<br>$W=137^\circ.88+598^\circ.3060000\,t$ |
| XVII | Prometheus | $\alpha_0=40^\circ.58-0^\circ.036\,T$<br>$\delta_0=83^\circ.53-0^\circ.004\,T$<br>$W=296^\circ.14+587^\circ.2890000\,t$ |
| XVII | Pandora | $\alpha_0=40^\circ.58-0^\circ.036\,T$<br>$\delta_0=83^\circ.53-0^\circ.004\,T$<br>$W=162^\circ.92+572^\circ.7891000\,t$ |
| XI | Epimetheus | $\alpha_0=40^\circ.58-0^\circ.036\,T-3^\circ.153\sin S_1+0^\circ.086\sin 2S_1$<br>$\delta_0=83^\circ.52-0^\circ.004\,T-0^\circ.356\cos S_1+0^\circ.005\cos 2S_1$<br>$W=293^\circ.87+518^\circ.4907239\,t+3^\circ.133\sin S_1-0^\circ.086\sin 2S_1$ |
| X | Janus | $\alpha_0=40^\circ.58-0^\circ.036\,T-1^\circ.623\sin S_2+0^\circ.023\sin 2S_2$<br>$\delta_0=83^\circ.53-0^\circ.004\,T-0^\circ.183\cos S_2+0^\circ.001\cos 2S_2$<br>$W=58^\circ.83+518^\circ.2359876\,t+1^\circ.613\sin S_2-0^\circ.023\sin 2S_2$ |
| I | Mimas | $\alpha_0=40^\circ.66-0^\circ.036\,T+13^\circ.56\sin S_3$<br>$\delta_0=83^\circ.52-0^\circ.004\,T-1^\circ.53\cos S_3$<br>$W=337^\circ.46+381^\circ.9945550\,t-13^\circ.48\sin S_3-44^\circ.85\sin S_9$ |
| II | Enceladus | $\alpha_0=40^\circ.66-0^\circ.036\,T$<br>$\delta_0=83^\circ.52-0^\circ.004\,T$<br>$W=2^\circ.82+262^\circ.7318996\,t$ |
| III | Tethys | $\alpha_0=40^\circ.66-0^\circ.036\,T+9^\circ.66\sin S_4$<br>$\delta_0=83^\circ.52-0^\circ.004\,T-1^\circ.09\cos S_4$<br>$W=10^\circ.45+190^\circ.6979085\,t-9^\circ.60\sin S_4+2^\circ.23\sin S_9$ |
| XIII | Telesto | $\alpha_0=50^\circ.50-0^\circ.036\,T$<br>$\delta_0=84^\circ.06-0^\circ.004\,T$<br>$W=56^\circ.88+190^\circ.6979330\,t$ |
| XIV | Calypso | $\alpha_0=40^\circ.58-0^\circ.036\,T+13^\circ.943\sin S_5-1^\circ.686\sin 2S_5$<br>$\delta_0=83^\circ.43-0^\circ.004\,T-1^\circ.572\cos S_5+0^\circ.095\cos 2S_5$<br>$W=149^\circ.36+190^\circ.6742373\,t-13^\circ.849\sin S_5+1^\circ.685\sin 2S_5$ |
| IV | Dione | $\alpha_0=40^\circ.66-0^\circ.036\,T$<br>$\delta_0=83^\circ.52-0^\circ.004\,T$<br>$W=357^\circ.00+131^\circ.5349316\,t$ |
| XII | Helene | $\alpha_0=40^\circ.58-0^\circ.036\,T+1^\circ.662\sin S_6+0^\circ.024\sin 2S_6$<br>$\delta_0=83^\circ.52-0^\circ.004\,T-0^\circ.187\cos S_6+0^\circ.095\cos 2S_6$<br>$W=245^\circ.39+131^\circ.6174056\,t-1^\circ.651\sin S_6+0^\circ.024\sin 2S_6$ |
| V | Rhea | $\alpha_0=40^\circ.38-0^\circ.036\,T+3^\circ.10\sin S_7$<br>$\delta_0=83^\circ.55-0^\circ.004\,T-0^\circ.35\cos S_7$<br>$W=235^\circ.16+79^\circ.6900478\,t-3^\circ.08\sin S_7$ |
| VI | Titan | $\alpha_0=36^\circ.41-0^\circ.036\,T+2^\circ.66\sin S_8$<br>$\delta_0=83^\circ.94-0^\circ.004\,T-0^\circ.30\cos S_8$<br>$W=189^\circ.64+22^\circ.5769768\,t-2^\circ.64\sin S_8$ |
| VIII | Iapetus | $\alpha_0=318^\circ.16-3^\circ.949\,T$<br>$\delta_0=75^\circ.03-1^\circ.143\,T$<br>$W=350^\circ.20+4^\circ.5379572\,t$ |
| XI | Phoebe | $\alpha_0=355^\circ.16$<br>$\delta_0=68^\circ.70-1^\circ.143\,T$<br>$W=304^\circ.70+930^\circ.8338720\,t$ |"""


TABLE_7532 = r"""**Table 7.53.2**  
Standard Cartographic Longitudes for Saturn's Satellites

| Satellite | Crater | Meridian |
|:---|:---|---:|
| Mimas | Palomides | $162^\circ$ |
| Enceladus | Salih | $5^\circ$ |
| Tethys | Arete | $299^\circ$ |
| Dione | Palinurus | $63^\circ$ |
| Rhea | Tore | $340^\circ$ |
| Iapetus | Almeric | $276^\circ$ |"""


def replace_fenced_table(source: str, heading: str, replacement: str) -> str:
    start = source.index(heading)
    fence_start = source.index("```latex", start)
    fence_end = source.index("```", fence_start + len("```latex")) + 3
    return source[:start] + replacement + source[fence_end:]


ROW_PATTERN = re.compile(
    r"^([IVX]+)(?:\s*&\s*|\s+)([A-Za-z]+):\s*&?\s*\n"
    r"\$\\begin\{aligned\}\[t\]\n(.*?)\n\\end\{aligned\}\$\s*\\\\(?:\[[^]]*\])?",
    re.MULTILINE | re.DOTALL,
)


def formula_lines(body: str) -> list[str]:
    lines: list[str] = []
    for item in re.split(r"\\\\\s*\n", body):
        item = item.strip()
        if not item:
            continue
        if item.startswith(r"&\quad"):
            if not lines:
                raise ValueError("orphaned aligned continuation")
            lines[-1] += " " + item.removeprefix(r"&\quad").strip()
            continue
        lines.append(item.replace("&", ""))
    return lines


def rotation_table(page: int, number: str, caption: str, expected_rows: int) -> str:
    tex = (TEX_DIR / f"page-{page}.tex").read_text(encoding="utf-8")
    marker = rf"\rotationtable{{Table {number}}}"
    start = tex.index(marker)
    tabular_start = tex.index(r"\begin{tabular}", start)
    tabular_end = tex.index(r"\end{tabular}", tabular_start)
    rows = ROW_PATTERN.findall(tex[tabular_start:tabular_end])
    if len(rows) != expected_rows:
        raise ValueError(f"Table {number}: expected {expected_rows} rows, found {len(rows)}")

    rendered = [
        f"**Table {number}**  ",
        caption,
        "",
        "| No. | Satellite | Rotation parameters |",
        "|:---:|:---|:---|",
    ]
    for numeral, satellite, body in rows:
        formulas = "<br>".join(f"${line}$" for line in formula_lines(body))
        rendered.append(f"| {numeral} | {satellite} | {formulas} |")
    return "\n".join(rendered)


def replace_malformed_fenced_table(source: str, heading: str, replacement: str) -> str:
    if heading not in source:
        return source
    return replace_fenced_table(source, heading, replacement)


def main() -> None:
    source = MARKDOWN.read_text(encoding="utf-8")
    if "**Table 7.53.1**" in source and "**Table 7.53.1**  \nRotation Parameters for Saturn's Satellites\n```latex" in source:
        source = replace_fenced_table(source, "**Table 7.53.1**", TABLE_7531)
        source = source.replace("\n\n\n}\n\n\n<!-- page-411 -->", "\n\n\n<!-- page-411 -->", 1)

    if "Table 7.53.2{" in source:
        start = source.index("Table 7.53.2{")
        fence_start = source.index("```latex", start)
        fence_end = source.index("```", fence_start + len("```latex")) + 3
        block_end = source.index("\n\n}\n", fence_end) + len("\n\n}")
        source = source[:start] + TABLE_7532 + source[block_end:]

    mars = rotation_table(407, "7.51.1", "Rotation Parameters for Mars' Satellites", 2)
    mars += r"""

where the values of $M_n$ are

$$
\begin{aligned}
M_1&=169^\circ.51-0^\circ.4357640\,t,\\
M_2&=192^\circ.93+1128^\circ.4096700\,t+0^\circ.6644\times10^{-9}t^2,\\
M_3&=53^\circ.47-0^\circ.0181510\,t.
\end{aligned}
$$"""
    source = replace_malformed_fenced_table(
        source, "Table 7.51.1 Rotation Parameters for Mars' Satellites", mars
    )
    source = source.replace("\n\n\n}\n\n\nBoth of these satellites", "\n\n\nBoth of these satellites", 1)

    replacements = [
        ("\\rotationtableTable 7.52.1", rotation_table(409, "7.52.1", "Rotation Parameters for Jupiter's Satellites", 8)),
        ("\\rotationtableTable 7.54.1", rotation_table(412, "7.54.1", "Rotation Parameters for Uranus' Satellites", 15)),
        ("\\rotationtableTable 7.55.1", rotation_table(413, "7.55.1", "Rotation Parameters for Neptune's Satellites", 7)),
    ]
    for heading, replacement in replacements:
        source = replace_malformed_fenced_table(source, heading, replacement)

    source = source.replace("\n\n\n}\n\n\n**Table 7.52.2**", "\n\n\n**Table 7.52.2**", 1)
    MARKDOWN.write_text(source, encoding="utf-8")


if __name__ == "__main__":
    main()
