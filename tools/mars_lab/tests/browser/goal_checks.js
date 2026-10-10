/** Goal-seek C/WASM completion plans and asynchronous host-boundary regressions. */
window.checkLabGoal = async function checkLabGoal() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Goal: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const complete = (outcome, data = {}, context = {}, error = null) =>
        labDOM.call('lab_goal_complete', outcome, data, context, error);
    const failureServices = 'setRenderedError,resetRenderedDigits,clearResultDetails,setStatus';
    const successServices = 'captureHistory,setRenderedResult,setEditor,installCards,setLastInput,clearDerivative,' +
        'setVariables,setDifferentiable,renderDerivatives,setSource,setTarget,hideTargetEntry,setSuccessStatus';
    for (const outcome of [0, -1, 5, 0x7fffffff])
        equal(complete(outcome), null, 'stale or invalid outcome cannot produce effects');
    for (const error of [undefined, null, false, 0, '', ' precise μ\u0000diagnostic ']) {
        const plan = complete(1, {error});
        equal(plan.result, false, 'rejected result is false');
        equal(plan.calls.map(call => call.service).join(','), failureServices, 'failure service order');
        equal(plan.calls[0].args[0], error || 'Goal seek failed', 'native diagnostic truthiness without trimming');
        equal(plan.calls[2].args[0].keepBindings, true, 'failure retains authored bindings');
        equal(plan.calls[3].args[0], 'Error', 'failure status');
    }
    for (const error of ['', 'Error: exact exception']) {
        const plan = complete(4, {error: 'ignored'}, null, error);
        equal(plan.calls[0].args[0], error, 'explicit exception takes precedence, even when empty');
    }
    for (const outcome of [2, 3]) {
        for (const skipHistoryUpdate of [false, true]) {
            let reads = 0;
            const data = {
                get binding_values() {
                    ++reads;
                    throw new Error('premature binding read');
                }
            };
            const cards = {
                get unchanged() {
                    ++reads;
                    throw new Error('premature status read');
                }
            };
            const context = {options: {skipHistoryUpdate}, cards};
            const plan = complete(outcome, data, context);
            equal(reads, 0, 'planning must not evaluate deferred getters');
            equal(plan.result, true, 'accepted result is true');
            equal(
                plan.calls.map(call => call.service).join(','),
                skipHistoryUpdate ? successServices.slice('captureHistory,'.length) : successServices,
                'success effects retain execution order');
            const editor = plan.calls.find(call => call.service === 'setEditor');
            equal(editor.args[0], data, 'actual response identity is preserved');
            equal(editor.args[1], context, 'actual completion context is preserved');
        }
    }
    for (const unchanged of [false, null, undefined, 0, '', true, 'yes'])
        equal(
            labDOM.call('lab_goal_status', {unchanged}), unchanged ? 'Goal already reached' : 'Goal reached',
            'native successful status selection');

    const savedFunctions = new Map();
    const savedRequests = {...labRequests};
    const savedCall = labDOM.call;
    const saved = {
        lastInput: labEditorState.lastInput,
        source: labEditorState.goalSource,
        target: labEditorState.goalTarget,
        derivative: lastDerivativeExpression,
        variables: currentVariables,
        differentiable: currentDifferentiable
    };
    const events = [];
    let live = true, outcome = 2, postError = null, editorResult = 'bound native μ', data = {}, cards = {};
    const request = {};
    const replace = (name, callback) => {
        if (!savedFunctions.has(name))
            savedFunctions.set(name, window[name]);
        window[name] = callback;
    };
    const record = name => (...args) => {
        events.push([name, ...args]);
    };
    const named = name => events.filter(event => event[0] === name);
    try {
        labRequests.runUI = async (operation, mode, callback) => {
            equal(operation, 'goal', 'goal request channel');
            equal(mode, 'expression', 'goal request mode');
            return callback(request);
        };
        labRequests.current = value => {
            equal(value, request, 'request identity');
            return live;
        };
        labRequests.input = (_, source) => {
            events.push(['input', source]);
            return !!source;
        };
        labRequests.outcome = () => live ? outcome : 0;
        labRequests.post = async (_, payload, route) => {
            events.push(['post', payload, route]);
            if (postError)
                throw postError;
            return {response: {ok: true}, data};
        };
        labDOM.call = (name, ...args) => {
            if (name === 'lab_evaluation_cards')
                return cards;
            // These presentation adapters are lexical constants, not replaceable window functions.
            if (name === 'lab_layout_error')
                return record('setRenderedError')(args[0]);
            if (name === 'lab_layout_more')
                return record('resetMoreDigitsButton')(...args);
            return savedCall(name, ...args);
        };
        for (const name
                 of ['setStatus', 'clearResultDetails', 'pushExpressionHistory', 'setRenderedResult',
                     'setExpressionEditor', 'installEvaluationTextCards', 'hideTargetEntry'])
            replace(name, record(name));
        replace('commitVisibleBindingInputs', async () => events.push(['commit']));
        replace('currentExpressionText', () => {
            events.push(['currentText']);
            return 'current authored α';
        });
        replace('goalSeekExpressionAndStarts', async (text, start) => ({expression: text, start}));
        replace('expressionForEvaluation', text => text);
        replace('requestedValuePrecision', () => 53);
        replace('expressionBodyForEditor', text => {
            events.push(['body', text]);
            return 'body ' + text;
        });
        replace('expressionWithBindings', async (body, bindings) => {
            events.push(['bind', body, bindings]);
            return editorResult;
        });
        replace('variableNamesFromBindings', bindings => {
            events.push(['variables', bindings]);
            return ['μ'];
        });
        replace('renderDerivativeButtons', variables => {
            events.push(['derivatives', variables, lastDerivativeExpression, currentDifferentiable]);
        });
        replace('expressionForEditor', text => {
            events.push(['goalSource', text]);
            return '  ' + text + '  ';
        });
        const bindings = [{name: 'μ', value: '1/3'}];
        data = {
            get binding_values() {
                events.push(['bindings']);
                return bindings;
            },
            get evaluation_ready() {
                events.push(['ready']);
                return true;
            }
        };
        cards = {
            get differentiable() {
                events.push(['differentiable']);
                return false;
            },
            get unchanged() {
                events.push(['unchanged']);
                return true;
            }
        };
        equal(await runGoalSeek('opaque α\u0000β', '7/3'), true, 'successful asynchronous adapter result');
        equal(named('post')[0][1].expression, 'opaque α\u0000β', 'request source remains opaque');
        equal(named('post')[0][1].target, '7/3', 'target remains exact text');
        equal(named('post')[0][2], '/goal_seek', 'goal route');
        const historyIndex = events.findIndex(event => event[0] === 'pushExpressionHistory');
        equal(
            events.slice(historyIndex - 1).map(event => event[0]).join(','),
            'currentText,pushExpressionHistory,setRenderedResult,bindings,ready,setExpressionEditor,' +
                'installEvaluationTextCards,bindings,variables,differentiable,derivatives,goalSource,' +
                'hideTargetEntry,unchanged,setStatus',
            'getter and effect timing preserved through native plan');
        equal(named('pushExpressionHistory')[0][1], 'current authored α', 'history reads current editor');
        equal(named('setExpressionEditor')[0][2], bindings, 'editor bindings retain identity');
        equal(named('derivatives')[0][2], '', 'derivative reset precedes controls');
        equal(named('derivatives')[0][3], false, 'differentiable state precedes controls');
        equal(labEditorState.lastInput, 'bound native μ', 'last input retains reconstructed editor text');
        equal(labEditorState.goalSource, 'opaque α\u0000β', 'goal source conversion retains opaque text');
        equal(labEditorState.goalTarget, '7/3', 'goal target remains exact');
        equal(named('setStatus').at(-1)[1], 'Goal already reached', 'late native status selection');

        events.length = 0;
        editorResult = '';
        data = {};
        cards = {};
        equal(
            await runGoalSeek('source', '0', {}, {skipHistoryUpdate: true, commitBindings: true}), true,
            'committed source with history suppressed');
        equal(named('pushExpressionHistory').length, 0, 'skip history suppresses capture');
        equal(named('post')[0][1].expression, 'current authored α', 'commit option rereads the source');
        equal(named('setExpressionEditor')[0][1], 'body current authored α', 'empty reconstruction uses body');
        equal(named('setExpressionEditor')[0][2], null, 'missing editor bindings remain null');
        equal(named('variables')[0][1].length, 0, 'missing derivative bindings use empty array');
        equal(named('setStatus').at(-1)[1], 'Goal reached', 'changed result status');

        for (const exception of [false, true]) {
            events.length = 0;
            outcome = 1;
            data = {error: ' precise native error '};
            postError = exception ? new Error('transport') : null;
            equal(await runGoalSeek('source', '0'), false, 'failed completion result');
            equal(
                named('setRenderedError')[0][1], exception ? 'Error: transport' : data.error,
                'adapter retains diagnostic precedence');
            equal(named('clearResultDetails')[0][1].keepBindings, true, 'adapter failure keeps bindings');
            equal(named('setExpressionEditor').length, 0, 'failed completion does not replace editor');
        }
        postError = null;
        outcome = 2;
        events.length = 0;
        replace('expressionWithBindings', async () => {
            live = false;
            return 'late result';
        });
        equal(await runGoalSeek('source', '0'), false, 'late reconstruction is rejected');
        equal(
            named('setExpressionEditor').length + named('pushExpressionHistory').length, 0,
            'stale reconstruction cannot publish effects');
        equal(named('setStatus').length, 1, 'stale reconstruction leaves completion status untouched');

        live = true;
        events.length = 0;
        replace('commitVisibleBindingInputs', async () => {
            live = false;
        });
        equal(await runGoalSeek('source', '0'), false, 'stale binding commit is rejected');
        equal(named('post').length, 0, 'stale binding commit never sends a request');
    } finally {
        for (const [name, callback] of savedFunctions) window[name] = callback;
        Object.assign(labRequests, savedRequests);
        labDOM.call = savedCall;
        labEditorState.lastInput = saved.lastInput;
        labEditorState.goalSource = saved.source;
        labEditorState.goalTarget = saved.target;
        lastDerivativeExpression = saved.derivative;
        currentVariables = saved.variables;
        currentDifferentiable = saved.differentiable;
    }
};
