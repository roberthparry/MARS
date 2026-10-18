/**
 * @file projection_checks_types.d.ts
 * @brief Browser entry-point declaration for native projection regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** @brief Check solver-card projection and native persistence records. */
    checkLabProjection(): Promise<void>;
}

/** @brief Browser markup and attributes restored after native projection checks. */
interface ProjectionFixtureElement {
    node: HTMLElement;
    html: string;
    attributes: [string, string][];
}

/** @brief Integrator fields inspected in the server persistence record. */
interface ProjectionFixtureServer {
    integrator_interval_cap: number;
    integrator_bounds: string;
    precision_bits: {expression: number};
}

/** @brief Server and local persistence records returned by the integrator fixture. */
interface ProjectionFixtureRecords {
    server: ProjectionFixtureServer;
    local: {'mars.exprLab.lastIntegratorIntervalCap': number};
}

/** @brief History fields selected for the equation fixture. */
interface ProjectionFixtureHistory {
    variable: string;
    text: string;
    mode: string;
}

/** @brief Calendar fields inspected after native schema projection. */
interface ProjectionFixtureCalendar {
    almanac_date: string;
    almanac_zone: string;
}
