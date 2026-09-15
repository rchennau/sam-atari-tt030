# SAM TT-Bridge HTTP Client for Atari TT030 (`tt_bridge`)

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
- `BUILD.md`: Step-by-step cross-compilation toolchain setup guide.

---

## Build Instructions

### Building with `m68k-atari-mint-gcc` (Recommended)
```bash
cd tt_bridge
make gcc
```
This outputs `bin/ttbridge.prg` optimized for Motorola 68030 / 68882.

### Building with `vbcc`
```bash
cd tt_bridge
make vbcc
```
Outputs `bin/ttbridge_vbcc.prg`.

---

## Execution & Command Line Options

Run `TTBRIDGE.PRG` from MiNT bash shell or GEM TOS desktop:

```bash
TTBRIDGE.PRG -h 192.168.1.1 -p 8080 -u /api/v1/status
```

### CLI Arguments:
- `-h, --host HOST`: Server IP address (default: `192.168.1.1`).
- `-p, --port PORT`: Server TCP port (default: `8080`).
- `-u, --url PATH`: Endpoint path (default: `/api/v1/status`).
- `-m, --method METHOD`: HTTP Method (`GET` / `POST`, default: `GET`).
- `-d, --data DATA`: Payload data string.
- `-o, --out FILE`: File path to output response.

