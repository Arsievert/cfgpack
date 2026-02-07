Getting Started
===============

Quick Start
-----------

Include just ``cfgpack/cfgpack.h``; it re-exports the public API surface.

.. code-block:: c

   #include "cfgpack/cfgpack.h"

   cfgpack_entry_t entries[128];
   cfgpack_value_t defaults[128];
   cfgpack_schema_t schema;
   cfgpack_parse_error_t err;
   cfgpack_value_t values[128];
   uint8_t present[(128+7)/8];
   uint8_t scratch[4096];

   // Parse schema from .map file
   cfgpack_parse_schema(map_data, map_len, &schema, entries, 128, defaults, &err);
   
   // Initialize runtime context
   cfgpack_init(&ctx, &schema, values, 128, defaults, present, sizeof(present));

   // Use typed convenience functions
   uint16_t speed;
   cfgpack_get_u16_by_name(&ctx, "maxsp", &speed);
   cfgpack_set_u16_by_name(&ctx, "maxsp", 100);

   // Serialize to MessagePack
   size_t len;
   cfgpack_pageout(&ctx, scratch, sizeof(scratch), &len);

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

Building
--------

.. code-block:: bash

   make              # builds build/out/libcfgpack.a
   make tests        # builds all test binaries
   make tools        # builds compression tool

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
