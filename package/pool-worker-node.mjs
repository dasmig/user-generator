// A pool worker for Node (a worker thread started by createUserPool).
import { parentPort } from 'node:worker_threads';
import { createUserGenerator } from './node.mjs';
import { serve } from './pool-worker-core.mjs';

serve(await createUserGenerator(), message => parentPort.postMessage(message), handler => {
  parentPort.on('message', handler);
});
