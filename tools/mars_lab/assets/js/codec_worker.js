/** Dedicated instance: no DOM imports, shared memory, network payloads or persistence. */
import {createCodec} from './codec.js';

const codec = createCodec();
let ready = false;
self.onmessage = async ({data}) => {
    try {
        if (data.type === 'init' && !ready) {
            const response = await fetch(data.url, {cache: 'no-store'});
            if (!response.ok)
                throw new Error('Could not load the worker codec');
            await codec.start(await response.arrayBuffer());
            ready = true;
            self.postMessage({type: 'ready'});
        } else if (data.type === 'decode' && ready) {
            const value = codec.decode(data.bytes);
            self.postMessage({type: 'result', token: data.token, value});
        } else {
            throw new Error('Invalid codec worker message');
        }
    } catch (error) {
        self.postMessage({type: 'error', token: data.token || 0, message: error.message});
    } finally {
        if (data.type === 'decode' && data.bytes instanceof Uint8Array)
            data.bytes.fill(0);
    }
};
