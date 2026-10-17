/**
 * @file evaluation_cards_checks_types.d.ts
 * @brief Evaluation-card presentation fixtures and browser entry points.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    checkLabEvaluationCards(): Promise<void>;
}

interface EvaluationCardsFixture extends WorksheetTextCards {
    derivative: string;
    expandable: boolean;
    solver_source: string;
    scalar: string;
    calendar: WorksheetCalendarCard[];
    unchanged: boolean;
}
