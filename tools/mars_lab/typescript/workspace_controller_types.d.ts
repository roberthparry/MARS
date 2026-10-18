/**
 * @file workspace_controller_types.d.ts
 * @brief Explicit capabilities borrowed by worksheet workspace controllers.
 *
 * The composition root supplies these dependencies without reading controls during
 * construction. Shared native calls and exports come from workspace_native.ts;
 * native continuations return opaque results, while synchronous state operations
 * retain their checked frames and return types. No runtime state is declared here.
 */
declare namespace WorksheetWorkspace {
    /** @brief Native continuation capability shared by asynchronous controllers. */
    interface NativeFlow {
        /** @brief Advance a native workflow with its browser-owned frame. */
        flow(kind: number, frame: object): Promise<unknown>;
    }

    /** @brief Callable state operations used by workspace controllers. */
    interface WorkspaceSync {
        /** @brief Restore the editor at the supplied native mode index. */
        (kind: 4, frame: WorksheetStateIndexFrame): void;

        /** @brief Restore a result or apply the selected mode. */
        (kind: 5 | 7, frame: WorksheetStateModeFrame): void;

        /** @brief Select a mode and report whether native policy accepted it. */
        (kind: 6, frame: WorksheetStateSelectionFrame): boolean;

        /** @brief Dispatch keyboard evaluation or flush pending saves. */
        (kind: 8 | 12, frame: WorksheetStateEmptyFrame): void;

        /** @brief Retain a native history entry. */
        (kind: 10, frame: WorksheetStateEntryFrame): void;

        /** @brief Normalise a supplied control using native configuration. */
        (kind: 11, frame: WorksheetStateControlsFrame): void;

        /** @brief Project integrator result metadata. */
        (kind: 14, frame: WorksheetStateDataFrame): void;
    }

    /** @brief Mode lookup shared by presentation and binding selection. */
    interface ModeAccess {
        /** @brief Resolve a mode name to its native index. */
        workspaceModeId(mode: unknown): number;

        /** @brief Read the active native mode. */
        currentMode(): WorksheetMode;
    }

    /** @brief Retained result snapshots; every mode may be absent before evaluation. */
    interface WorkspaceResultStates {
        expression?: WorksheetResultSnapshot;
        equation?: WorksheetResultSnapshot;
        diffequation?: WorksheetResultSnapshot;
        matrix?: WorksheetResultSnapshot;
        integrator?: WorksheetResultSnapshot;
        datetime?: WorksheetResultSnapshot;
        almanac?: WorksheetResultSnapshot;
    }

    /** @brief Form metadata, row projection and guarded bounds restoration. */
    interface IntegratorDependencies extends NativeFlow {
        readonly dom: WorksheetNativeCalls; /**< Scoped native projection. */
        readonly sync: WorkspaceSync; /**< Synchronous result installation. */
        readonly forms: WorksheetIntegratorForms; /**< Concrete owner exposed by the public forms getter. */
        readonly wire: {
            /** @brief Validate the interval cap against its catalogue fallback. */
            intervals(value: number, fallback: number): number;
        };
        readonly controls: { // Editor input and structured integration controls.
            readonly expr: HTMLTextAreaElement;
            readonly integratorBoundStack: HTMLElement;
            readonly integratorIntervalCap: HTMLInputElement;
        };
        readonly defaults: { // Bounds and numerical interval budget from the catalogue.
            readonly DEFAULT_INTEGRATOR_BOUNDS_TEXT: string;
            readonly DEFAULT_INTEGRATOR_INTERVAL_CAP: number;
        };
        readonly locations: {
            /** @brief Parse a form value using number-input semantics. */
            marsFormNumber(value: unknown): number;
        };
        readonly bindingEditor: {
            /** @brief Read the authored expression retained by the editor. */
            currentExpressionText(): string;
        };
        readonly bindingIntegratorPanel: {
            /** @brief Render native-approved integration rows. */
            renderIntegratorRows(rows: WorksheetIntegratorRow[]): void;
        };
    }

    /** @brief Mode selection, editor capture and control normalisation. */
    interface ModeDependencies extends NativeFlow {
        readonly wire: WorksheetNativeExports; /**< Native mode lookup and selection. */
        readonly sync: WorkspaceSync; /**< Synchronous mode and control transitions. */
        readonly equationVariable: null; /**< Retained control slot for normalisation. */
        readonly configuration: LabConfiguration; /**< Native control-validation defaults. */
        readonly modeNames: readonly WorksheetMode[]; /**< Catalogue in native index order. */
        readonly controls: { // Inputs forwarded to the native normalisation frame.
            readonly matrixOperation: WorksheetSelect;
            readonly integratorIntervalCap: HTMLInputElement;
        };
        readonly defaults: { // Authored starting text for each mode.
            readonly DEFAULT_EXPRESSION_TEXT: string;
            readonly DEFAULT_EQUATION_TEXT: string;
            readonly DEFAULT_DIFFEQUATION_TEXT: string;
            readonly DEFAULT_MATRIX_TEXT: string;
            readonly DEFAULT_INTEGRATOR_TEXT: string;
            readonly DEFAULT_DATETIME_TEXT: string;
            readonly DEFAULT_ALMANAC_TEXT: string;
        };
        readonly requests: {
            /** @brief Run a mode-selection continuation under UI request ownership. */
            runUI(operation: string, mode: WorksheetMode,
                  action: (request: WorksheetRequest) => Promise<unknown>): Promise<unknown>;
        };
        readonly evaluation: {
            /** @brief Start evaluation with optional history and input-reuse settings. */
            evaluateLabMode(mode: WorksheetMode, options?: WorksheetEvaluationOptions): Promise<unknown> | undefined;
        };
        readonly persistence: {
            /** @brief Save the selected mode's current authored source. */
            saveWorksheet(mode: WorksheetMode): void;
        };
        readonly buffer: {
            /** @brief Stage a mode name and return its encoded byte length. */
            stageText(text: string): number;
        };
    }

