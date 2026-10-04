import { runTests } from "./protocol-tests.js";
import { runPipelineTests } from "./pipeline-tests.js";
import { runSweepTests } from "./sweep-tests.js";
const protocol = await runTests(console.log), dsp = await runPipelineTests(console.log), sweep = await runSweepTests(console.log);
const result = { total: protocol.total + dsp.total + sweep.total, failures: protocol.failures + dsp.failures + sweep.failures };
console.log(result);
if (result.failures) process.exitCode = 1;
