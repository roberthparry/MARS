/**
 * @file main_check_types.d.ts
 * @brief Plot, calendar and almanac fixtures for the main browser harness.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Records produced by the native plot controllers and fixture generator. */
interface LabTestPlotGesture {
    busy: boolean;
    active: boolean;
    wheel: boolean;
    pinch: boolean;
    changed: boolean;
    pinchGeometry: number[];
}
interface LabTestPlotHost extends HTMLElement {
    marsPlotDrag: LabTestPlotGesture;
    marsPlotZoom?: LabTestPlotGesture;
    marsComplexPlaneBounds?: string[];
    marsComplexExpanded?: string;
}
interface LabTestPlotInput extends HTMLInputElement {
    marsZoomValue?: string;
    marsZoomText?: string;
}
interface LabTestHoverPath extends SVGPathElement { marsHoverSegment?: unknown; }
interface LabTestPlotTitle extends SVGTitleElement { marsPlotLabel: string; }
interface LabTestCalendarSection { rows: unknown[]; [key: string]: unknown; }
type LabTestCalendarCard = 'overview' | 'range' | 'calendar' | 'solar' | 'local';
type LabTestCalendarEvaluation = {ok: boolean} & Record<LabTestCalendarCard, string> &
    Record<`${LabTestCalendarCard}_sections`, LabTestCalendarSection[]>;
interface LabTestSavedPlotResult extends WorksheetResultSnapshot {
    plotRendered: {innerHTML: string};
}
interface LabTestAlmanacFixture {
    visibility: string;
    almanac_presentation: {all: LabTestAlmanacVariant; visible: LabTestAlmanacVariant};
    all_rows: unknown[];
    events: unknown[];
    [key: string]: unknown;
}
interface Window {
    labAlmanacFixtures(): [LabTestAlmanacFixture, LabTestAlmanacFixture, LabTestCalendarSection[]];
}
