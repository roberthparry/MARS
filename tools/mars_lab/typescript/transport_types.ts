/**
 * @file transport_types.ts
 * @brief Contracts for native numeric exports and browser continuations.
 *
 * Describes the ABI, scoped host services and native effect plans shared by browser
 * adapters. Opaque payloads remain unknown until a consuming adapter validates them.
 */
/** Contracts at the native numeric ABI and browser-owned continuation boundary. */
type MarsTransportNumericExport = (...arguments_: number[]) => number;
type MarsTransportExports = {memory: WebAssembly.Memory} & Record<`lab_${string}`, MarsTransportNumericExport>;
type MarsTransportService = (...arguments_: never[]) => unknown;
type MarsTransportServices = Readonly<Record<string, MarsTransportService>>;
type NativeServicePlan = MarsTransportPlan;
type NativeServiceRegistry = MarsTransportServices;
interface MarsTransportPlan {
    calls?: {service: string; args: unknown[]}[];
    done?: boolean;
    value?: unknown;
    wait?: boolean;
    save?: string;
    prevent?: boolean;
    stop?: boolean;
}
type MarsTransportCodec = import('./codec.js').Codec;
type MarsTransportJobs = import('./jobs.js').Jobs;
type MarsTransportCodecModule = typeof import('./codec.js');
type MarsTransportJobsModule = typeof import('./jobs.js');
type MarsTransportBuffer = import('./codec_types.js').CodecBuffer;
interface MarsTransportResponse extends Response {
    /** Consume and decode one bounded Protobuf response body. */
    labData(): Promise<unknown>;
}

/** Narrow worksheet capabilities supplied after native bootstrap loads the UI. */
interface LabDOMPresentation {
    /** Return a native card identifier, or minus one for an unregistered element. */
    card_id(element: HTMLElement): number;
    /** Request deferred fitting without entering a nested native DOM scope. */
    schedule_fit(): void;
}

/** Native decoding capabilities required by the HTTP client. */
interface LabResponseDecoder {
    /** Return the current native limit for one response body, in bytes. */
    message_limit(): number;
    /** Decode an owned buffer, observing the request's cancellation signal. */
    decode(buffer: ArrayBuffer, signal?: AbortSignal | null): Promise<unknown>;
}

/** Browser constructors used to validate handles in their owning document's realm. */
interface MarsTransportDOMTypes {
    Node: Node;
    Element: Element;
    HTMLElement: HTMLElement;
    SVGElement: SVGElement;
    Document: Document;
    DocumentFragment: DocumentFragment;
    EventTarget: EventTarget;
    SVGSVGElement: SVGSVGElement;
    SVGGraphicsElement: SVGGraphicsElement;
    HTMLInputElement: HTMLInputElement;
    HTMLTextAreaElement: HTMLTextAreaElement;
    HTMLSelectElement: HTMLSelectElement;
    HTMLButtonElement: HTMLButtonElement;
}
