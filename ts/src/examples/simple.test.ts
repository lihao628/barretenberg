import { eccNewPippenger, examplesSimpleCreateAndVerifyProof } from '../bindings/index.js';
import { wasm } from '../call_wasm_export/index.js';
import { Crs } from '../index.js';
import debug from 'debug';

describe('simple', () => {
  it('should construct proof', async () => {
    wasm.on('log', debug('wasm'));
    debug.enable('wasm');

    const crs = new Crs(2 ** 19);
    await crs.init();
    const pippengerPtr = eccNewPippenger(Buffer.from(crs.getG1Data()), crs.numPoints);
    const valid = examplesSimpleCreateAndVerifyProof(pippengerPtr, Buffer.from(crs.getG2Data()));
    expect(valid).toBe(true);
  });
});