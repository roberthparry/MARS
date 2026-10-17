/**
 * @file syntax_checks_types.d.ts
 * @brief Function syntax metadata fixtures and browser capabilities.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabSyntax(): Promise<void>;
}

interface SyntaxFixtureEntry {
    source: string;
    html: string;
    spans: {text: string; kind: string; display?: string}[];
}
interface SyntaxFixtureData {
    ok?: boolean;
    error?: string;
    presentation?: {
        matrix_headings?: SyntaxFixtureEntry[];
        integrator?: {detail_text: string; value_text: string};
    };
}
