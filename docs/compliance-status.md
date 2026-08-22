# Compliance status

This record distinguishes completed repository controls from matters that
require a human rights or release decision. It is an engineering record, not
legal advice.

## Automated controls

`make check-compliance` verifies that private ESAA paths are absent from the
Git index, required legal and provenance records are tracked and installed,
SPDX relationships refer to declared packages, the principal third-party
notices remain present, and the distributed almanac files still match their
recorded SHA-256 checksums.

For a binary release, `make release-evidence` writes
`build/compliance/release-evidence.json`. That record identifies the source
commit and worktree state, hashes the built shared library and every resolved
dynamic library, records the owning Debian package and version where
available, and hashes the installed package copyright file. It supplements
the source-oriented `DEPENDENCIES.spdx`; it does not replace a distributor's
obligation to supply licence or corresponding-source material for libraries
that the distributor bundles.

## Human decision still required

The public *Applied Differential Equations* notes are a modern typesetting of
Robert H. Parry's contemporaneous transcription of Professor P. G. L. Leach's
1986 blackboard lectures. Their preface records that history and attribution,
but it is not evidence of permission from the lecturer, the University of the
Witwatersrand or another possible rights holder. Continued public distribution
therefore requires a rights decision: obtain suitable permission, establish
the applicable ownership and exception with qualified legal advice, replace
the material with an independently written treatment, or keep it outside the
public repository. The generated PDF has the same status as its Markdown
sources.

This matter is deliberately not an automated commit failure. MARS continues
to permit the author to maintain and commit these notes while their public
distribution status is resolved.

## Historical limitation

The exact historical NumPy, SpiceyPy and PyERFA package versions used to
generate the existing astronomical coefficients were not retained. The input
kernels, algorithms, generator revisions and generated-output checksums are
recorded in the almanac provenance document. Any future regeneration must
record exact tool versions and SHA-256 hashes for every input before replacing
the packaged data.

## Release review

Before each public release, review the current WeatherAPI terms if the optional
integration remains enabled, review any changed upstream licence or provider,
run the automated compliance check, produce release evidence from the final
binary, and retain the evidence with the release artefacts.
