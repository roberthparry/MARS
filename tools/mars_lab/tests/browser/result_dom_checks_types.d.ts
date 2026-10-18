/**
 * @file result_dom_checks_types.d.ts
 * @brief Browser entry points and mutable capabilities for result DOM regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

/** Entry points for these browser regressions. */
interface Window {
    /** Check result projection, deferred completion and cleanup. */
    checkLabResultDom(): Promise<void>;
}

/** Render response settled explicitly by the browser fixture. */
interface ResultDomRenderResponse {
    svg: string;
}

/** Pending render request with its exact source and settlement callbacks. */
interface ResultDomRenderRequest {
    source: string;

    /** Complete the render with native markup. */
    resolve(value: ResultDomRenderResponse): void;

    /** Fail the render without changing the rejection reason. */
    reject(reason?: unknown): void;
}

/** Optional markup returned by the deferred presentation fixture. */
interface ResultDomPresentationResponse extends WorksheetRequests.MarsApiData {
    html?: string;
}

/** Result node whose disabled state may need restoration. */
interface ResultDomElement extends HTMLElement {
    disabled?: boolean;
}

/** Exact DOM state retained while result checks mutate live cards. */
interface ResultDomSnapshot {
    node: HTMLElement;
    attributes: {name: string; value: string}[];
    children: ChildNode[];
    disabled: boolean | undefined;
    matrixRequest: PropertyDescriptor | undefined;
    renderRequest: PropertyDescriptor | undefined;
}
