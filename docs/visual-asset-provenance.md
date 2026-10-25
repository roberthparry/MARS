# Visual asset provenance

This record identifies the authorship and distribution status of visual assets
created for MARS. It is intended to prevent original project artwork and user
interface captures from being mistaken for unrecorded third-party material.

## Original MARS artwork

The following artwork was created specifically for MARS during the project's
development under the direction of Robert H. Parry (Rob), the project's sole
creator and maintainer:

- `packaging/linux/mars-lab.svg`; and
- the SVG and PNG concepts in `packaging/linux/icon-concepts/`.

These assets are original MARS material. They were not copied or adapted from
third-party logos, product identities or branded icon sets. Robert H. Parry
confirms that MARS has the right to distribute them under the repository's MIT
Licence.

The concept artwork first entered the repository on 23 May 2026. The selected
MARS Lab icon entered the repository during the same
project design process in May and June 2026. Git history remains the detailed
record of their subsequent revisions.

## MARS Lab screenshots

The PNG files in `docs/images/mars-lab/` are captures of the MARS Lab user
interface running MARS. They document original MARS screens, controls, results
and generated mathematical rendering. They do not reproduce another
application's user interface or third-party branded artwork and are distributed
under the repository's MIT Licence.

The Almanac capture also includes NASA Blue Marble imagery displayed on the
observer globe, with its attribution visible below the globe. The underlying
imagery retains the provenance and terms recorded in
`THIRD_PARTY_NOTICES.md`; the screenshot does not change those terms.

## Offshore emblem

`packaging/jurisdiction-db/flags/zz-sea.svg` is a vector tracing of the illustration
generated on 24 October 2026 with the built-in image-generation tool, using the
user-supplied Pioneer plaque as the edit target. The requested adaptation uses gold etched lines on MARS green
and replaces the unclothed figures with clothed African-Caribbean adults in
reggae-inspired clothing, with a knitted tam and headwrap. Potrace 1.16 traced
the approved raster's gold shapes into scalable paths on a flat green background.
The bundled SVG contains no bitmap, external resource or active content. It is
decorative artwork, not a faithful scientific transcription. The one-off tracing
tools are not build, installation or runtime dependencies.
The original design credits and public-domain source record are retained in
`THIRD_PARTY_NOTICES.md`; the exact prompt is in
`packaging/jurisdiction-db/flags/zz-sea.txt`. This adapted artwork must not be
confused with an official national or NASA flag. The packaged source and
`OFFSHORE-NOTICES.txt` are imported into the jurisdiction database; Lab serves
the stored SVG rather than reading the packaged source at runtime.

## Future assets

Any future visual asset copied or adapted from another source must be recorded
in `THIRD_PARTY_NOTICES.md` before public distribution. The record must name
the source, author or rights holder, licence, source URL and any attribution or
redistribution conditions. A screenshot containing third-party material must
be reviewed separately; being a screenshot does not remove the underlying
material's licence or copyright conditions.

This provenance statement was confirmed on 22 August 2026.
