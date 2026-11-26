# Security Policy (`SECURITY.md`)

## Supported Versions

| Version | Supported |
|---|---|
| `1.1.x` (`main`) | Yes |
| `< 1.1.0` | No (upgrade to `1.1.0`) |

## Security Scope

`Asmodensius-Native` is a local bare-metal simulation engine and local-only visualization tool:

- **Simulation core (`core/`):** freestanding C23, zero network I/O, zero filesystem I/O, zero dynamic heap allocation, zero external libraries, UBSan trap-mode verified (`ubsan_trap_build`).
- **Local viewers (`demo/viewer_server.py`, `demo/live_server.py`):** intended strictly for localhost (`127.0.0.1`) use by the local operator; never expose viewer servers directly to untrusted public networks.
- **Zero secrets (Law 15):** no API keys, tokens, credentials, or personal data are stored or processed by this repository.

## Reporting a Vulnerability

Please report suspected security defects (for example, out-of-bounds memory access in snapshot/ledger parsing, local viewer path traversal, or WebSocket DoS) privately via email to `0_0@0thernes.art` with:

1. Affected file path, commit SHA, and command line.
2. Minimal reproducible input or snapshot triggering the fault.
3. Observed vs. expected behavior (UBSan trap, crash, or unauthorized file access).

You will receive an acknowledgment within 7 days. Confirmed fixes are committed directly to `main` with a regression test and documented in `HARDENING_LOG.md` and `CHANGELOG.md`.
