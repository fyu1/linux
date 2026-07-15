.. SPDX-License-Identifier: GPL-2.0

====
MPAM
====

What is MPAM
============
MPAM (Memory Partitioning and Monitoring) is a feature in the CPUs and memory
system components such as the caches or memory controllers that allow memory
traffic to be labelled, partitioned and monitored.

Traffic is labelled by the CPU, based on the control or monitor group the
current task is assigned to using resctrl.  Partitioning policy can be set
using the schemata file in resctrl, and monitor values read via resctrl.
See Documentation/filesystems/resctrl.rst for more details.

This allows tasks that share memory system resources, such as caches, to be
isolated from each other according to the partitioning policy (so called noisy
neighbours).

Supported Platforms
===================
Use of this feature requires CPU support, support in the memory system
components, and a description from firmware of where the MPAM device controls
are in the MMIO address space. (e.g. the 'MPAM' ACPI table).

The MMIO device that provides MPAM controls/monitors for a memory system
component is called a memory system component. (MSC).

Because the user interface to MPAM is via resctrl, only MPAM features that are
compatible with resctrl can be exposed to user-space.

MSC are considered as a group based on the topology. MSC that correspond with
the L3 cache are considered together, it is not possible to mix MSC between L2
and L3 to 'cover' a resctrl schema.

The supported features are:

* Cache portion bitmap controls (CPOR) on the L2 or L3 caches.  To expose
  CPOR at L2 or L3, every CPU must have a corresponding CPU cache at this
  level that also supports the feature.  Mismatched big/little platforms are
  not supported as resctrl's controls would then also depend on task
  placement.

* Memory bandwidth maximum controls (MBW_MAX) on or after the L3 cache.
  resctrl exposes these as the ``MB`` resource.  The domain identifiers
  used in the ``MB:`` schemata line depend on which MSC group backs the
  resource:

  **L3-cache MSC (cache-level MB control).**
  When the MB control hardware sits on the L3 cache MSC, resctrl uses L3
  cache-ids to identify where bandwidth is applied.  The topology of the
  MSC group must match the L3 cache topology so that cache-ids can be
  repainted.  If the memory bandwidth control is on the memory rather
  than the L3 then there must be a single global L3 as otherwise it is
  unknown which L3 the traffic came from.  There must be no caches
  between the L3 and the memory so that the two ends of the path have
  equivalent traffic.

  **Memory MSC (memory-level MB control).**
  When the MB control hardware sits on a memory MSC above L3, resctrl uses
  NUMA node identifiers instead of L3 cache-ids.  The native node-scoped
  control is exposed as ``MB_NODE``; the legacy ``MB`` control keeps the
  ``MB:`` schemata line for backward compatibility and, in the default
  legacy mode, is emulated by ``MB_NODE`` when it has no hardware of its
  own.  See the MB control emulation section in
  Documentation/filesystems/resctrl.rst for the control layout and
  emulation modes.

  On either path, read the control ``scope`` file under
  ``info/MB/resource_schemata/`` (``L3`` or ``NODE``) to learn which
  identifier space the ``MB:`` line uses.

  When ``scope`` reads ``NODE``, each numeric ID ``XX`` in the ``MB:``
  line is a NUMA node id and corresponds to the standard NUMA node
  directory ``/sys/devices/system/node/nodeXX/``.  That directory
  describes the node: ``cpulist``/``cpumap`` (the CPUs local to it, empty
  for a CPU-less node), ``distance`` (NUMA distances to other nodes),
  ``meminfo`` (its memory), and its ``memoryX`` symlinks.  Use these
  files to map an ``MB:`` (or ``MB_NODE:``) entry to a physical NUMA node
  and its CPUs and memory.

  **CPU-less NUMA nodes.**
  A memory MSC may be associated with a NUMA node that has no local CPUs
  (for example a memory-only node that still participates in bandwidth
  control).  The driver falls back to ``cpu_possible_mask`` for the MSC
  affinity so that traffic from remote CPUs is still accounted for and
  the node can appear as an ``MB``/``MB_NODE`` domain.

  When the MPAM driver finds multiple groups of MSC it can use for the
  ``MB`` resource, it prefers the group closest to the L3 cache.

* Cache Storage Usage (CSU) counters can expose the 'llc_occupancy' provided
  there is at least one CSU monitor on each MSC that makes up the L3 group.
  Exposing CSU counters from other caches or devices is not supported.

* Memory Bandwidth Usage (MBWU) on or after the L3 cache.  resctrl can
  expose ``mbm_total_bytes`` from either an L3-cache MSC or a memory MSC:

  **L3-cache MSC.**
  When MBWU monitors sit on the L3 cache MSC, counters are exposed on the
  ``L3_MON`` resource and use L3 cache-ids.  The MSC group topology must
  match the L3 cache topology so that cache-ids can be repainted.

  **Memory MSC.**
  When MBWU monitors sit on a memory MSC above L3, counters are exposed on
  the ``MB`` resource with node scope.  Monitor directories are named
  ``mon_NODE_XX`` where ``XX`` is the same NUMA node identifier that appears
  in the ``MB:`` schemata line.  CPU-less memory nodes are supported the
  same way as for MB controls above.

  ``mbm_local_bytes`` is not exposed as MPAM cannot distinguish local
  traffic from global traffic on these paths.

  When only L3-scoped monitoring was supported, platforms with memory
  bandwidth monitors on CPU-less NUMA nodes could not expose
  ``mbm_total_bytes``.  Node-scoped monitoring on the ``MB`` resource
  removes that restriction for memory-class MSCs.

Reporting Bugs
==============
If you are not seeing the counters or controls you expect please share the
debug messages produced when enabling dynamic debug and booting with:
dyndbg="file mpam_resctrl.c +pl"
