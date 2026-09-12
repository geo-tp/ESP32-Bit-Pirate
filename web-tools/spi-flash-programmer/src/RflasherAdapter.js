import { SerialTransport } from "../../shared/serial/SerialTransport.js";
import { SerialTimeoutError } from "../../shared/serial/SerialErrors.js";
import { identifyJedec } from "./chipDatabase.js?v=20260912";

const ACK = 0x06;
const NAK = 0x15;
const BUS_SPI = 0x08;
const DEFAULT_READ_CHUNK = 65536;
const DEFAULT_PAGE_SIZE = 256;
const ADDRESS_24BIT_LIMIT = 0x1000000;
const ADDRESS_32BIT_LIMIT = 0x100000000;

const COMMANDS = {
  NOP: 0x00,
  Q_IFACE: 0x01,
  Q_CMDMAP: 0x02,
  Q_PGMNAME: 0x03,
  Q_SERBUF: 0x04,
  Q_BUSTYPE: 0x05,
  Q_WRNMAXLEN: 0x08,
  SYNCNOP: 0x10,
  Q_RDNMAXLEN: 0x11,
  S_BUSTYPE: 0x12,
  O_SPIOP: 0x13,
  S_SPI_FREQ: 0x14,
  S_PIN_STATE: 0x15,
};

export class RflasherAdapter {
  constructor({ log = () => {} } = {}) {
    this.log = log;
    this.transport = null;
    this.info = null;
    this.lastProbe = null;
  }

  async connect({ spiFrequency = 8_000_000 } = {}) {
    this.transport = new SerialTransport({ baudRate: 115200 });
    await this.transport.requestAndOpen();
    this.log("Serial port opened at 115200 baud, 8N1, no flow control.");
    this.log("DTR asserted, RTS deasserted when supported by the browser driver.");
    await sleep(250);

    await this.synchronize();
    const iface = await this.queryIface();
    const commandMap = await this.queryCommandMap();
    const busType = await this.queryBusType();
    await this.setBusType(BUS_SPI);
    const programmerName = await this.queryProgrammerName();
    const serialBuffer = await this.queryLe16(COMMANDS.Q_SERBUF);
    const maxWrite = await this.queryLe24(COMMANDS.Q_WRNMAXLEN);
    const maxRead = await this.queryLe24(COMMANDS.Q_RDNMAXLEN);
    const actualSpiFrequency = await this.setSpiFrequency(spiFrequency);
    await this.setPinState(true);

    this.info = {
      iface,
      commandMap,
      busType,
      programmerName,
      serialBuffer,
      maxWrite,
      maxRead,
      spiFrequency: actualSpiFrequency,
    };

    this.log(`serprog adapter detected: ${programmerName}`);
    return this.info;
  }

  async disconnect() {
    if (!this.transport) {
      return;
    }

    try {
      await this.setPinState(false);
    } catch (error) {
      this.log(`Warning while disabling adapter pins: ${error.message}`);
    }

    await this.transport.close();
    this.transport = null;
    this.info = null;
    this.lastProbe = null;
  }

  getProgrammerInfo() {
    return this.info;
  }

  async probe() {
    this.ensureConnected();
    const jedecId = await this.spiOp(new Uint8Array([0x9f]), 3);
    const legacyId = await this.spiOp(new Uint8Array([0xab, 0x00, 0x00, 0x00]), 1);
    const sfdpHeader = await this.spiOp(new Uint8Array([0x5a, 0x00, 0x00, 0x00, 0x00]), 8);
    const chip = identifyJedec(jedecId);

    this.lastProbe = {
      jedecId,
      legacyId,
      sfdpHeader,
      chip,
    };

    if (chip) {
      this.log(`Probe matched ${chip.manufacturer} ${chip.model}.`);
    } else {
      this.log("Probe completed, but this JEDEC ID is not in the documented compatible chip database.");
    }

    return this.lastProbe;
  }

  async read(start, length) {
    this.ensureConnected();
    if (!Number.isInteger(start) || start < 0 || start >= ADDRESS_32BIT_LIMIT) {
      throw new Error("Read start must be a 32-bit SPI flash address.");
    }
    if (!Number.isInteger(length) || length <= 0 || length > this.safeReadChunkSize(DEFAULT_READ_CHUNK)) {
      throw new Error(`Read chunk length must be between 1 and ${this.safeReadChunkSize(DEFAULT_READ_CHUNK)} bytes.`);
    }

    const command = this.buildAddressCommand(start, { read: true });

    this.log(`Reading ${length} bytes at ${formatAddress(start)} with opcode 0x${command[0].toString(16).padStart(2, "0")}.`);
    return this.spiOp(command, length, 6000);
  }

