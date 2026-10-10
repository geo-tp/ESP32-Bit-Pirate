// Read-only ESP-IDF partition/NVS decoder. Everything stays in the browser.
const PAGE_SIZE = 4096;
const ENTRY_SIZE = 32;
const ENTRY_COUNT = 126;
const utf8 = new TextDecoder("utf-8", { fatal: false });
const TYPE_NAMES = {
  0x01: "u8", 0x11: "i8", 0x02: "u16", 0x12: "i16", 0x04: "u32", 0x14: "i32",
  0x08: "u64", 0x18: "i64", 0x24: "float", 0x28: "double",
  0x21: "string", 0x41: "blob", 0x42: "blob-data", 0x48: "blob-index",
};
const PAGE_STATES = new Set([0xfffffffe, 0xfffffffc, 0xfffffff8]);
const CRC_TABLE = new Uint32Array(256);
for (let i = 0; i < 256; i++) {
  let crc = i;
  for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  CRC_TABLE[i] = crc >>> 0;
}

// ESP-IDF uses esp_rom_crc32_le(0xffffffff, data), corresponding to
// zlib.crc32(data, 0xffffffff), NOT the usual CRC32 with an initial seed of 0.
export function nvsCrc32(bytes) {
  let crc = 0;
  for (const byte of bytes) crc = (crc >>> 8) ^ CRC_TABLE[(crc ^ byte) & 0xff];
  return (crc ^ 0xffffffff) >>> 0;
}

function u32(bytes, offset) {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(offset, true);
}
function nameAt(bytes, offset, size) {
  const data = bytes.subarray(offset, offset + size);
  const end = data.indexOf(0);
  return utf8.decode(end >= 0 ? data.subarray(0, end) : data);
}
function hex(value) { return `0x${value.toString(16).toUpperCase().padStart(6, "0")}`; }
function allFF(bytes) { return bytes.every((byte) => byte === 0xff); }
function asciiName(text) { return /^[\x20-\x7e]{1,15}$/.test(text); }

export function parsePartitionTable(bytes, flashCapacity = Number.POSITIVE_INFINITY, kind = "nvs") {
  if (!(bytes instanceof Uint8Array) || bytes.length < 32) throw new Error("Invalid partition table data.");
  const partitions = [];
  let foundMagic = false;
  for (let offset = 0; offset + 32 <= bytes.length; offset += 32) {
    const magic = bytes[offset] | (bytes[offset + 1] << 8);
    if (magic === 0xffff) break;
    if (magic === 0xebeb) break; // Partition table checksum record.
    if (magic !== 0x50aa) {
      if (!foundMagic) throw new Error("No ESP-IDF partition table at this address. Try another table offset.");
      throw new Error(`Invalid partition table entry at ${hex(offset)}.`);
    }
    foundMagic = true;
    const type = bytes[offset + 2];
    const subtype = bytes[offset + 3];
    const start = u32(bytes, offset + 4);
    const size = u32(bytes, offset + 8);
    const label = nameAt(bytes, offset + 12, 16);
    const flags = u32(bytes, offset + 28);
    if (!size || start % 0x1000 !== 0 || size % 0x1000 !== 0 || start + size > flashCapacity) {
      throw new Error(`Invalid partition range: ${label || hex(start)}.`);
    }
    partitions.push({ label, type, subtype, start, size, flags, encrypted: (flags & 1) !== 0 });
  }
  if (!foundMagic) throw new Error("No valid ESP-IDF partition table found.");
  return kind === "all" ? partitions : partitions.filter((partition) => partition.type === 0x01 && partition.subtype === 0x02);
}

function decodeNumber(type, view) {
  switch (type) {
    case 0x01: return view.getUint8(24);
    case 0x11: return view.getInt8(24);
    case 0x02: return view.getUint16(24, true);
    case 0x12: return view.getInt16(24, true);
    case 0x04: return view.getUint32(24, true);
    case 0x14: return view.getInt32(24, true);
    case 0x08: return view.getBigUint64(24, true).toString();
    case 0x18: return view.getBigInt64(24, true).toString();
    case 0x24: return view.getFloat32(24, true);
    case 0x28: return view.getFloat64(24, true);
    default: return null;
  }
}

