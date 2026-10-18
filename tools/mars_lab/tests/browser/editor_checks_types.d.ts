/**
 * @file editor_checks_types.d.ts
 * @brief Editor metadata fixtures, compact results and browser entry point.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** @brief Check native editor metadata, source selection and lazy getter boundaries. */
    checkLabEditor(): void;
}

/** @brief Metadata fields deliberately accept malformed native-fixture values. */
interface EditorFixtureMetadata {
    expression?: unknown;
    text?: unknown;
    body?: unknown;
    display?: unknown;
    wrapped?: unknown;
    bindings?: unknown;
}

/** @brief Compact metadata preserves raw display and binding values for validation. */
interface EditorFixtureCompact {
    display: unknown;
    shortened: boolean;
    bindings: unknown;
}

/** @brief One independent metadata projection used as the fixture oracle. */
interface EditorFixtureProjection {
    /** @brief Select the expected value without calling production metadata helpers. */
    (value: unknown, editors: Map<string, EditorFixtureMetadata>): unknown;
}
