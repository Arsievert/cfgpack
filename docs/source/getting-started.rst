Getting Started
===============

Quick Start
-----------

Include just ``cfgpack/cfgpack.h``; it re-exports the public API surface.

.. code-block:: c

   #include "cfgpack/cfgpack.h"

   cfgpack_entry_t entries[128];
   cfgpack_schema_t schema;
   cfgpack_parse_error_t err;
   cfgpack_value_t values[128];
   char str_pool[256];
   uint16_t str_offsets[128];
   uint8_t scratch[4096];

   // Parse schema — defaults are written directly into values[] and str_pool[]
   cfgpack_parse_schema(map_data, map_len, &schema, entries, 128,
                        values, str_pool, sizeof(str_pool), str_offsets, 128, &err);

   // Initialize runtime context — values and str_pool already contain defaults
   cfgpack_ctx_t ctx;
   cfgpack_init(&ctx, &schema, values, 128,
                str_pool, sizeof(str_pool), str_offsets, 128);

   // Use typed convenience functions
   uint16_t speed;
   cfgpack_get_u16_by_name(&ctx, "maxsp", &speed);
   cfgpack_set_u16_by_name(&ctx, "maxsp", 100);

   // Serialize to MessagePack
   size_t len;
   cfgpack_pageout(&ctx, scratch, sizeof(scratch), &len);

.. note::

   The example above uses fixed-size arrays (128 entries). If the schema size
   is not known at compile time, use ``cfgpack_schema_measure()`` to learn
   exact buffer sizes before allocating. This requires only 32 bytes of stack
   instead of ~8KB for oversized discovery buffers. See the
   `API Reference <api-reference.html>`_ and ``examples/allocate-once/`` for
   the full measure-then-allocate pattern, ``examples/low_memory/`` for a
   complete example combining measure-then-allocate with schema migration, or
   ``examples/fleet_gateway/`` for msgpack binary schemas with a three-version
   migration chain using ``cfgpack_schema_measure_msgpack()``.

Map Format
----------

Schema files use a simple text format:

- First line: ``<name> <version>`` header (e.g., ``vehicle 1``)
- Entry lines: ``INDEX NAME TYPE DEFAULT  # optional description``

Example:

.. code-block:: text

   vehicle 1

   1  id     u32   0        # Unique vehicle identifier
   2  model  fstr  "MX500"  # Model code (max 16 chars)
   3  vin    str   NIL      # Vehicle identification number

   10 maxsp  u16   120      # Maximum speed in km/h
   11 accel  f32   2.5      # Acceleration limit in m/s^2

Default Values
~~~~~~~~~~~~~~

Each schema entry requires a default value:

- ``NIL`` — no default; value must be explicitly set before use
- Integer literals: ``0``, ``42``, ``-5``, ``0xFF``, ``0b1010``
- Float literals: ``3.14``, ``-1.5e-3``, ``0.0``
- Quoted strings: ``"hello"``, ``""``, ``"default value"``

Alternative Schema Formats
~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to ``.map`` text files, CFGPack supports two other schema formats:

- **JSON** — parsed by ``cfgpack_schema_parse_json()``, useful for tooling
  integration and human-readable interchange.
- **MessagePack binary** — a compact binary encoding parsed by
  ``cfgpack_schema_parse_msgpack()``. Use the ``cfgpack-schema-pack`` tool
  (``make tools``) to convert ``.map`` or ``.json`` schemas to binary. See the
  `API Reference <api-reference.html>`_ for wire-format details.

Building
--------

.. code-block:: bash

   make              # builds build/out/libcfgpack.a
   make tests        # builds all test binaries
   make tools        # builds CLI tools (compression, schema packing)

Build Modes
~~~~~~~~~~~

CFGPack supports two build modes:

.. list-table::
   :header-rows: 1

   * - Mode
     - Default
     - stdio
     - Print Functions
   * - ``CFGPACK_EMBEDDED``
     - Yes
     - Not linked
     - Silent no-ops
   * - ``CFGPACK_HOSTED``
     - No
     - Linked
     - Full printf

To compile in hosted mode:

.. code-block:: bash

   $(CC) -DCFGPACK_HOSTED -Iinclude myapp.c -Lbuild/out -lcfgpack
