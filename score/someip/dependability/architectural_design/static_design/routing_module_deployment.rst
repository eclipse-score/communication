Routing Module Deployment
=========================

Install the routing application under ``/opt/someip_routing``. On QNX, load
``/opt/someip_routing/bin/mods-someip-routing.so`` with ``mount -T io-sock``. The module runs inside io-sock and refuses
unloading because it has no coordinated kernel-thread shutdown. On Linux, run
``/opt/someip_routing/bin/linux_routing_module``; SIGINT and SIGTERM stop the standalone process.

Both deployments use ``/opt/someip_routing/etc/mw_com_config.json`` and an mw::log configuration file:

- ``ModulePath()`` returns the installed Linux executable or QNX module path under ``/opt/someip_routing/bin``;
  ``ModuleConfigDirectory()`` resolves the sibling ``etc`` directory.
- ``MW_LOG_CONFIG_FILE`` is set to ``logging.json`` in that directory unless already set, since mw::log otherwise
  looks next to the hosting executable. Logging configuration is process-wide and must be set before mw::log is first
  initialized. The module initializes mw::log before starting the transport.
- The C transport formats messages in a fixed-size buffer and forwards them to mw::log with their corresponding severity.
- mw::com is initialized with ``mw_com_config.json`` from that directory. A missing or unreadable file fails startup
  with ``ENOENT`` instead of terminating the host process.
- ``RoutingBridge`` starts receiving routing commands; a failure fails startup with its errno.
