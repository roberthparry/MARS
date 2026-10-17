/**
 * @file application_types.ts
 * @brief Contracts for native bootstrap configuration and catalogue data.
 *
 * Shared by startup and worksheet adapters. These declarations describe validated
 * configuration, native defaults and catalogue entries without introducing runtime state.
 */
/** Native-validated bootstrap data, shared without browser runtime dependencies. */
interface LabConfiguration {
    CONTROL_TOKEN: string;
    CONTROL_QUERY_PREFIX: string;
    DEFAULT_EXPRESSION: string;
    DEFAULT_EQUATION: string;
    DEFAULT_DIFFEQUATION: string;
    DEFAULT_MATRIX: string;
    DEFAULT_INTEGRATOR: string;
    DEFAULT_INTEGRATOR_BOUNDS: string;
    DEFAULT_INTEGRATOR_INTERVAL_CAP: string;
    DEFAULT_DATETIME_TEXT: string;
    DEFAULT_DATETIME_DATE: string;
    DEFAULT_DATETIME_JURISDICTION: string;
    DEFAULT_DATETIME_GMT_OFFSET: string;
    DEFAULT_ALMANAC_TEXT: string;
    DEFAULT_ALMANAC_TIME: string;
    DEFAULT_ALMANAC_ZONE: string;
    DEFAULT_ALMANAC_LATITUDE: string;
    DEFAULT_ALMANAC_LONGITUDE: string;
    DEFAULT_ALMANAC_VISIBILITY: string;
    ALMANAC_WORKSHEET_TITLE: string;
    ALMANAC_COVERAGE_TEXT_JS: string;
}

interface LabCatalogue {
    options: [string, string][];
    towns: Record<string, string[][]>;
}

interface Window {
    labReady: Promise<unknown>;
    labInitialEvaluation: Promise<unknown>;
}
