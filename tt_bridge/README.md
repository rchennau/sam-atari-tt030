# SAM TT-Bridge HTTP Client for Atari TT030

The **SAM TT-Bridge HTTP Client** is a lightweight M68K C application designed to run natively on the Atari TT030 under TOS 3.06 or FreeMiNT, establishing HTTP/1.1 communication with host services listening on port 8080 (such as SAM orchestrator APIs, bridge endpoints, or local network telemetry).

---

## Features
- **Dual Network Stack Support**: Compatible with MiNT socket layer (`sys/socket.h`) and STiNG TCP/IP stack (`transprt.h`).
- **Atari TT030 Native Optimizations**: Built with `-m68030 -m88881` CPU flags for fast execution in TT-RAM.
- **Port 8080 Target**: Default pre-configured port for SAM TT-Bridge communication.
- **TOS Console & Shell Compatible**: Automatic keypress pause (`Cnecin`) when executed directly from TOS Desktop double-click.

---

## Directory Layout
- `src/main.c`: CLI parser, parameter handling, and TOS interface.
- `src/http_client.h`: Core HTTP client header and response structs.
- `src/http_client.c`: Socket creation, HTTP header/body parser, request transmitter.
- `Makefile`: M68K cross-compilation build definitions (`gcc` / `vbcc`).
- `BUILD.md`: Step-by-step cross-compilation toolchain setup.
