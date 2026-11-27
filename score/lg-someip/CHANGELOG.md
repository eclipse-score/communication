# Changelog

All notable changes to LG SOME/IP are documented here.

## Unreleased

- Initial public GitHub project import and CI workflow.
- Fix the QNX build: link `__progname`, guard `SO_VLANPRIO`, and add the
  missing include path for the QNX message-passing server.
- Build the example applications on QNX and add a QNX build guide.
- Receive IPv4/IPv6 SD Endpoint Options (0x24/0x26) and skip unknown SD option
  types by length instead of dropping the whole SD message
  ([#1661](https://github.com/eclipse-score/communication/issues/1661)).