function parsePageEntries(data, base, sequence, warnings) {
  const entries = [];
  for (let slot = 0; slot < ENTRY_COUNT;) {
    const bitmapByte = data[base + 32 + (slot >> 2)];
    const state = (bitmapByte >> ((slot & 3) * 2)) & 3;
    if (state !== 2) { slot++; continue; } // Current live entries only.
    const pos = base + 64 + slot * ENTRY_SIZE;
    const entry = data.subarray(pos, pos + ENTRY_SIZE);
    const ns = entry[0], type = entry[1], span = entry[2], chunk = entry[3];
    const key = nameAt(entry, 8, 16);
    const view = new DataView(entry.buffer, entry.byteOffset, entry.byteLength);
    const expectedCrc = view.getUint32(4, true);
    const input = new Uint8Array(28);
    input.set(entry.subarray(0, 4), 0);
    input.set(entry.subarray(8, 32), 4);
    if (nvsCrc32(input) !== expectedCrc || !TYPE_NAMES[type] || !asciiName(key) || !span || span > ENTRY_COUNT - slot) {
      warnings.push(`Skipped invalid NVS entry at ${hex(base + 64 + slot * ENTRY_SIZE)} (CRC, type, key or span).`);
      slot++;
      continue;
    }
    const variable = type === 0x21 || type === 0x41 || type === 0x42;
    const entryValue = { ns, type, typeName: TYPE_NAMES[type], span, chunk, key, sequence, slot };
    if (variable) {
      const length = view.getUint16(24, true);
      if (span !== 1 + Math.ceil(length / ENTRY_SIZE) || length > (span - 1) * ENTRY_SIZE) {
        warnings.push(`Skipped ${key}: invalid length/span.`);
        slot++;
        continue;
      }
      const valueBytes = data.subarray(pos + ENTRY_SIZE, pos + ENTRY_SIZE + length);
      if (nvsCrc32(valueBytes) !== view.getUint32(28, true)) {
        warnings.push(`Skipped ${key}: data checksum mismatch.`);
        slot += span;
        continue;
      }
      // Reject a partially erased payload. Bits are stored per 32-byte entry.
      let livePayload = true;
      for (let i = 1; i < span; i++) {
        const bit = (data[base + 32 + ((slot + i) >> 2)] >> (((slot + i) & 3) * 2)) & 3;
        if (bit !== 2) { livePayload = false; break; }
      }
      if (!livePayload) {
        warnings.push(`Skipped ${key}: incomplete data entries.`);
        slot += span;
        continue;
      }
      entryValue.bytes = valueBytes;
      entryValue.value = type === 0x21 ? utf8.decode(valueBytes.subarray(0, valueBytes.indexOf(0) < 0 ? length : valueBytes.indexOf(0))) : null;
    } else if (type === 0x48) {
      if (span !== 1 || chunk !== 0xff) { slot++; continue; }
      entryValue.length = view.getUint32(24, true);
      entryValue.count = view.getUint8(28);
      entryValue.chunkStart = view.getUint8(29);
    } else {
      if (span !== 1) { slot++; continue; }
      entryValue.value = decodeNumber(type, view);
    }
    entries.push(entryValue);
    slot += span;
  }
  return entries;
}

function summarizeBlob(bytes) {
  const shown = bytes.subarray(0, 48);
  const sample = Array.from(shown, (byte) => byte.toString(16).padStart(2, "0")).join(" ").toUpperCase();
  return `${bytes.length} bytes | ${sample}${bytes.length > shown.length ? " …" : ""}`;
}

