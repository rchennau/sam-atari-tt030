# SAM SCP TT (`sam-scp-tt`) — Transputer-Assisted SCP Bridge for Atari TT030

`sam-scp-tt` is a transputer-assisted SCP file transfer acceleration utility for the Atari TT030. It couples the host 68030 processor with the ATW800/2 INMOS T425 transputer to offload packet checksumming, buffer management, and payload framing during remote secure copy transfers over MiNTnet or STiNG network links.

---

## Hardware & Operating System Support
- **Target Workstation**: Atari TT030 (MC68030 @ 32 MHz)
- **Co-Processor**: ATW800/2 Transputer Board (T425 RISC Transputer @ 40 MHz)
- **Bus Driver**: `fpgabios.tos` resident link driver (`trap_1` OS extensions)

---

## Build Instructions

### Prerequisites
- Cross-compiler: `m68k-atari-mint-gcc`
- Transputer toolchain: INMOS `icc` / `ilink` (for custom server extensions)

### 1. Build Host Bridge Binary (`sam-scp-tt.ttp`)
From the repository root:
```bash
m68k-atari-mint-gcc -m68020-60 -O2 -s -o sam-scp-tt/sam-scp-tt.ttp src/x25519bench/sam_scp_tt.c
```

### 2. Transputer Firmware Link
`sam-scp-tt` uses `xserv2.btl` combined binary server staged under `C:\ATW800_2\TRANS_DEV\xserv.btl` or `/etc/dropbear/xserv.btl`.

---

## Usage & Execution Instructions

1. **Deploy Binary & Server Firmware**:
   - Copy `sam-scp-tt.ttp` to `C:\ATW800_2\ATWTOOLS\SAMSCP.TTP` or `/usr/bin/sam-scp-tt`.
   - Ensure `fpgabios.tos` is loaded in `AUTO/` folder.

2. **Run Acceleration Bridge**:
   ```bash
   sam-scp-tt /etc/dropbear/xserv.btl
   ```

3. **Console Output on Clean Bootstrap**:
   ```
   sam-scp-tt: SCP transputer acceleration bridge active on ATW800/2 T425
   ```

4. **Interactive File Transfer**:
   Transfers initiated via `scp` automatically utilize the resident bridge for accelerated framing and transputer memory transfers.
