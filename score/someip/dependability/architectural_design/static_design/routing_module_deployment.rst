Routing Module Deployment
=========================

On QNX, deploy ``mods-someip-routing.so`` under ``<prefix>/bin`` and load it with ``mount -T io-sock``. The module
runs inside io-sock and refuses unloading because it has no coordinated kernel-thread shutdown. On Linux, deploy and
run ``linux_routing_module`` under ``<prefix>/bin``; SIGINT and SIGTERM stop the standalone process.

Both deployments use ``<prefix>/etc/mw_com_config.json`` and an mw::log configuration file:

- The configuration directory is ``<prefix>/etc`` for a binary installed as ``<prefix>/bin/<binary>``
  (``ModuleConfigDirectory()``); the module finds its own path with ``dladdr()``.
- ``MW_LOG_CONFIG_FILE`` is set to ``logging.json`` in that directory unless already set, since mw::log otherwise
  looks next to the hosting executable. Logging configuration is process-wide and must be set before mw::log is first
  initialized. The module initializes mw::log before starting the transport.
- The C transport formats messages in a fixed-size buffer and forwards them to mw::log with their corresponding severity.
- mw::com is initialized with ``mw_com_config.json`` from that directory. A missing or unreadable file fails startup
  with ``ENOENT`` instead of terminating the host process.
- ``RoutingBridge`` starts receiving routing commands; a failure fails startup with its errno.