  async readRange(start, length, { chunkSize = DEFAULT_READ_CHUNK, onProgress = () => {}, onChunk = () => {} } = {}) {
    this.ensureConnected();
    if (!Number.isInteger(start) || start < 0 || start >= ADDRESS_32BIT_LIMIT) {
      throw new Error("Read start must be a 32-bit SPI flash address.");
    }
    if (!Number.isInteger(length) || length <= 0) {
      throw new Error("Read length must be greater than zero.");
    }
    if (start + length > ADDRESS_32BIT_LIMIT) {
      throw new Error("Read range exceeds 32-bit SPI flash address space.");
    }

    const output = new Uint8Array(length);
    const safeChunk = this.safeReadChunkSize(chunkSize);
    for (let offset = 0; offset < length; offset += safeChunk) {
      const currentLength = Math.min(safeChunk, length - offset);
      const bytes = await this.read(start + offset, currentLength);
      output.set(bytes, offset);
      onChunk({ offset: start + offset, bytes });
      onProgress({ done: offset + currentLength, total: length, phase: "Reading" });
    }
    return output;
  }

  async dumpFull({ chunkSize = DEFAULT_READ_CHUNK, onProgress = () => {}, onChunk = () => {} } = {}) {
    const chip = this.requireIdentifiedChip();
    const output = new Uint8Array(chip.capacity);
    const safeChunk = this.safeReadChunkSize(chunkSize);

    this.log(`Starting full dump of ${chip.manufacturer} ${chip.model}: ${chip.capacity} bytes.`);
    for (let offset = 0; offset < chip.capacity; offset += safeChunk) {
      const length = Math.min(safeChunk, chip.capacity - offset);
      const bytes = await this.read(offset, length);
      output.set(bytes, offset);
      onChunk({ offset, bytes });
      onProgress({ done: offset + length, total: chip.capacity, phase: "Reading" });
    }
    this.log("Full dump complete.");
    return output;
  }

  async writeFull(bytes, { pageSize = DEFAULT_PAGE_SIZE, onProgress = () => {} } = {}) {
    const chip = this.requireIdentifiedChip();
    const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);

    if (data.length !== chip.capacity) {
      throw new Error(`Input file must be exactly ${chip.capacity} bytes for ${chip.model}; got ${data.length} bytes.`);
    }

    this.log(`Starting full-chip write for ${chip.manufacturer} ${chip.model}: ${data.length} bytes.`);
    await this.eraseChip({ onProgress: (progress) => onProgress({ ...progress, phase: "Erasing" }) });

    for (let offset = 0; offset < data.length; offset += pageSize) {
      const page = data.slice(offset, offset + pageSize);
      await this.pageProgram(offset, page);
      onProgress({ done: offset + page.length, total: data.length, phase: "Writing" });
    }

