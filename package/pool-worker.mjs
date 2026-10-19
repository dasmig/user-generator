// A pool worker for browsers (a module Web Worker started by createUserPool).
import { createUserGenerator } from './index.mjs';
import { serve } from './pool-worker-core.mjs';

serve(await createUserGenerator(), message => self.postMessage(message), handler => {
  self.onmessage = event => handler(event.data);
});
