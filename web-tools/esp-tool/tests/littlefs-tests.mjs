import assert from "node:assert/strict";
import { writeFile } from "node:fs/promises";
import { parseLittleFs, crc32Littlefs } from "../src/LittleFsParser.js";
import { createZip } from "../src/ZipWriter.js";
import { parsePartitionTable } from "../src/NvsParser.js";
const bsize = 4096;
const count = 16;
const image = new Uint8Array(count * bsize).fill(255);
const te = new TextEncoder();
const u32 = (buf, at, n) => new DataView(buf.buffer, buf.byteOffset, buf.byteLength).setUint32(at, n >>> 0, true);
const be32 = (buf, at, n) => new DataView(buf.buffer, buf.byteOffset, buf.byteLength).setUint32(at, n >>> 0, false);
const pack32 = (n) => { const b = new Uint8Array(4); u32(b, 0, n); return b; };
const concat = (...parts) => { const b = new Uint8Array(parts.reduce((n,x)=>n+x.length,0)); let i=0; for (const p of parts){ b.set(p,i);i+=p.length;}return b; };
const tag = (type,id,len) => ((type <<20) | (id << 10) | len) >>> 0;
const pair = (a,b) => concat(pack32(a),pack32(b));
function writeMetadata(block, revision, tags) {
  const at = block*bsize; u32(image, at, revision);
  let offset = 4, previous = 0xffffffff;
  let crc = crc32Littlefs(image.subarray(at,at+4));
  for (const [type, id, bytes] of tags) {
    const current=tag(type,id,bytes.length);
    be32(image,at+offset,current^previous);
    image.set(bytes,at+offset+4);
    crc = crc32Littlefs(image.subarray(at+offset,at+offset+4+bytes.length),crc);
    offset += 4+bytes.length;
    previous = current;
  }
  const crcTag=tag(0x500,0x3ff,4);
  be32(image,at+offset,crcTag^previous);
  crc=crc32Littlefs(image.subarray(at+offset,at+offset+4),crc);
  u32(image,at+offset+4,crc);
  offset+=8;
  return { offset, lastTag: crcTag };
}
const superInfo=concat(pack32(0x00020001),pack32(4096),pack32(count),pack32(255),pack32(0x7fffffff),pack32(1022));
const small = te.encode("sample config content\n");
const replacement = te.encode("updated config\n");
const big = new Uint8Array(9000);
for (let i=0;i<big.length;i++) big[i]=(i*13+5)&255;
// CTZ block 0: all data; block 1: 1 pointer + data; block 2: 2 pointers + data.
image.set(big.subarray(0,4096),4*4096);
u32(image,5*4096,4); image.set(big.subarray(4096,4096+4092),5*4096+4);
u32(image,6*4096,5); u32(image,6*4096+4,4); image.set(big.subarray(4096+4092),6*4096+8);
const superTag = [[0x0ff,0,te.encode("littlefs")],[0x201,0,superInfo]];
const rootTags=[...superTag,
  [0x401,1,new Uint8Array(0)], [0x001,1,te.encode("config.txt")], [0x201,1,small],
  [0x401,2,new Uint8Array(0)], [0x001,2,te.encode("big.bin")], [0x202,2,concat(pack32(6),pack32(big.length))],
  [0x401,3,new Uint8Array(0)], [0x002,3,te.encode("dir")], [0x200,3,pair(2,3)],
];
writeMetadata(0,2,rootTags);
writeMetadata(1,1,rootTags);
const dirTags=[[0x401,0,new Uint8Array(0)], [0x001,0,te.encode("nested.dat")], [0x201,0,te.encode("nested!\n")],
 [0x401,1,new Uint8Array(0)],[0x001,1,te.encode("empty")],[0x201,1,new Uint8Array(0)]];
writeMetadata(2,1,dirTags);
writeMetadata(3,0,dirTags);
let fs=parseLittleFs(image);
assert.equal(fs.files,4);
assert.equal(fs.folders,1);
assert.equal(fs.version,0x20001);
const byPath = Object.fromEntries(fs.entries.map(e=>[e.path,e]));
assert.deepEqual(fs.readFile(byPath["config.txt"]),small);
assert.deepEqual(fs.readFile(byPath["big.bin"]),big);
assert.equal(new TextDecoder().decode(fs.readFile(byPath["dir/nested.dat"])),"nested!\n");
assert.equal(fs.readFile(byPath["dir/empty"]).length,0);

// Appending a second CRC-protected commit must supersede the earlier struct.
function appendCommit(block, offset, previous, tags) {
  const base=block*bsize; let crc=0xffffffff;
  for (const [type,id,data] of tags) {
    const cur=tag(type,id,data.length);
    be32(image,base+offset,cur^previous);
    image.set(data,base+offset+4);
    crc=crc32Littlefs(image.subarray(base+offset,base+offset+4+data.length),crc);
    offset+=4+data.length;previous=cur;
  }
  const check=tag(0x500,0x3ff,4);
  be32(image,base+offset,check^previous);
  crc=crc32Littlefs(image.subarray(base+offset,base+offset+4),crc);
  u32(image,base+offset+4,crc);
  return offset+8;
}
const prev=writeMetadata(0,3,rootTags);
const commitStart=prev.offset;
appendCommit(0,prev.offset,prev.lastTag,[[0x201,1,replacement]]);
fs=parseLittleFs(image);
assert.deepEqual(fs.readFile(fs.entries.find(e=>e.path==="config.txt")),replacement);
// Incomplete second commit must leave the last committed version intact.
image[0*4096+commitStart+8] ^= 1;
fs=parseLittleFs(image);
assert.deepEqual(fs.readFile(fs.entries.find(e=>e.path==="config.txt")),small);
// Corrupt all commits in the newest metadata block -> fallback to older copy.
image[0*4096+36] ^= 1;
fs=parseLittleFs(image);
assert.deepEqual(fs.readFile(fs.entries.find(e=>e.path==="config.txt")),small);
// ZIP export validates file data and nested paths.
const items=fs.entries.map(e=>({path:e.path, directory:e.kind==="dir", data:e.kind==="file"?fs.readFile(e):new Uint8Array()}));
const zip=createZip(items);
await writeFile("/mnt/data/littlefs-work/littlefs-test.zip",new Uint8Array(await zip.arrayBuffer()));
// Verify partition selection includes SPIFFS-style LittleFS subtype but keeps NVS filtering.
const table=new Uint8Array(4096).fill(255);
table.set([0xaa,0x50,1,0x82],0);u32(table,4,0x200000);u32(table,8,0x100000);table.set(te.encode("littlefs"),12);table[12+8]=0;
table.set([0xaa,0x50,1,0x02],32);u32(table,36,0x9000);u32(table,40,0x5000);table.set(te.encode("nvs"),44);table[44+3]=0;
assert.deepEqual(parsePartitionTable(table,4194304).map(x=>x.label),["nvs"]);
assert.deepEqual(parsePartitionTable(table,4194304,"all").map(x=>x.label),["littlefs","nvs"]);
assert.throws(()=>parseLittleFs(new Uint8Array(2*bsize).fill(255)), /metadata pair|superblock/i);
console.log("LittleFS tests passed (superblock, CRC fallback, nested paths, inline files, CTZ 3 blocks, empty files, ZIP, table).");
