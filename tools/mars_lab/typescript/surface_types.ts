/**
 * @file surface_types.ts
 * @brief Shared native plot and browser rendering contracts.
 *
 * Describes geometry, camera state, retained frames and DOM capabilities consumed by
 * the plot controllers. These declarations introduce no runtime state.
 * Geometry records come first, followed by retained view state and the interfaces
 * connecting browser controllers. Native C owns projection and range calculations.
 */

/** Shared native surface contracts. These classic-script types have no runtime module dependency. */
interface MarsTransportDOMTypes {
    HTMLCanvasElement: HTMLCanvasElement;
}

/** Two-dimensional coordinates, ordered x then y. */
type MarsSurfacePoint2 = [number, number];

/** Three-dimensional coordinates, ordered x, y then z. */
type MarsSurfacePoint3 = [number, number, number];

/** Camera quaternion, ordered x, y, z then w. */
type MarsSurfaceQuaternion = [number, number, number, number];

/** Camera angles in degrees: azimuth, elevation and roll. */
type MarsSurfaceAngles = [number, number, number];

/** @brief Native screen-space hit and approximate sample readout. */
interface MarsSurfaceHoverHit {
    x: number;
    y: number;
    distance: number;
    text: string;
}

/** Retained display scale and translation. */
interface MarsSurfaceCameraState {
    scale: number;
    x: number;
    y: number;
}

/** Native coordinate bounds, exact range strings and refinement state. */
interface MarsSurfaceVolumeState {
    base: number[];
    bounds: number[];
    /** Coordinate-box proportions; curves retain these across input-range refinements. */
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

/** Saved plot identity, camera and volume used when replacing a drawing. */
interface MarsSurfaceSnapshot {
    key: string;
    identity: string;
    camera: MarsSurfaceCameraState;
    orientation: MarsSurfaceQuaternion;
    angles: MarsSurfaceAngles;
    volume: MarsSurfaceVolumeState;
    refining?: boolean;
}

/** Pointer and touch state shared with the native gesture controller. */
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

/** Native segment or mesh face, with its display colour and ordering depth. */
interface MarsSurfacePrimitive {
    points: MarsSurfacePoint3[];
    colour: string;
    depth: number;
}

/** Native-decoded primitives and interleaved position, colour and corner attributes. */
interface MarsSurfaceMesh {
    primitives: MarsSurfacePrimitive[];
    vertices: number[];
    units: number[];
}

/** Projected polygon vertices and their rendering depth. */
interface MarsSurfacePolygon {
    points: MarsSurfacePoint2[];
    depth: number;
}

/** Clipped, projected primitive with the original native display colour. */
interface MarsSurfaceDrawing extends MarsSurfacePolygon {
    colour: string;
}

/** Positioned axis label or status text supplied by the native renderer. */
interface MarsSurfaceText {
    text: string;
    x: number;
    y: number;
    align: CanvasTextAlign;
}

/** Axis tick metadata retained from the native SVG. */
interface MarsSurfaceTick {
    axis: number;
    fraction: number;
    label: string;
}

/** Native projection plan shared by interactive drawing and SVG export. */
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
    texts: MarsSurfaceText[];
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

/**
 * @brief Browser rendering resources before native volume initialisation completes.
 * Only volume becomes required when lab_surface_volume_init completes, promoting
 * this record to MarsSurfaceView. Other optional fields have separate lifetimes:
 * framePlan, project, polygons and paintAxes are installed by drawing; drawing is
 * a lazily populated cache. resizeObserver depends on browser support, and
 * settleTimer is assigned when a settled drawing is scheduled.
 */
interface MarsSurfaceInitialView {
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
    ticks: MarsSurfaceTick[];
    names: string[];
    observer: MutationObserver;
    resizeObserver?: ResizeObserver;
    hidden: [SVGElement, string][];
    camera: MarsSurfaceCameraState;
    angles: MarsSurfaceAngles;
    orientation: MarsSurfaceQuaternion;
    volume?: MarsSurfaceVolumeState;
    frame: number;
    settleTimer?: number;
    settled: SVGGElement | null;
    framePlan?: MarsSurfaceFrame;

    /** @brief Project a native three-dimensional point through the retained camera frame. */
    project?(point: MarsSurfacePoint3): MarsSurfacePoint2;

    /** Cached native painter-order plan for the current frame; absent until needed. */
    drawing?: MarsSurfaceDrawing[];

    /** @brief Obtain native-clipped primitives in stable far-to-near drawing order. */
    polygons?(): MarsSurfaceDrawing[];

    /** @brief Draw the retained native axes and labels through a canvas-compatible context. */
    paintAxes?(context: MarsSurfaceAxisContext): void;
}

/** Ready-to-render view: native initialisation has attached the required volume. */
interface MarsSurfaceView extends MarsSurfaceInitialView {
    volume: MarsSurfaceVolumeState;
}

/** Plain browser-to-native boundary used by the redraw scheduler. */
interface MarsSurfaceVolumeNative {
    /** @brief Invoke a native operation; its owning adapter supplies the concrete result type. */
    call(name: string, ...args: unknown[]): unknown;
}

/** Capabilities used by a refinement owner without access to worksheet singleton state. */
interface MarsSurfaceVolumeHost {
    readonly native: MarsSurfaceVolumeNative;

    /** @brief Read the native generation used to reject superseded refinement. */
    generation(): number;

    /** @brief Cancel only the plot request managed by the request owner. */
    cancelPlot(): void;

    /** @brief Cancel the plot controller's pending control debounce. */
    cancelControls(): void;

    /** @brief Read the current mode at the moment refinement is scheduled. */
    mode(): WorksheetMode;

    /** @brief Read the authored expression without interpreting its mathematics. */
    source(): string;

    /** @brief Prepare retained geometry for a connected chart, or return null. */
    prepare(chart: SVGSVGElement | null): MarsSurfaceView | null;

    /** @brief Repaint the retained chart after native rollback without requesting more samples. */
    redraw(view: MarsSurfaceView): void;

    /** @brief Request native resampling, retaining the originating chart for targeted replacement. */
    refine(variable: string, chart?: SVGSVGElement): unknown;
}

/** Retained-renderer dependencies resolved lazily during classic-script startup. */
interface MarsSurfacePreviewHost {
    readonly native: LabDOMBridge;
    readonly camera: MarsSurfaceCamera;
    readonly volume: MarsSurfaceVolume;
    readonly exporter: MarsSurfaceExporter;
}
