import { identifyJedec } from '../src/chipDatabase.js?v=20260912';
import { RflasherAdapter } from '../src/RflasherAdapter.js?v=20260912';
const output = document.querySelector('#results');
const assert = (condition, message) => { if (!condition) throw new Error(message); };
const id = (hex) => Uint8Array.from(hex.match(/../g), (byte) => parseInt(byte, 16));
try {
  // Independently selected upstream facts, including IDs absent from the old DB.
  for (const [hex, model, capacity] of [
    ['684017', 'B.25Q64AS', 8388608], ['684018', 'B.25Q128AS', 16777216],
    ['ef4017', 'W25Q64', 8388608], ['c84017', 'GD25Q64', 8388608],
    ['204017', 'XM25QH64', 8388608], ['20ba17', 'N25Q064', 8388608],
  ]) {
    const chip = identifyJedec(id(hex));
    assert(chip?.model.includes(model) && chip.capacity === capacity, `Incorrect ${hex}`);
  }
  for (const value of [null, [], [0x68, 0x40], [0x68, 0x40, 0x17, 0], [-1, 0, 0],
    [256, 0, 0], [NaN, 0, 0], [1.5, 0, 0], ['104', 64, 23], id('000000'), id('ffffff'),
    id('684016'), id('ef4019'), id('204015')]) {
    assert(identifyJedec(value) === null, `Must reject ${value}`);
  }
  const mutable = identifyJedec(id('684017'));
  mutable.capacity = 1;
  assert(identifyJedec(id('684017')).capacity === 8388608, 'Lookup must not expose database mutations');
  const provenance = await (await fetch('../data/chip-database-provenance.json')).json();
  assert(provenance.entries.length === 225, 'Unexpected database count');
  assert(new Set(provenance.entries.map(([key]) => key)).size === 225, 'Duplicate ID');
  for (const [key, chip] of provenance.entries) {
    assert(identifyJedec(id(key)).capacity === chip.capacity, `Runtime/provenance mismatch ${key}`);
  }

  const adapter = new RflasherAdapter();
  adapter.transport = {}; // No physical port is opened.
  adapter.info = { maxRead: 65536 };
  const calls = [];
  adapter.spiOp = async (command, length) => {
    calls.push([...command]);
    if (command[0] === 0x9f) return id('684017');
    if (command[0] === 0x5a) return new Uint8Array([0x53, 0x46, 0x44, 0x50, 0, 0, 0, 0]);
    return new Uint8Array(length).fill(command[0] === 0x03 ? 0xa5 : 0);
  };
  assert((await adapter.probe()).chip.capacity === 8388608, 'Boya probe failed');
  const dump = await adapter.dumpFull();
  const reads = calls.filter(c => c[0] === 0x03);
  assert(dump.length === 8388608 && dump[0] === 0xa5 && dump.at(-1) === 0xa5, 'Full dump failed');
  assert(reads.length === 128 && reads.at(-1).join(',') === '3,127,0,0', 'Wrong Boya address/chunking');
  await adapter.pageProgram(0x7fff00, new Uint8Array(256));
  assert(calls.at(-2).slice(0,4).join(',') === '2,127,255,0', 'Wrong Boya program command');
  await adapter.eraseChip();
  assert(calls.at(-3)[0] === 0x06 && calls.at(-2)[0] === 0xc7 && calls.at(-1)[0] === 0x05, 'Wrong erase sequence');
  const large = provenance.entries.find(([, chip]) => chip.capacity > 16777216);
  adapter.lastProbe.chip = identifyJedec(id(large[0]));
  assert([...adapter.buildAddressCommand(0x1000000, {read: true})].join(',') === '19,1,0,0,0', 'Wrong 4-byte read');
  assert([...adapter.buildAddressCommand(0x1000000, {read: false})].join(',') === '18,1,0,0,0', 'Wrong 4-byte program');
  adapter.lastProbe.chip = null;
  let rejected = false;
  try { await adapter.dumpFull(); } catch { rejected = true; }
  assert(rejected, 'Unknown chip must block full dump');
  const previousCalls = calls.length;
  rejected = false;
  try { await adapter.eraseChip(); } catch { rejected = true; }
  assert(rejected && calls.length === previousCalls, 'Unknown chip must not issue erase commands');
  output.textContent = 'PASS: sourced IDs, ambiguity exclusions, input validation, provenance, Boya probe/dump/program, native 4-byte commands, unknown-chip guard.';
  document.documentElement.dataset.testResult = 'pass';
} catch (error) {
  output.textContent = `FAIL: ${error.stack}`;
  document.documentElement.dataset.testResult = 'fail';
}
