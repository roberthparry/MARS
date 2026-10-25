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

/** @brief Minimal native volume input used without allocating browser rendering resources. */
interface LabTestSurfaceVolumeFixture {
    chart: SVGSVGElement;
    units: number[];
    volume: MarsSurfaceVolumeState;
}

/** @brief Volume fixture with the camera needed for zoom and rollback policies. */
interface LabTestSurfaceCameraFixture extends LabTestSurfaceVolumeFixture {
    camera: MarsSurfaceCameraState;
}

interface LabTestCalendarSection { rows: unknown[]; [key: string]: unknown; }
type LabTestCalendarCard = 'overview' | 'range' | 'calendar' | 'solar' | 'local';

/** @brief Native calendar cards and their matching section lists, written out for fixture readers. */
interface LabTestCalendarEvaluation {
    ok: boolean;
    overview: string;
    range: string;
    calendar: string;
    solar: string;
    local: string;
    overview_sections: LabTestCalendarSection[];
    range_sections: LabTestCalendarSection[];
    calendar_sections: LabTestCalendarSection[];
    solar_sections: LabTestCalendarSection[];
    local_sections: LabTestCalendarSection[];
}

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

/** @brief A complete or deliberately incomplete native reply used by the result tests. */
interface LabTestAlmanacReply {
    ok: boolean;
    visibility?: string;
    almanac_presentation?: {all: LabTestAlmanacVariant; visible: LabTestAlmanacVariant};
    all_rows?: unknown[];
    events?: unknown[];
    [name: string]: unknown;
}

interface Window {
    labAlmanacFixtures(): [LabTestAlmanacFixture, LabTestAlmanacFixture, LabTestCalendarSection[], string, LabTestAlmanacFixture];
}
