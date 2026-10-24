/**
 * @file application_types.ts
 * @brief Contracts for native bootstrap configuration and catalogue data.
 *
 * Names the fields consumed by browser adapters and native worksheet code.
 * lab_page_bootstrap() supplies configuration; lab_page_jurisdictions() supplies
 * catalogue entries, with town presentation text from lab_forms_town_presentation().
 * These declarations add no runtime state or interpretation of native values.
 */

/** @brief Flat bootstrap fields, grouped by their browser and native responsibilities. */
interface LabConfiguration {
    /** @brief Browser ABI compatibility and authenticated control URL fields. */
    BROWSER_ABI_VERSION: string;
    CONTROL_TOKEN: string;
    CONTROL_QUERY_PREFIX: string;

    /** @brief Initial mathematical worksheet text and the equation's unknown variable. */
    DEFAULT_EXPRESSION: string;
    DEFAULT_EQUATION: string;
    DEFAULT_EQUATION_VARIABLE: string;
    DEFAULT_DIFFEQUATION: string;
    DEFAULT_MATRIX: string;

    /** @brief Integration defaults; the native bootstrap sends the interval cap as a number. */
    DEFAULT_INTEGRATOR: string;
    DEFAULT_INTEGRATOR_BOUNDS: string;
    DEFAULT_INTEGRATOR_INTERVAL_CAP: number;

    /** @brief Calendar defaults used by native field restoration, filling and reset. */
    DEFAULT_DATETIME_TEXT: string;
    DEFAULT_DATETIME_DATE: string;
    DEFAULT_DATETIME_JURISDICTION: string;
    DEFAULT_DATETIME_LATITUDE: string;
    DEFAULT_DATETIME_LONGITUDE: string;
    DEFAULT_DATETIME_ELEVATION: string;
    DEFAULT_DATETIME_GMT_OFFSET: string;

    /** @brief Almanac defaults; coordinates, elevation and time-zone offset remain text. */
    DEFAULT_ALMANAC_TEXT: string;
    DEFAULT_ALMANAC_DATE: string;
    DEFAULT_ALMANAC_TIME: string;
    DEFAULT_ALMANAC_ZONE: string;
    DEFAULT_ALMANAC_LATITUDE: string;
    DEFAULT_ALMANAC_LONGITUDE: string;
    DEFAULT_ALMANAC_ELEVATION: string;
    DEFAULT_ALMANAC_VISIBILITY: string;

    /** @brief Native worksheet title and ephemeris coverage text for presentation. */
    ALMANAC_WORKSHEET_TITLE: string;
    ALMANAC_COVERAGE_TEXT_JS: string;
}

/** @brief One native town object, including prepared selection and display text. */
interface LabTown {
    /** @brief Native display jurisdiction, distinct from the catalogue grouping when necessary. */
    country_code?: string;

    /** @brief Display name supplied by the jurisdiction database. */
    name: string;

    /** @brief Latitude, longitude and elevation are native-authored decimal text. */
    latitude: string;
    longitude: string;
    elevation: string;

    /** @brief Named time zone used for browser offset lookup. */
    timezone: string;

    /** @brief Whether this town is the jurisdiction's default selection. */
    default: boolean;

    /** @brief Selection key prepared by the native forms module. */
    value: string;

    /** @brief Coordinate summary prepared by the native forms module. */
    detail: string;
}

/** @brief Town arrays indexed by jurisdiction code, including country-level fallbacks. */
interface LabTownCatalogue {
    [jurisdiction: string]: LabTown[];
}

/** @brief Consumed jurisdiction catalogue fields, including database availability. */
interface LabCatalogue {
    /** @brief Decimal database cursor for bounded town responses; zero terminates pagination. */
    next?: string;
    /** @brief Whether the native jurisdiction database supplied the catalogue. */
    available: boolean;

    /** @brief Menu entries are native two-element arrays of code and display label. */
    options: [code: string, label: string][];

    /** @brief Town objects grouped by jurisdiction code. */
    towns: LabTownCatalogue;

    /** @brief Explanation supplied when the jurisdiction database is unavailable. */
    error?: string;
}

/** @brief Browser startup completion promises; neither resolves to a result payload. */
interface Window {
    /** @brief Completion of application startup, rejecting if bootstrap fails. */
    labReady: Promise<void>;

    /** @brief Completion of native state restoration and the initial evaluation workflow. */
    labInitialEvaluation: Promise<void>;
}