    this.log("Write complete. Starting verification readback.");
    await this.verifyFull(data, { onProgress });
    this.log("Full-chip write and verification complete.");
  }

  async eraseChip({ onProgress = () => {} } = {}) {
    const chip = this.requireIdentifiedChip();
    this.log("Sending write enable and chip erase (0xC7).");
    await this.writeEnable();
    await this.spiOp(new Uint8Array([0xc7]), 0, 2000);
    await this.waitWhileBusy({
      timeoutMs: Math.max(180000, Math.ceil(chip.capacity / ADDRESS_24BIT_LIMIT) * 180000),
      intervalMs: 250,
      onProgress,
    });
  }

  async verifyFull(expected, { chunkSize = DEFAULT_READ_CHUNK, onProgress = () => {} } = {}) {
    const chip = this.requireIdentifiedChip();
    const data = expected instanceof Uint8Array ? expected : new Uint8Array(expected);
    const safeChunk = this.safeReadChunkSize(chunkSize);

    if (data.length !== chip.capacity) {
      throw new Error(`Verify buffer must be exactly ${chip.capacity} bytes for ${chip.model}; got ${data.length} bytes.`);
    }

    for (let offset = 0; offset < data.length; offset += safeChunk) {
      const length = Math.min(safeChunk, data.length - offset);
      const actual = await this.read(offset, length);
      for (let index = 0; index < actual.length; index += 1) {
        if (actual[index] !== data[offset + index]) {
          const address = offset + index;
          throw new Error(
            `Verification failed at 0x${address.toString(16).padStart(6, "0")}: expected 0x${data[offset + index].toString(16).padStart(2, "0")}, got 0x${actual[index].toString(16).padStart(2, "0")}.`
          );
        }
      }
      onProgress({ done: offset + length, total: data.length, phase: "Verifying" });
    }
  }

  async cancel() {
    throw new Error("Cancel is not implemented in the compatibility prototype.");
  }

  async synchronize() {
    this.log("Synchronizing serprog with SYNCNOP.");

    if (await this.testSync(1200)) {
      this.log("SYNCNOP response OK.");
      return;
    }

    this.log("No immediate SYNCNOP response. Sending NOP drain sequence like rflasher.");
    await this.transport.write(new Uint8Array(8).fill(COMMANDS.NOP));
    const drained = await this.transport.readAvailable(80);
    if (drained.length > 0) {
      this.log(`Drained ${drained.length} byte(s): ${formatBytes(drained).toUpperCase()}`);
    }

    for (let attempt = 1; attempt <= 8; attempt += 1) {
      this.log(`SYNCNOP retry ${attempt}/8.`);
      if (await this.testSync(1200)) {
        this.log("SYNCNOP response OK.");
        return;
      }
      await sleep(50);
    }

    throw new Error(
      "No serprog SYNCNOP response. The selected device may not be in serprog/SPI flash adapter mode, the wrong serial port may be selected, or another app may hold the port."
    );
  }

  async testSync(timeoutMs) {
    await this.transport.readAvailable(20);
    await this.transport.write(new Uint8Array([COMMANDS.SYNCNOP]));

    try {
      const response = await this.transport.readExact(2, timeoutMs);
      if (response[0] === NAK && response[1] === ACK) {
        return true;
      }
      this.log(`SYNCNOP expected NAK ACK, got ${formatBytes(response).toUpperCase()}.`);
      return false;
    } catch (error) {
      if (error instanceof SerialTimeoutError) {
        return false;
      }
      throw error;
    }
  }

  async queryIface() {
    const bytes = await this.command(COMMANDS.Q_IFACE, new Uint8Array(), 2);
    const version = bytes[0] | (bytes[1] << 8);
    if (version !== 1) {
      throw new Error(`Unsupported serprog interface version ${version}.`);
    }
    return version;
  }

  async queryCommandMap() {
    return this.command(COMMANDS.Q_CMDMAP, new Uint8Array(), 32);
  }

  async queryProgrammerName() {
    const bytes = await this.command(COMMANDS.Q_PGMNAME, new Uint8Array(), 16);
    const end = bytes.indexOf(0);
    const usable = end === -1 ? bytes : bytes.slice(0, end);
    return new TextDecoder().decode(usable);
  }

  async queryBusType() {
    return (await this.command(COMMANDS.Q_BUSTYPE, new Uint8Array(), 1))[0];
  }

  async setBusType(busType) {
    const status = await this.statusCommand(COMMANDS.S_BUSTYPE, new Uint8Array([busType]));
    if (!status) {
      throw new Error("Adapter rejected SPI bus selection.");
    }
  }

  async queryLe16(command) {
    const bytes = await this.command(command, new Uint8Array(), 2);
    return bytes[0] | (bytes[1] << 8);
  }

  async queryLe24(command) {
    const bytes = await this.command(command, new Uint8Array(), 3);
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
  }

  async setSpiFrequency(freqHz) {
    const payload = new Uint8Array([
      freqHz & 0xff,
      (freqHz >> 8) & 0xff,
      (freqHz >> 16) & 0xff,
      (freqHz >> 24) & 0xff,
    ]);
    const response = await this.command(COMMANDS.S_SPI_FREQ, payload, 4);
    const actual = response[0] | (response[1] << 8) | (response[2] << 16) | (response[3] << 24);
    if (this.info) {
      this.info.spiFrequency = actual;
    }
    this.log(`SPI frequency set to ${actual.toLocaleString()} Hz.`);
    return actual;
  }

  async setPinState(enabled) {
    await this.statusCommand(COMMANDS.S_PIN_STATE, new Uint8Array([enabled ? 1 : 0]));
  }

  async writeEnable() {
    await this.spiOp(new Uint8Array([0x06]), 0, 2000);
  }

  async readStatus() {
    return (await this.spiOp(new Uint8Array([0x05]), 1, 2000))[0];
  }

  async waitWhileBusy({ timeoutMs = 30000, intervalMs = 20, onProgress = () => {} } = {}) {
    const started = performance.now();
    let status = await this.readStatus();

    while (status & 0x01) {
      const elapsed = performance.now() - started;
      if (elapsed > timeoutMs) {
        throw new Error(`SPI flash stayed busy for more than ${timeoutMs} ms.`);
      }
      onProgress({ done: Math.min(elapsed, timeoutMs), total: timeoutMs, phase: "Waiting" });
      await sleep(intervalMs);
      status = await this.readStatus();
    }
  }

  async pageProgram(address, data) {
    if (data.length < 1 || data.length > DEFAULT_PAGE_SIZE) {
      throw new Error(`Page program length must be 1-${DEFAULT_PAGE_SIZE} bytes.`);
    }
    if ((address & 0xff) + data.length > DEFAULT_PAGE_SIZE) {
      throw new Error("Page program must not cross a 256-byte page boundary.");
    }

    if (!Number.isInteger(address) || address < 0 || address >= ADDRESS_32BIT_LIMIT) {
      throw new Error("Page program address must be a 32-bit SPI flash address.");
    }

    const addressCommand = this.buildAddressCommand(address, { read: false });
    const command = new Uint8Array(addressCommand.length + data.length);
    command.set(addressCommand, 0);
    command.set(data, addressCommand.length);

    await this.writeEnable();
    await this.spiOp(command, 0, 4000);
    await this.waitWhileBusy({ timeoutMs: 5000, intervalMs: 10 });
  }

  buildAddressCommand(address, { read }) {
    const use4ByteAddress = address >= ADDRESS_24BIT_LIMIT || (this.lastProbe?.chip?.capacity ?? 0) > ADDRESS_24BIT_LIMIT;
    if (use4ByteAddress) {
      return new Uint8Array([
        read ? 0x13 : 0x12,
        Math.floor(address / 0x1000000) & 0xff,
        (address >> 16) & 0xff,
        (address >> 8) & 0xff,
        address & 0xff,
      ]);
    }

    return new Uint8Array([
      read ? 0x03 : 0x02,
      (address >> 16) & 0xff,
      (address >> 8) & 0xff,
      address & 0xff,
    ]);
  }

  async spiOp(writeBytes, readLength, timeoutMs = 3000) {
    const payload = new Uint8Array(6 + writeBytes.length);
    payload[0] = writeBytes.length & 0xff;
    payload[1] = (writeBytes.length >> 8) & 0xff;
    payload[2] = (writeBytes.length >> 16) & 0xff;
    payload[3] = readLength & 0xff;
    payload[4] = (readLength >> 8) & 0xff;
    payload[5] = (readLength >> 16) & 0xff;
    payload.set(writeBytes, 6);

    return this.command(COMMANDS.O_SPIOP, payload, readLength, timeoutMs);
  }

  async command(command, payload, responseLength, timeoutMs = 2000) {
    this.ensureConnected();
    const packet = new Uint8Array(1 + payload.length);
    packet[0] = command;
    packet.set(payload, 1);
    await this.transport.write(packet);

    const ack = (await this.transport.readExact(1, timeoutMs))[0];
    if (ack === NAK) {
      throw new Error(`serprog command 0x${command.toString(16)} returned NAK.`);
    }
    if (ack !== ACK) {
      throw new Error(`serprog command 0x${command.toString(16)} expected ACK, got 0x${ack.toString(16)}.`);
    }

    return responseLength > 0 ? this.transport.readExact(responseLength, timeoutMs) : new Uint8Array();
  }

  async statusCommand(command, payload, timeoutMs = 2000) {
    this.ensureConnected();
    const packet = new Uint8Array(1 + payload.length);
    packet[0] = command;
    packet.set(payload, 1);
    await this.transport.write(packet);
    const status = (await this.transport.readExact(1, timeoutMs))[0];
    if (status !== ACK && status !== NAK) {
      throw new Error(`serprog status command 0x${command.toString(16)} returned 0x${status.toString(16)}.`);
    }
    return status === ACK;
  }

  ensureConnected() {
    if (!this.transport) {
      throw new Error("Not connected.");
    }
  }

  requireIdentifiedChip() {
    this.ensureConnected();
    const chip = this.lastProbe?.chip;
    if (!chip?.capacity) {
      throw new Error("Probe and identify a supported SPI flash chip before full-chip operations.");
    }
    return chip;
  }

  safeReadChunkSize(requested) {
    const adapterLimit = this.info?.maxRead || DEFAULT_READ_CHUNK;
    return Math.max(1, Math.min(requested, DEFAULT_READ_CHUNK, adapterLimit));
  }
}

export function formatBytes(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(" ");
}

function formatAddress(address) {
  return `0x${address.toString(16).padStart(address >= ADDRESS_24BIT_LIMIT ? 8 : 6, "0")}`;
}

function sleep(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}
