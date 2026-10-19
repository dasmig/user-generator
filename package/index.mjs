// dasmig user generator, browser and worker entry. The module finds
// user.wasm next to itself (new URL('user.wasm', import.meta.url)),
// which bundlers follow; pass locateFile or wasmBinary to load it otherwise.
import createUserModule from './user.mjs';
import { createGenerator } from './generator.mjs';
import { createPool, poolSize } from './pool.mjs';

export function createUserGenerator(moduleOptions = {}) {
  return createGenerator(createUserModule, moduleOptions);
}

// A pool of module Web Workers (pool-worker.mjs, next to this file) that
// generates batches in pieces and joins them in order. Each worker loads its
// own copy of the module, tens of MB, so the default is at most 4.
export async function createUserPool(options = {}) {
  const size = poolSize(options, Math.min(4, globalThis.navigator?.hardwareConcurrency ?? 2));
  return createPool(() => {
    const worker = new Worker(new URL('./pool-worker.mjs', import.meta.url), { type: 'module' });
    return {
      send: message => worker.postMessage(message),
      onMessage: handler => { worker.onmessage = event => handler(event.data); },
      onError: handler => { worker.onerror = event => handler(new Error(event.message || 'pool worker failed')); },
      terminate: () => worker.terminate()
    };
  }, size);
}
