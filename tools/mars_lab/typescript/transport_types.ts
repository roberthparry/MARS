/**
 * @file transport_types.ts
 * @brief Contracts for native numeric exports and browser continuations.
 *
 * Describes the ABI, scoped host services and native effect plans shared by browser
 * adapters. These declarations have no runtime behaviour. Type-only module references
 * keep this file in the classic-script scope without loading codec or worker modules.
 * Opaque payloads remain unknown until a consuming adapter validates them.
 */

// Native numeric ABI.

/** @brief Native entry point accepting numeric arguments and returning a number. */
interface MarsTransportNumericExport {
    (...args: number[]): number;
}

/** @brief Borrowed WASM memory and numeric application exports whose names begin with lab_. */
type MarsTransportExports = {
    memory: WebAssembly.Memory;
    [entry: `lab_${string}`]: MarsTransportNumericExport;
};

// Browser services and native continuation plans.

/**
 * @brief Registration type for callbacks with different parameter lists and result types.
 * The never[] parameter list accepts those callbacks without claiming that each can
 * handle arbitrary values. Only LabDOMBridge.services supplies native-selected arguments;
 * it checks registry ownership before invoking the callback at that dynamic boundary.
 */
interface MarsTransportService {
    (...args: never[]): unknown;
}

/** @brief Read-only lookup of browser callbacks by their native service names. */
interface MarsTransportServices {
    readonly [name: string]: MarsTransportService;
}

/** @brief One native-selected invocation of a registered browser service. */
interface MarsTransportServiceCall {
    service: string;
    args: unknown[];
}

/** @brief Native instructions for a continuation step or browser event. */
interface MarsTransportPlan {
    /** @brief Services to invoke in order, after leaving the native handle scope. */
    calls?: MarsTransportServiceCall[];

    /** @brief Finish the continuation without dispatching further services. */
    done?: boolean;

    /** @brief Result returned when done is true. */
    value?: unknown;

    /** @brief In an asynchronous continuation, await the final service result before advancing. */
    wait?: boolean;

    /** @brief Frame field receiving the final service result, awaited when wait is set. */
    save?: string;

    /** @brief For browser events, cancel the default action before dispatching services. */
    prevent?: boolean;

    /** @brief For browser events, stop propagation before dispatching services. */
    stop?: boolean;
}

/** @brief Worksheet-facing name for the same native continuation plan. */
type NativeServicePlan = MarsTransportPlan;

/** @brief Worksheet-facing name for the same browser service registry. */
type NativeServiceRegistry = MarsTransportServices;

// Lazy module contracts: use the owning modules' types without loading them here.

/** @brief Codec instance owned by the wire transport after native initialisation. */
type MarsTransportCodec = import('./codec.js').Codec;

/** @brief Background decode controller owned by the wire transport. */
type MarsTransportJobs = import('./jobs.js').Jobs;

/** @brief Exports returned when the codec module is dynamically loaded. */
type MarsTransportCodecModule = typeof import('./codec.js');

/** @brief Exports returned when the worker-job module is dynamically loaded. */
type MarsTransportJobsModule = typeof import('./jobs.js');

/** @brief Non-shared buffer or buffer view accepted by the codec. */
type MarsTransportBuffer = import('./codec_types.js').CodecBuffer;

// Browser network and presentation capabilities.

/** @brief HTTP response with a bounded native Protobuf decoder. */
interface MarsTransportResponse extends Response {
    /** @brief Consume and decode one bounded Protobuf response body. */
    labData(): Promise<unknown>;
}

/** @brief Narrow worksheet capabilities supplied after native bootstrap loads the UI. */
interface LabDOMPresentation {
    /** @brief Return a native card identifier, or minus one for an unregistered element. */
    cardId(element: HTMLElement): number;

    /** @brief Request deferred fitting without entering a nested native DOM scope. */
    scheduleFit(): void;
}

/** @brief Native decoding capabilities required by the HTTP client. */
interface LabResponseDecoder {
    /** @brief Return the current native limit for one response body, in bytes. */
    messageLimit(): number;

    /** @brief Decode an owned buffer, observing the request's cancellation signal. */
    decode(buffer: ArrayBuffer, signal?: AbortSignal | null): Promise<unknown>;
}

/**
 * @brief DOM constructor names mapped to the instance types accepted by marsTransportIsDOM.
 * This is a compile-time lookup, not a stored table of constructors or DOM objects.
 * The runtime check uses the object's own document realm, including iframe documents.
 */
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
