CFGPack Documentation
=====================

A MessagePack-based configuration library for embedded systems.

**Embedded profile:** no heap allocation. All buffers are caller-owned.
Hard caps: max 128 schema entries.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   getting-started
   api
   api-reference
   versioning
   compression
   stack-analysis

Features
--------

- Fixed-cap schema (up to 128 entries) with typed values (u/i 8-64, f32/f64, str/fstr)
- Parses ``.map`` specs into caller-owned schema; no heap allocations
- Default values for schema entries, automatically applied at initialization
- Set/get by index and by schema name with type/length validation
- MessagePack encoding/decoding with size caps
- Schema versioning with embedded schema name for version detection
- Index remapping and type widening for schema migrations
- Optional LZ4/heatshrink decompression support

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
