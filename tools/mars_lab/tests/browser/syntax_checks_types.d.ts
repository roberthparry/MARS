/**
 * @file syntax_checks_types.d.ts
 * @brief Function syntax metadata fixtures and browser capabilities.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** @brief Check native function, matrix and integrator text presentation. */
    checkLabSyntax(): Promise<void>;
}

/** @brief One native text span, optionally replacing a matrix section heading. */
interface SyntaxFixtureSpan {
    text: string;
    kind: string;
    display?: string;
}

/** @brief Native markup and spans for one exact source string. */
interface SyntaxFixtureEntry {
    source: string;
    html: string;
    spans: SyntaxFixtureSpan[];
}

/** @brief Native fields inspected by the matrix and integrator fixtures. */
interface SyntaxFixtureData {
    ok?: boolean;
    error?: string;
    presentation?: {
        matrix_headings?: SyntaxFixtureEntry[];
        integrator?: {detail_text: string; value_text: string};
    };
}

/** @brief Matrix response paired with its inspected presentation payload. */
interface SyntaxFixtureResult {
    response: MarsTransportResponse;
    data: SyntaxFixtureData;
}
