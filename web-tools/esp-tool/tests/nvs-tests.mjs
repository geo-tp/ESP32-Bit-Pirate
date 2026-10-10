import assert from "node:assert/strict";
import { nvsCrc32, parsePartitionTable, parseNvsPartition, formatNvsText } from "../src/NvsParser.js";

const write32 = (bytes, at, value) => new DataView(bytes.buffer).setUint32(at, value >>> 0, true);
const write16 = (bytes, at, value) => new DataView(bytes.buffer).setUint16(at, value, true);
function partitionTable() {
  const table = new Uint8Array(0x1000).fill(0xff);
  const add = (index, label, type, subtype, start, size, flags=0) => {
    const o = index * 32;
    table[o] = 0xaa; table[o + 1] = 0x50;
    table[o + 2] = type; table[o + 3] = subtype;
    write32(table, o + 4, start); write32(table, o + 8, size);
    table.set(new TextEncoder().encode(label), o + 12);
    table[o + 12 + label.length] = 0;
    write32(table, o + 28, flags);
  };
  add(0, "nvs", 1, 2, 0x9000, 0x5000);
  add(1, "phy_init", 1, 1, 0xe000, 0x1000);
  add(2, "otadata", 1, 0, 0xf000, 0x2000);
  add(3, "nvs_extra", 1, 2, 0x18000, 0x4000, 1);
  return table;
}

function page(data, pageIdx, sequence) {
  const base = pageIdx * 4096;
  write32(data, base, 0xfffffffe);
  write32(data, base + 4, sequence);
  data[base + 8] = 0xfe;
  write32(data, base + 28, nvsCrc32(data.subarray(base + 4, base + 28)));
}
function addEntry(data, pageNum, slot, { ns, type, key, value = 0, chunk = 255, bytes, isErased = false }) {
  const base = pageNum * 4096;
  const offset = base + 64 + slot * 32;
  const view = new DataView(data.buffer);
  const count = bytes === undefined ? 1 : 1 + Math.ceil(bytes.length / 32);
  const entry = data.subarray(offset, offset + 32);
  entry.fill(0xff);
  entry[0] = ns; entry[1] = type; entry[2] = count; entry[3] = chunk;
  entry.set(new TextEncoder().encode(key), 8);
  entry[8 + key.length] = 0;
  if (bytes) {
    write16(data, offset + 24, bytes.length);
    view.setUint32(offset + 28, nvsCrc32(bytes), true);
    for (let i = 0; i < count - 1; i++) {
      const block = data.subarray(offset + 32 + i*32, offset + 64 + i*32);
      block.fill(0xff);
      block.set(bytes.subarray(i*32, (i+1)*32));
    }
  } else {
    if (type === 0x01) view.setUint8(offset + 24, value);
    else if (type === 0x02) view.setUint16(offset + 24, value, true);
    else if (type === 0x04) view.setUint32(offset + 24, value, true);
    else if (type === 0x08) view.setBigUint64(offset + 24, BigInt(value), true);
    else if (type === 0x48) {
      view.setUint32(offset + 24, value.length, true);
      view.setUint8(offset + 28, value.count);
      view.setUint8(offset + 29, value.start);
    }
  }
  const checksum = new Uint8Array(28);
  checksum.set(entry.subarray(0, 4)); checksum.set(entry.subarray(8, 32), 4);
  write32(data, offset + 4, nvsCrc32(checksum));
  for (let s = slot; s < slot + count; s++) {
    const b = base + 32 + Math.floor(s / 4);
    const shift = (s % 4) * 2;
    data[b] = (data[b] & ~(3 << shift)) | ((isErased ? 0 : 2) << shift);
  }
  return count;
}

assert.equal(nvsCrc32(new Uint8Array([0,1,1,255,116,101,115,116,0,...new Array(11).fill(0),1,...new Array(7).fill(255)])), 0x7e817bfa, "Cross-check against Espressif NVS example CRC");
const detected = parsePartitionTable(partitionTable(), 4 * 1024 * 1024);
assert.deepEqual(detected.map(x => x.label), ["nvs", "nvs_extra"]);
assert.equal(detected[1].encrypted, true);
assert.throws(() => parsePartitionTable(new Uint8Array(4096).fill(255)), /No valid/);

const data = new Uint8Array(4096 * 4).fill(255);
page(data, 0, 5);
addEntry(data, 0, 0, { ns:0, type:1, key:"settings", value:1 });
addEntry(data, 0, 1, { ns:1, type:4, key:"counter", value:123456 });
addEntry(data, 0, 2, { ns:1, type:0x21, key:"ssid", bytes:new TextEncoder().encode("MyNetwork\0") });
addEntry(data, 0, 4, { ns:1, type:8, key:"large", value:"18446744073709551615" });
addEntry(data, 0, 5, { ns:1, type:1, key:"deleted", value:77, isErased:true });
page(data, 2, 10);
addEntry(data, 2, 0, { ns:1, type:4, key:"counter", value:999 });
addEntry(data, 2, 1, { ns:1, type:0x42, key:"cert", chunk:0, bytes: new Uint8Array(32).fill(0xaa) });
// A chunk index is stored after the BLOB_DATA record, potentially on a different page.
page(data, 1, 8);
addEntry(data, 1, 0, { ns:1, type:0x42, key:"cert", chunk:1, bytes: new Uint8Array([1, 2, 3]) });
addEntry(data, 2, 3, { ns:1, type:0x48, key:"cert", value:{ length:35, count:2, start:0 } });
const result = parseNvsPartition(data);
const byKey = new Map(result.entries.map((entry) => [entry.key, entry]));
assert.equal(result.namespaces, 1);
assert.equal(result.entries.length, 4);
assert.equal(byKey.get("ssid").value, "MyNetwork");
assert.equal(byKey.get("counter").value, 999);
assert.equal(byKey.get("large").value, "18446744073709551615");
assert.match(byKey.get("cert").value, /^35 bytes \|/);
assert.equal(byKey.has("deleted"), false);
assert.match(formatNvsText(result, detected[0]), /\[settings\][\s\S]*ssid \(string\) = "MyNetwork"/);
const encrypted = new Uint8Array(4096).fill(0x12);
assert.throws(() => parseNvsPartition(encrypted), /could not be decoded/);
console.log("NVS parser tests passed (table, CRC, namespaces, integers, strings, deleted entries, updates, multi-page blobs, invalid pages).");
