/**
 * @file surface_types.ts
 * @brief Shared native plot and browser rendering contracts.
 *
 * Describes geometry, camera state, retained frames and DOM capabilities consumed by
 * the plot controllers. These declarations introduce no runtime state.
 */
/** Shared native surface contracts. These classic-script types have no runtime module dependency. */
interface MarsTransportDOMTypes {
    HTMLCanvasElement: HTMLCanvasElement;
}

type MarsSurfacePoint2 = [number, number];
/** @brief Native screen-space hit and approximate sample readout. */
interface MarsSurfaceHoverHit { x: number; y: number; distance: number; text: string; }
type MarsSurfacePoint3 = [number, number, number];
type MarsSurfaceQuaternion = [number, number, number, number];
type MarsSurfaceAngles = number[];
interface MarsSurfaceCameraState { scale: number; x: number; y: number; }
interface MarsSurfaceVolumeState {
    base: number[];
    bounds: number[];
    units: number[];
    original: string[];
    exact: string[];
    sampled: number[];
    labelWidth: string;
    exponent: number;
    sampleExponent: number;
    autoHeight: boolean;
    dirty?: boolean;
    started?: boolean;
}
interface MarsSurfaceSnapshot {
    key: string;
    identity: string;
    camera: MarsSurfaceCameraState;
    orientation: MarsSurfaceQuaternion;
    angles: MarsSurfaceAngles;
    volume: MarsSurfaceVolumeState;
    refining?: boolean;
}
interface MarsSurfaceGesture {
    active?: boolean;
    wheel?: boolean;
    busy?: boolean;
    slider?: boolean;
    trackball?: boolean;
    rotationStart?: MarsSurfaceQuaternion;
    rotationBox?: DOMRect;
    rotationCancelled?: boolean;
    rollOnly?: boolean;
    x: number;
    y: number;
    touchX: number;
    touchY: number;
    surfaceExponent?: number;
    pinchDistance?: number;
    pinchCancelled?: boolean;
    secondX?: number;
    secondY?: number;
}
interface MarsSurfacePrimitive {
    points: MarsSurfacePoint3[];
    colour: string;
    depth: number;
}
interface MarsSurfacePolygon { points: MarsSurfacePoint2[]; depth: number; }
interface MarsSurfaceFrame {
    matrix: number[];
    rows: number[];
    scaling: number[];
    offset: number[];
    width: number;
    height: number;
    fit: number;
    drawMesh: boolean;
    paths: number[][];
    texts: {text: string; x: number; y: number; align: CanvasTextAlign}[];
}
/** The small drawing protocol shared by Canvas and vector axis output. */
interface MarsSurfaceAxisContext {
    strokeStyle: string | CanvasGradient | CanvasPattern;
    fillStyle: string | CanvasGradient | CanvasPattern;
    lineWidth: number;
    font: string;
    textAlign: CanvasTextAlign;

    /** @brief Clear the requested drawing region; vector-only contexts may have no backing pixels to clear. */
    clearRect(x: number, y: number, width: number, height: number): void;

    /** @brief Discard the current path before recording a new axis or tick segment. */
    beginPath(): void;

    /** @brief Start a new subpath at the supplied projected coordinates. */
    moveTo(x: number, y: number): void;

    /** @brief Append a straight segment to the current projected path. */
    lineTo(x: number, y: number): void;

    /** @brief Draw the recorded path using the current stroke colour and width. */
    stroke(): void;

    /** @brief Draw an axis label using the current font, alignment and fill colour. */
    fillText(text: string, x: number, y: number): void;
}
interface MarsSurfaceView {
    chart: SVGSVGElement;
    host: HTMLElement;
    holder: HTMLDivElement;
    primitives: MarsSurfacePrimitive[];
    vertices: Float32Array;
    units: number[];
    width: number;
    height: number;
    overlay: HTMLDivElement;
    gpu: MarsSurfaceGPU | null;
    context: CanvasRenderingContext2D | null;
    ink: CanvasRenderingContext2D;
    ticks: {axis: number; fraction: number; label: string}[];
    names: string[];
    observer: MutationObserver;
    resizeObserver?: ResizeObserver;
    hidden: [SVGElement, string][];
    camera: MarsSurfaceCameraState;
    angles: MarsSurfaceAngles;
    orientation: MarsSurfaceQuaternion;
    volume: MarsSurfaceVolumeState;
    frame: number;
    settleTimer?: number;
    settled: SVGGElement | null;
    framePlan?: MarsSurfaceFrame;
    /** @brief Project a native three-dimensional point through the retained camera frame. */
    project?: (point: MarsSurfacePoint3) => MarsSurfacePoint2;

    /** @brief Project polygon vertices and retain their depth for rendering order. */
    polygon?: (points: MarsSurfacePoint3[]) => MarsSurfacePolygon;

    /** @brief Draw the retained native axes and labels through a canvas-compatible context. */
    paintAxes?: (context: MarsSurfaceAxisContext) => void;
}
/** Native initialisation attaches the volume before a view becomes renderable. */
type MarsSurfaceInitialView = Omit<MarsSurfaceView, 'volume'> & {volume?: MarsSurfaceVolumeState};
/** Capabilities used by a refinement owner without access to worksheet singleton state. */
interface MarsSurfaceVolumeHost {
    readonly native: Pick<LabDOMBridge, 'call'>;
    /** Read the native generation used to reject superseded refinement. */
    generation(): ReturnType<typeof WorksheetRequests.labRequests.latestMain>;
    /** Cancel only the plot request managed by the request owner. */
    cancel_plot(): void;
    /** Cancel the plot controller's pending control debounce. */
    cancel_controls(): void;
    /** Read the current mode at the moment refinement is scheduled. */
    mode(): WorksheetMode;
    /** Read the authored expression without interpreting its mathematics. */
    source(): string;
    /** Prepare retained geometry for a connected chart, or return null. */
    prepare(chart: SVGSVGElement | null): MarsSurfaceView | null;
    /** Request native resampling, retaining the originating chart for targeted replacement. */
    refine(variable: string, chart?: SVGSVGElement): unknown;
}
/** Retained-renderer dependencies resolved lazily during classic-script startup. */
interface MarsSurfacePreviewHost {
    readonly native: Pick<LabDOMBridge, 'call'>;
    readonly camera: Pick<MarsSurfaceCamera, 'fromAngles' | 'drag' | 'angles'>;
    readonly volume: Pick<MarsSurfaceVolume, 'interacting' | 'clone' | 'initialise' | 'set'>;
    readonly exporter: Pick<MarsSurfaceExporter, 'layer' | 'svg'>;
}
