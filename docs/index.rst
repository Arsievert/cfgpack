CFGPack Documentation
=====================

A MessagePack-based configuration library for embedded systems.

**Embedded profile:** no heap allocation. All buffers are caller-owned.
Hard caps: max 128 schema entries.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   getting-started
   api-reference
   versioning
   compression
   littlefs
   stack-analysis
   fuzz-testing

Features
--------

- Fixed-cap schema (up to 128 entries) with typed values (u/i 8-64, f32/f64, str/fstr)
- Parses ``.map``, JSON, and MessagePack binary schemas into caller-owned storage; no heap allocations
- Default values for schema entries, automatically applied at initialization
- Set/get by index and by schema name with type/length validation
- MessagePack encoding/decoding with size caps
- CRC-32C integrity checking on all serialized blobs (always on, verified on pagein)
- Measure-then-allocate for serialization via ``cfgpack_pageout_measure()``
- Schema versioning with embedded schema name for version detection
- Index remapping and type widening for schema migrations
- Optional LZ4/heatshrink decompression support
- Optional LittleFS storage wrappers for flash-based embedded systems

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
