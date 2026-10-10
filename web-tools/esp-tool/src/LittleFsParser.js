// Read-only littlefs v2 metadata/CTZ decoder. Flash images are never modified.
// Format: https://github.com/littlefs-project/littlefs/blob/master/SPEC.md
const decoder = new TextDecoder("utf-8", { fatal: true });
const encoder = new TextEncoder();
const NULL_BLOCK = 0xffffffff;
function u32(data, at) { return new DataView(data.buffer, data.byteOffset, data.byteLength).getUint32(at, true); }
function be32(data, at) { return new DataView(data.buffer, data.byteOffset, data.byteLength).getUint32(at, false); }
function crc32Littlefs(bytes, seed = 0xffffffff) {
  let crc = seed >>> 0;
  for (const b of bytes) {
    crc ^= b;
    for (let i = 0; i < 8; ++i) crc = ((crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0)) >>> 0;
  }
  return crc >>> 0;
}
export { crc32Littlefs };

function tagType(tag) { return (tag >>> 20) & 0x7ff; }
function tagId(tag) { return (tag >>> 10) & 0x3ff; }
function tagLength(tag) { return tag & 0x3ff; }
function validBlock(block, count) { return Number.isInteger(block) && block >= 0 && block < count; }
function pairKey(pair) { return pair.slice().sort((a,b) => a-b).join(":"); }
function safeName(bytes) {
  const name = decoder.decode(bytes);
  if (!name || name === "." || name === ".." || name.includes("/") || name.includes("\\") || name.includes("\0") || /[\x00-\x1f\x7f]/.test(name)) {
    throw new Error("Unsafe or invalid littlefs entry name.");
  }
  return name;
}
function cloneEntries(entries) { return entries.map((entry) => ({ ...entry })); }
function applyTags(state, tags) {
  for (const { type, id, data } of tags) {
    if (type === 0x401) { // Create entry, shift ids >= new position
      if (id > state.entries.length) throw new Error("Invalid littlefs create index.");
      state.entries.splice(id, 0, {});
    } else if (type === 0x4ff) { // Delete entry
      if (id < state.entries.length) state.entries.splice(id, 1);
    } else if (type >= 0x001 && type <= 0x0ff && id !== 0x3ff) {
      while (state.entries.length <= id) state.entries.push({});
      state.entries[id].kind = type === 0x001 ? "file" : type === 0x002 ? "dir" : type === 0x0ff ? "super" : "unknown";
      state.entries[id].name = decoder.decode(data);
    } else if (type >= 0x200 && type <= 0x2ff && id !== 0x3ff) {
      while (state.entries.length <= id) state.entries.push({});
      if (type === 0x201) state.entries[id].struct = { type: "inline", bytes: data };
      else if (type === 0x202 && data.length === 8) state.entries[id].struct = { type: "ctz", head: u32(data, 0), size: u32(data, 4) };
      else if (type === 0x200 && data.length === 8) state.entries[id].struct = { type: "dir", pair: [u32(data, 0), u32(data, 4)] };
    } else if ((type & 0x700) === 0x600 && data.length === 8) {
      state.tail = [u32(data, 0), u32(data, 4)];
      state.split = (type & 1) === 1;
    }
  }
}

