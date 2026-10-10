// Small, dependency-free ZIP writer (STORED, ZIP32). Keeps original bytes unchanged.
const te = new TextEncoder();
const CRC_TABLE = new Uint32Array(256);
for (let n = 0; n < 256; ++n) {
  let v = n;
  for (let j = 0; j < 8; ++j) v = (v >>> 1) ^ (v & 1 ? 0xedb88320 : 0);
  CRC_TABLE[n] = v >>> 0;
}
function crc32(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) crc = (crc >>> 8) ^ CRC_TABLE[(crc ^ b) & 255];
  return (crc ^ 0xffffffff) >>> 0;
}
function u16(arr, at, value) { arr[at] = value & 255; arr[at + 1] = (value >>> 8) & 255; }
function u32(arr, at, value) { u16(arr, at, value); u16(arr, at + 2, value >>> 16); }
export function createZip(items) {
  if (items.length > 65535) throw new Error("Too many files for ZIP32.");
  const local = [], central = [];
  let offset = 0, directorySize = 0;
  for (const { path, data, directory = false } of items) {
    if (!(data instanceof Uint8Array)) throw new Error("Invalid ZIP file content.");
    const name = directory && !path.endsWith("/") ? `${path}/` : path;
    if (!name || name.startsWith("/") || name.split("/").some((p) => p === "." || p === ".." || p.includes("\\"))) throw new Error("Unsafe ZIP filename.");
    const nameBytes = te.encode(name);
    if (nameBytes.length > 65535 || data.length > 0xffffffff) throw new Error("File too large for ZIP32.");
    const checksum = crc32(data);
    const a = new Uint8Array(30 + nameBytes.length);
    u32(a, 0, 0x04034b50); u16(a, 4, 20); u16(a, 6, 0x0800); // UTF-8
    u32(a, 14, checksum); u32(a, 18, data.length); u32(a, 22, data.length);
    u16(a, 26, nameBytes.length); a.set(nameBytes, 30);
    local.push(a, data);
    const b = new Uint8Array(46 + nameBytes.length);
    u32(b, 0, 0x02014b50); u16(b, 4, 20); u16(b, 6, 20); u16(b, 8, 0x0800);
    u32(b, 16, checksum); u32(b, 20, data.length); u32(b, 24, data.length);
    u16(b, 28, nameBytes.length); u32(b, 38, directory ? 0x10 : 0);
    u32(b, 42, offset); b.set(nameBytes, 46);
    central.push(b);
    offset += a.length + data.length;
    directorySize += b.length;
    if (offset + directorySize + 22 > 0xffffffff) throw new Error("Archive exceeds ZIP32 limit.");
  }
  const footer = new Uint8Array(22);
  u32(footer, 0, 0x06054b50); u16(footer, 8, items.length); u16(footer, 10, items.length);
  u32(footer, 12, directorySize); u32(footer, 16, offset);
  return new Blob([...local, ...central, footer], { type: "application/zip" });
}
