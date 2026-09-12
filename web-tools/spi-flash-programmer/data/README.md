# SPI NOR identification database

The browser database is generated from **flashrom**, revision
[`399bffcb7d0dc3de96223163e3477f1137df319f`](https://github.com/flashrom/flashrom/tree/399bffcb7d0dc3de96223163e3477f1137df319f).
It contains 225 three-byte JEDEC IDs representing 268 upstream records.
This is a documented command-compatible subset, **not hardware certification**.

## Sources and regeneration

`chip-database-provenance.json` records each included upstream model, vendor,
capacity in bytes, operating voltage in millivolts, upstream test status and
source filename. Source URLs are `<upstream>/blob/<revision>/<source>`.
The same revision's `include/flashchips.h` defines the manufacturer/device IDs;
`include/flash.h` defines addressing feature flags. The JSON also records
exclusions and their reasons. Multiple upstream variants sharing an ID are
retained; a three-byte probe cannot determine their exact suffix.

From this directory's parent:

```sh
git clone https://github.com/flashrom/flashrom.git /tmp/flashrom-reference
git -C /tmp/flashrom-reference checkout 399bffcb7d0dc3de96223163e3477f1137df319f
python3 scripts/generate-chip-database.py /tmp/flashrom-reference
```

The generator requires the exact revision and a clean checkout. It copies facts
from explicit records, without extrapolating families, suffixes or capacities
from the density byte. Generated runtime data is separate from the larger audit
JSON, which the browser does not download. Retained upstream copyright notices
and GPL-2.0-or-later license text accompany these derived data.

## Inclusion rules

- SPI bus, standard three-byte RDID probe, one-byte manufacturer ID and explicit
  two-byte device ID. Extended/continuation IDs and alternate probes are omitted.
- Documented operation at 3.3 V. This subset does not include 1.8 V-only devices.
- Standard SPI read, 256-byte page program and documented C7 chip erase.
- Above 16 MiB, both native 13h read and 12h program must be documented.
- Capacity at most 64 MiB, avoiding unbounded full-dump browser allocations.
- No per-die status or special prepare/finish access hooks.
- **Every upstream record sharing an ID must pass**, and capacities must agree.
  An unsupported variant is not silently discarded in favor of a compatible one.

The adapter still assumes ordinary SPI mode and an unprotected chip. It does
not implement flashrom's unlock procedures, vendor-specific initialization,
SFDP parameter parsing, or automatic voltage selection. An upstream test status
is evidence about flashrom, not a test performed with this browser programmer.
Unknown/excluded IDs remain blocked for full-chip operations.

## Changes from the original 20-entry database

- Boya/BoHong `684017` identifies an 8 MiB `B.25Q64AS`, the upstream family label
  covering the BY25Q64AS reported in
  [issue #175](https://github.com/geo-tp/ESP32-Bit-Pirate/issues/175).
  No fabricated `684016` sibling is added.
- Micron's N25Q entries use documented IDs such as `20ba17`; the old `204016`,
  `204017`, `204018` Micron labels are replaced with upstream XMC identities.
- `204015` is excluded: shared M45PE16/XMC identification, and not every variant
  has C7 erase documented.
- `ef4019` is excluded: W25Q256FV lacks the native 12h program feature in this
  revision, whereas W25Q256JV_Q supports it. The old generic W25Q256 entry
  incorrectly assumed the same write command for both. Supporting this ID needs
  additional disambiguation or a separately verified addressing implementation.

## Verification

Serve the repository with Python and open
`web-tools/spi-flash-programmer/tests/` for browser module tests. Tests exercise
known and unknown IDs, malformed inputs, Boya probing/full-dump chunking, and
native 4-byte read/program commands with a mocked SPI transport. They do not
substitute for testing read/erase/write on physical chips.