export function parseLittleFs(image) {
  if (!(image instanceof Uint8Array) || image.length < 8192) throw new Error("LittleFS partition image is too small.");
  // ESP littlefs uses 4096-byte erase blocks; the superblock's block size validates this.
  const blockSize = 4096;
  if (image.length % blockSize !== 0) throw new Error("LittleFS partition must be 4 KB aligned.");
  const count = image.length / blockSize;
  const warnings = [];
  const pairCache = new Map();

  function scanBlock(block) {
    if (!validBlock(block, count)) return null;
    const base = block * blockSize;
    const rev = u32(image, base);
    let offset = 4, ptag = 0xffffffff, crc = crc32Littlefs(image.subarray(base, base + 4));
    let state = { entries: [], tail: [NULL_BLOCK, NULL_BLOCK], split: false };
    let pending = [], validCommits = 0;
    while (offset + 4 <= blockSize) {
      const encoded = be32(image, base + offset);
      const tag = (encoded ^ ptag) >>> 0;
      if ((tag & 0x80000000) || tag === 0 || tag === 0xffffffff) break;
      const size = tagLength(tag) === 0x3ff ? 0 : tagLength(tag);
      const type = tagType(tag), id = tagId(tag);
      if (offset + 4 + size > blockSize || pending.length > 1024) break;
      const rawTag = image.subarray(base + offset, base + offset + 4);
      crc = crc32Littlefs(rawTag, crc);
      const data = image.subarray(base + offset + 4, base + offset + 4 + size);
      if ((type & 0x7fe) === 0x500) { // CRC tag (not optional FCRC 0x5ff)
        if (size < 4 || u32(data, 0) !== crc) break;
        const staged = { entries: cloneEntries(state.entries), tail: state.tail.slice(), split: state.split };
        try { applyTags(staged, pending); } catch { break; }
        state = staged;
        pending = [];
        ++validCommits;
        ptag = (tag ^ ((type & 1) << 31)) >>> 0;
        crc = 0xffffffff;
      } else {
        crc = crc32Littlefs(data, crc);
        pending.push({ type, id, data });
        ptag = tag;
      }
      offset += 4 + size;
    }
    return validCommits ? { ...state, block, rev, validCommits } : null;
  }

  function readPair(pair) {
    if (pair.length !== 2 || !pair.every((n) => validBlock(n, count)) || pair[0] === pair[1]) {
      throw new Error("LittleFS metadata pair is outside the partition.");
    }
    const key = pairKey(pair);
    if (pairCache.has(key)) return pairCache.get(key);
    const a = scanBlock(pair[0]), b = scanBlock(pair[1]);
    if (!a && !b) throw new Error(`Invalid LittleFS metadata pair (${key}); CRC or format mismatch.`);
    const newer = (a && b) ? (((a.rev - b.rev) | 0) > 0 ? a : b) : a ?? b;
    pairCache.set(key, newer);
    return newer;
  }

  let root = [0, 1];
  let superblock = null;
  const visitedSuper = new Set();
  for (let depth = 0; depth < count; depth++) {
    const key = pairKey(root);
    if (visitedSuper.has(key)) throw new Error("LittleFS superblock chain loop.");
    visitedSuper.add(key);
    const dir = readPair(root);
    const entry = dir.entries.find((e) => e.kind === "super" && e.name === "littlefs" && e.struct?.type === "inline");
    if (!entry) throw new Error("Not a LittleFS v2 filesystem (missing superblock).");
    if (entry.struct.bytes.length < 24) throw new Error("Invalid LittleFS superblock.");
    const sb = entry.struct.bytes;
    const version = u32(sb, 0), diskBlockSize = u32(sb, 4), diskBlockCount = u32(sb, 8);
    if ((version >>> 16) !== 2 || diskBlockSize !== blockSize || diskBlockCount < 2 || diskBlockCount > count) {
      throw new Error("Unsupported LittleFS version, block size, or partition geometry.");
    }
    superblock = { version, blockSize, blockCount: diskBlockCount };
    // The root directory may move as the filesystem ages. Follow superblock copies only.
    if (dir.tail.every((b) => validBlock(b, count)) && !dir.split) {
      const next = readPair(dir.tail);
      const nextSuper = next.entries.some((e) => e.kind === "super" && e.name === "littlefs");
      if (nextSuper) { root = dir.tail; continue; }
    }
    break;
  }
  if (!superblock) throw new Error("No valid LittleFS superblock.");

  const entries = [];
  const visitedDirs = new Set();
  function walk(pair, path, depth) {
    if (depth > 32) throw new Error("LittleFS directory depth exceeded.");
    let nextPair = pair;
    for (let steps = 0; steps < count; steps++) {
      const key = pairKey(nextPair);
      if (visitedDirs.has(key)) throw new Error("LittleFS directory loop detected.");
      visitedDirs.add(key);
      const dir = readPair(nextPair);
      for (const e of dir.entries) {
        if (e.kind !== "file" && e.kind !== "dir") continue;
        if (!e.struct) throw new Error(`Missing file structure: ${e.name || "unnamed"}.`);
        const name = safeName(encoder.encode(e.name));
        const filename = path ? `${path}/${name}` : name;
        if (e.kind === "dir") {
          if (e.struct.type !== "dir") throw new Error(`Invalid directory structure: ${filename}.`);
          entries.push({ path: filename, name, kind: "dir", size: 0 });
          walk(e.struct.pair, filename, depth + 1);
        } else {
          if (e.struct.type !== "inline" && e.struct.type !== "ctz") throw new Error(`Invalid file structure: ${filename}.`);
          const size = e.struct.type === "inline" ? e.struct.bytes.length : e.struct.size;
          if (size > image.length) throw new Error(`Invalid file size for ${filename}.`);
          entries.push({ path: filename, name, kind: "file", size, struct: e.struct });
        }
      }
      if (!dir.split) return;
      if (!dir.tail.every((b) => validBlock(b, count))) throw new Error("Broken LittleFS directory tail.");
      nextPair = dir.tail;
    }
    throw new Error("LittleFS directory chain exceeded partition blocks.");
  }
  walk(root, "", 0);
  const unique = new Set();
  for (const e of entries) {
    if (unique.has(e.path)) throw new Error(`Duplicate path in LittleFS: ${e.path}.`);
    unique.add(e.path);
  }

  // CTZ index/seek ported from littlefs lfs_ctz_index and lfs_ctz_find.
  function ctzIndex(position) {
    const b = blockSize - 8;
    let i = Math.floor(position / b);
    if (i === 0) return { index: 0, offset: position };
    const popcount = (n) => { let p = 0; while (n > 0) { p += n % 2; n = Math.floor(n / 2); } return p; };
    i = Math.floor((position - 4 * (popcount(i - 1) + 2)) / b);
    return { index: i, offset: position - b * i - 4 * popcount(i) };
  }
  function trailingZeroes(n) { let k = 0; while (n % 2 === 0 && n > 0) { n /= 2; k++; } return k; }
  function ctzBlock(head, currentIndex, targetIndex) {
    let current = currentIndex;
    let block = head;
    const seen = new Set();
    while (current > targetIndex) {
      if (!validBlock(block, count) || seen.has(block)) throw new Error("Broken LittleFS CTZ chain.");
      seen.add(block);
      const delta = current - targetIndex;
      const skip = Math.min(Math.ceil(Math.log2(delta + 1)) - 1, trailingZeroes(current));
      block = u32(image, block * blockSize + 4 * skip);
      current -= 2 ** skip;
    }
    if (!validBlock(block, count)) throw new Error("LittleFS data block out of range.");
    return block;
  }
  function readFile(item) {
    if (item.kind !== "file") throw new Error("Only files can be downloaded.");
    if (item.struct.type === "inline") return item.struct.bytes.slice();
    const { size, head } = item.struct;
    if (size === 0) return new Uint8Array(0);
    const last = ctzIndex(size - 1).index;
    if (!validBlock(head, count)) throw new Error(`Invalid CTZ head: ${item.path}.`);
    const out = new Uint8Array(size);
    for (let pos = 0; pos < size;) {
      const { index, offset } = ctzIndex(pos);
      const block = ctzBlock(head, last, index);
      // ctzIndex already returns the physical offset after CTZ pointers.
      const blockOffset = offset;
      if (blockOffset >= blockSize) throw new Error(`Invalid CTZ offset: ${item.path}.`);
      const length = Math.min(size - pos, blockSize - blockOffset);
      out.set(image.subarray(block * blockSize + blockOffset, block * blockSize + blockOffset + length), pos);
      pos += length;
    }
    return out;
  }

  return {
    ...superblock, entries, warnings,
    files: entries.filter((e) => e.kind === "file").length,
    folders: entries.filter((e) => e.kind === "dir").length,
    readFile,
  };
}
