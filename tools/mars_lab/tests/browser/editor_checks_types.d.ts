/**
 * @file editor_checks_types.d.ts
 * @brief Editor metadata fixtures, compact results and browser entry point.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabEditor(): void;
}

/** Metadata fields deliberately accept malformed native-fixture values. */
interface EditorFixtureMetadata {
    expression?: unknown; text?: unknown; body?: unknown; display?: unknown; wrapped?: unknown; bindings?: unknown;
}
interface EditorFixtureCompact { display: unknown; shortened: boolean; bindings: unknown; }
