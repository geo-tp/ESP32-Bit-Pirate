// SPDX-License-Identifier: MIT
// Read chunks directly: no per-byte queue for the high-volume IQ stream.
export class SdrSerialTransport {
  constructor() {
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.chunk = new Uint8Array();
    this.offset = 0;
    this.closing = null;
  }

  async open(port) {
    if (this.closing) await this.closing;
    this.closing = null;
    if (this.port) throw new Error("The CDC port is already open.");
    this.port = port;
    try {
      await port.open({ baudRate: 115200, dataBits: 8, stopBits: 1, parity: "none",
        flowControl: "none", bufferSize: 262144 });
      this.reader = port.readable.getReader();
      this.writer = port.writable.getWriter();
      await port.setSignals({ dataTerminalReady: true, requestToSend: false });
    } catch (error) {
      await this.close();
      throw error;
    }
  }

  async write(text) {
    if (!this.writer) throw new Error("The CDC port is closed.");
    await withTimeout(this.writer.write(new TextEncoder().encode(text)), 2000, "CDC write timed out.");
  }

  async readExact(length, deadline) {
    const output = new Uint8Array(length);
    let written = 0;
    while (written < length) {
      if (performance.now() >= deadline) throw new Error("Timed out waiting for the SDR adapter.");
      if (this.offset >= this.chunk.length) {
        if (!this.reader) throw new Error("The CDC port is closed.");
        const { value, done } = await withTimeout(this.reader.read(), deadline - performance.now(),
          "Timed out waiting for the SDR adapter. Check SDR mode and the CDC connection.");
        if (done) throw new Error("The CDC port was disconnected.");
        this.chunk = value;
        this.offset = 0;
        if (!value.length) continue;
      }
      const count = Math.min(length - written, this.chunk.length - this.offset);
      output.set(this.chunk.subarray(this.offset, this.offset + count), written);
      this.offset += count;
      written += count;
    }
    return output;
  }

  async readLine(deadline) {
    let line = "";
    while (line.length < 1024) {
      const [byte] = await this.readExact(1, deadline);
      if (byte === 10) return line.trim();
      if (byte !== 13 && (byte < 32 || byte > 126)) {
        throw new Error("Expected SDR adapter text. Reset the device into SDR mode and reconnect.");
      }
      line += String.fromCharCode(byte);
    }
    throw new Error("SDR adapter response is too long.");
  }

  close() {
    if (!this.closing) this.closing = this.closePort();
    return this.closing;
  }

  async closePort() {
    const { reader, writer, port } = this;
    this.reader = this.writer = this.port = null;
    if (reader) {
      try { await reader.cancel(); } catch { /* Device may already be unplugged. */ }
      reader.releaseLock();
    }
    if (writer) {
      // Release even if a timed-out write is still pending, then abort the stream.
      writer.releaseLock();
      try { await port.writable.abort(); } catch { /* Device may already be unplugged. */ }
    }
    if (port) {
      try { await port.close(); } catch { /* Also called when open() fails. */ }
    }
    this.chunk = new Uint8Array();
    this.offset = 0;
  }
}

function withTimeout(promise, ms, message) {
  let timer;
  return Promise.race([promise, new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(message)), Math.max(0, ms));
  })]).finally(() => clearTimeout(timer));
}
