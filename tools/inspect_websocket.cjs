// Read-only diagnostic. Run manually; never sends set or trigger.
// node inspect_websocket.cjs <ws-module> <url> [layer=3]
'use strict';
const [modulePath, url, layerText = '3'] = process.argv.slice(2);
const layer = Number(layerText);
if (!modulePath || !url || !Number.isInteger(layer) || layer < 1) {
  console.error('Usage: node inspect_websocket.cjs <ws-module> <url> [layer=3]');
  process.exit(1);
}
const endpoint = new URL(url);
if (endpoint.protocol !== 'ws:' || endpoint.hostname !== '127.0.0.1') {
  throw new Error('Use ws://127.0.0.1 with the configured port.');
}
const WebSocket = require(modulePath);
const ws = new WebSocket(url);
let started = false;
const timer = setTimeout(() => {
  if (!started) { console.error('No Composition received.'); process.exitCode = 1; }
  ws.terminate();
}, 10000);
ws.on('message', raw => {
  try {
    const message = JSON.parse(raw.toString());
    if (Array.isArray(message.layers)) {
      console.log(JSON.stringify({type:'composition', received_at:new Date().toISOString()}));
      if (started) return;
      started = true;
      const target = message.layers[layer - 1];
      if (!target) throw new Error(`Layer ${layer} does not exist.`);
      const parameters = {blend_mode:target.transition?.blend_mode,
        duration:target.transition?.duration, maskmode:target.maskmode};
      for (const [name, parameter] of Object.entries(parameters)) {
        if (!Number.isSafeInteger(parameter?.id)) throw new Error(`Missing ${name} parameter ID.`);
      }
      for (const [name, parameter] of Object.entries(parameters)) {
        console.log(JSON.stringify({type:'parameter_mapping', layer, name, ...parameter}));
        ws.send(JSON.stringify({action:'get', parameter:`/parameter/by-id/${parameter.id}`}));
      }
    } else if (!['sources_update', 'effects_update'].includes(message.type)) {
      console.log(JSON.stringify(message));
    }
  } catch (error) { console.error(error.message); process.exitCode = 1; ws.terminate(); }
});
ws.on('error', error => { console.error(error.message); process.exitCode = 1; });
ws.on('close', () => clearTimeout(timer));
process.on('SIGINT', () => ws.terminate());
