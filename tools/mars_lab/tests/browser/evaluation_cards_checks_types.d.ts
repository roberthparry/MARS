/**
 * @file evaluation_cards_checks_types.d.ts
 * @brief Evaluation-card presentation fixtures and browser entry points.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** Run card-selection and DOM-restoration regressions after browser readiness. */
    checkLabEvaluationCards(): Promise<void>;
}

/** Native card fields inspected by the browser fixtures. */
interface EvaluationCardsFixture extends WorksheetTextCards {
    derivative: string;
    expandable: boolean;
    solver_source: string;
    scalar: string;
    calendar: WorksheetCalendarCard[];
    unchanged: boolean;
}

/** DOM contents and attributes restored after a card fixture. */
interface EvaluationCardsSnapshot {
    node: HTMLElement & {value?: string};
    children: ChildNode[];
    value: string | undefined;
    attributes: [string, string][];
}

/** Captured arguments from the card action adapter. */
interface EvaluationCardsInvocation {
    source: string;
    target: string;
    start: LabTestRecord | undefined;
    options: WorksheetRequests.MarsApiGoalOptions | undefined;
}
