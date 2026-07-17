from pathlib import Path


target = Path("docs/Explanatory Supplement to the Astronomical Almanac.md")
snapshot = Path("/tmp/almanac-repaired.md")

current = target.read_text(encoding="utf-8")
recovery = snapshot.read_text(encoding="utf-8")

toc_marker = "Table 7.51.1 Rotation Parameters for Mars' Satellites407<br>"
bad_marker = "**Table 7.51.1**  \nRotation Parameters for Mars' Satellites"
resume_marker = "<!-- page-327 -->"

recovery_start = recovery.index(toc_marker)
recovery_end = recovery.index(resume_marker, recovery_start)
bad_start = current.index(bad_marker)
current_resume = current.index(resume_marker, bad_start)

assert current[:bad_start].endswith("Table 7.48.1 Physical Ephemeris Parameters for Pluto406<br>\n")
assert "Figures 6.11.1 and 6.11.2. These elements are" in recovery[recovery_start:recovery_end]
assert "<!-- page-326 -->" in recovery[recovery_start:recovery_end]

restored = (
    current[:bad_start]
    + recovery[recovery_start:recovery_end]
    + current[current_resume:]
)

target.write_text(restored, encoding="utf-8")