    /** @brief Native precision reads and asynchronous precision changes. */
    interface PrecisionDependencies extends NativeFlow {
        readonly wire: WorksheetNativeExports; /**< Precision policy and current mode. */
    }

    /** @brief Result capture, control projection and calculus action dispatch. */
    interface PresentationDependencies {
        readonly dom: WorksheetNativeCalls; /**< Native projection, including batched calculus labels. */
        readonly sync: WorkspaceSync; /**< Retained result restoration. */
        readonly equationVariable: null; /**< Retained control slot for busy-state projection. */
        readonly workspaceMode: ModeAccess; /**< Active mode and native index lookup. */
        readonly editorState: { // Binding metadata captured with each result.
            readonly currentVariables: string[];
            readonly currentDifferentiable: boolean;
        };
        readonly controls: { // Browser controls projected or subscribed by the presenter.
            readonly modeTabs: HTMLElement[];
            readonly matrixOperation: WorksheetSelect;
            readonly helpCards: HTMLElement[];
            readonly copyButtons: WorksheetCopyButton[];
            readonly moreDigitButtons: WorksheetCopyButton[];
            readonly derivativeButtons: HTMLElement;
        };
        readonly defaults: { readonly ALMANAC_COVERAGE_TEXT: string; }; /**< Native coverage text. */
        readonly surfacePreview: {
            /** @brief Capture result metadata while preserving the interactive surface view. */
            captureResult(capture: () => WorksheetResultSnapshot): WorksheetResultSnapshot;
        };
        readonly functionCard: {
            /** @brief Clear the retained function execution. */
            clearFunctionRun(): void;
        };
        readonly calculus: {
            /** @brief Request a derivative for the supplied variable and action button. */
            takeDerivative(wrt: string, actionButton?: HTMLButtonElement | null): Promise<unknown>;

            /** @brief Request an integral for the supplied variable and action button. */
            takeIntegral(wrt: string, actionButton?: HTMLButtonElement | null): Promise<unknown>;

            /** @brief Request a plot for the supplied variable and action button. */
            takePlot(wrt: string, actionButton?: HTMLButtonElement | null): Promise<unknown>;
        };
        readonly history: {
            /** @brief Read the active mode's backward history count. */
            backwardCount(): number;

            /** @brief Read the active mode's forward history count. */
            forwardCount(): number;
        };
        readonly locations: {
            /** @brief Refresh the enhanced selector from its native control. */
            syncRoundedSelect(select: WorksheetSelect): void;
        };
        readonly workspaceLayout: {
            /** @brief Schedule an editor geometry update. */
            scheduleEditorResizeGrip(): void;
        };
        readonly bindingEditor: {
            /** @brief Report whether the current expression is ready for evaluation. */
            expressionReadyToEvaluate(): boolean;
        };
        readonly workspacePrecision: {
            /** @brief Report whether precision cannot decrease further. */
            atMinimumPrecision(): boolean;

            /** @brief Report whether precision cannot increase further. */
            atMaximumPrecision(): boolean;
        };
        readonly bindingGoal: {
            /** @brief Report whether bindings permit goal seeking. */
            canGoalSeek(): boolean;
        };
    }

    /** @brief Editor geometry inputs and animation-frame ownership. */
    interface LayoutDependencies {
        readonly dom: WorksheetNativeCalls; /**< Native editor geometry projection. */
        readonly controls: { readonly labTextareas: HTMLTextAreaElement[]; }; /**< Resizable editors. */
        readonly viewportHeight: number; /**< Current browser viewport height. */

        /** @brief Cancel a queued geometry update. */
        cancelFrame(handle: number): void;

        /** @brief Queue a geometry update and return its cancellable frame handle. */
        requestFrame(callback: FrameRequestCallback): number;
    }

    /** @brief History insertion and pending-save publication. */
    interface HistoryDependencies {
        readonly sync: WorkspaceSync; /**< Native history and persistence commands. */
    }

    /** @brief Native binding visibility for the active mode. */
    interface BindingSelectionDependencies {
        readonly dom: WorksheetNativeCalls; /**< Native variable and binding selection. */
        readonly workspaceMode: ModeAccess; /**< Active mode and native index lookup. */
        readonly workspaceIntegrator: {
            /** @brief Read names bound by the active integration rows. */
            currentIntegratorBoundNames(): Set<string>;
        };
    }
}