export function parseNvsPartition(data) {
  if (!(data instanceof Uint8Array) || !data.length || data.length % PAGE_SIZE) {
    throw new Error("NVS region must be a complete number of 4 KB pages.");
  }
  const warnings = [];
  const pages = [];
  let invalidPages = 0;
  let usedPages = 0;
  for (let base = 0; base < data.length; base += PAGE_SIZE) {
    const block = data.subarray(base, base + PAGE_SIZE);
    if (allFF(block)) continue;
    const state = u32(block, 0);
    if (!PAGE_STATES.has(state)) {
      invalidPages++;
      warnings.push(`Invalid page header at ${hex(base)} (encrypted or corrupted?).`);
      continue;
    }
    const crc = nvsCrc32(block.subarray(4, 28));
    if (crc !== u32(block, 28)) {
      invalidPages++;
      warnings.push(`Page header CRC mismatch at ${hex(base)} (encrypted or corrupted?).`);
      continue;
    }
    usedPages++;
    const sequence = u32(block, 4);
    pages.push({ sequence, base, entries: parsePageEntries(data, base, sequence, warnings) });
  }
  if (!usedPages && invalidPages) throw new Error("NVS data could not be decoded (encrypted, read-protected or damaged). Raw backup is still available.");
  pages.sort((a, b) => a.sequence - b.sequence);
  const rawEntries = pages.flatMap((page) => page.entries);
  // Namespace table: ns=0/u8; the stored numeric value is the namespace index.
  const namespaces = new Map();
  for (const entry of rawEntries) {
    if (entry.ns === 0 && entry.type === 0x01 && entry.value > 0) namespaces.set(entry.value, entry.key);
  }
  const latest = new Map();
  for (const entry of rawEntries) {
    if (entry.ns === 0 || entry.type === 0x42) continue;
    latest.set(`${entry.ns}\0${entry.key}`, entry);
  }
  const parsed = [];
  for (const entry of latest.values()) {
    const namespace = namespaces.get(entry.ns) ?? `namespace_${entry.ns}`;
    if (entry.type === 0x48) {
      const chunks = [];
      for (let i = 0; i < entry.count; i++) {
        const chunk = [...rawEntries].reverse().find((item) => item.ns === entry.ns && item.key === entry.key && item.type === 0x42 && item.chunk === ((entry.chunkStart + i) & 0xff));
        if (!chunk) break;
        chunks.push(chunk.bytes);
      }
      const sum = chunks.reduce((count, chunk) => count + chunk.length, 0);
      if (chunks.length !== entry.count || sum !== entry.length) {
        warnings.push(`Incomplete blob ${namespace}/${entry.key}: ${chunks.length}/${entry.count} chunks.`);
        parsed.push({ namespace, key: entry.key, type: "blob (incomplete)", value: `${entry.length} bytes (incomplete)` });
        continue;
      }
      const bytes = new Uint8Array(sum);
      let pos = 0;
      for (const chunk of chunks) { bytes.set(chunk, pos); pos += chunk.length; }
      parsed.push({ namespace, key: entry.key, type: "blob", value: summarizeBlob(bytes) });
    } else if (entry.type === 0x41) {
      parsed.push({ namespace, key: entry.key, type: "blob", value: summarizeBlob(entry.bytes) });
    } else {
      parsed.push({ namespace, key: entry.key, type: entry.typeName, value: entry.value });
    }
  }
  parsed.sort((a, b) => a.namespace.localeCompare(b.namespace) || a.key.localeCompare(b.key));
  if (usedPages && !parsed.length && rawEntries.length === 0 && warnings.length) {
    warnings.push("No valid NVS records. Encrypted entries cannot be decoded without NVS encryption keys.");
  }
  return { entries: parsed, namespaces: namespaces.size, pages: usedPages, invalidPages, warnings };
}

export function formatNvsText(result, partition) {
  const lines = [
    "ESP-IDF NVS extract (read-only)",
    `Partition: ${partition.label} | offset ${hex(partition.start)} | size ${hex(partition.size)} (${partition.size} bytes)`,
    `Namespaces: ${result.namespaces} | Keys: ${result.entries.length} | Pages: ${result.pages}`,
    "",
  ];
  let currentNs = null;
  for (const entry of result.entries) {
    if (entry.namespace !== currentNs) {
      if (currentNs !== null) lines.push("");
      currentNs = entry.namespace;
      lines.push(`[${currentNs}]`);
    }
    lines.push(`${entry.key} (${entry.type}) = ${typeof entry.value === "string" && entry.type === "string" ? JSON.stringify(entry.value) : String(entry.value)}`);
  }
  if (!result.entries.length) lines.push("(no decoded keys)");
  if (result.warnings.length) {
    lines.push("", "Warnings:");
    for (const warning of result.warnings) lines.push(`- ${warning}`);
  }
  return lines.join("\n") + "\n";
}
