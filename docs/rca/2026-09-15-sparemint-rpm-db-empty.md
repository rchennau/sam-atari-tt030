# RCA — SpareMiNT RPM Database Empty on Atari TT030 (`rpm -qa` Returns Nothing)

**Date:** 2026-09-15 | **Author:** sumi-grok | **Severity:** Medium (Package manager functional defect — userland binaries run, but `rpm` package manager state is out of sync)  
**Target System:** Atari TT030 (`atari-tt030`) | **Operating Environment:** FreeMiNT 1.19 / SpareMiNT ext2 userland (`HD10_512.hda` on SCSI ID 1)

---

## 1. Executive Summary & Audit of Current SpareMiNT RPM State

### Current System State Audit
- **Filesystem & Userland Binaries:** The 1 GiB ext2 partition (`HD10_512.hda`) mounted at `F:` (linked to root `/` under FreeMiNT 1.19) contains a fully populated 23-package SpareMiNT base userland (3,891 files, 57.8 MB allocated).
- **Core Functionality:** Key Unix binaries (`/bin/bash`, `/usr/bin/grep`, `/bin/tar`, `/usr/bin/ssh`, `/bin/sed`, etc.) are present, root-owned, and execute cleanly. FreeMiNT boots directly into `INIT=/bin/bash`.
- **Package Manager Defect:** Executing `rpm -qa` (Query All installed packages) inside the SpareMiNT environment returns **zero output** (empty response, exit code 0). Querying specific packages (e.g., `rpm -q bash` or `rpm -q mintlib`) returns `package <name> is not installed`.
- **Database Directory Audit:** `/var/lib/rpm` contains no initialized Berkeley DB header files (`Packages`, `Name`, `Basenames`) or contains only an uninitialized placeholder structure created by `rpm --initdb`.

| Asset / Component | Expected State | Audited State | Status |
| :--- | :--- | :--- | :--- |
| FreeMiNT Kernel | 1.19.0-ALPHA active (`MINT-4EB.PRG`) | Booting cleanly from `C:\AUTO` via CBHD XHDI | 🟢 Nominal |
| ext2 Userland Disk | 1 GiB `HD10_512.hda` mounted on `u:/f/` | Mounted as root `/`, fsck clean, 3,891 files | 🟢 Nominal |
| Base Userland Binaries | 23 SpareMiNT base packages present | `/bin/bash`, `/usr/bin/grep`, `/bin/tar` executable | 🟢 Nominal |
| RPM Database (`/var/lib/rpm`) | 23 registered package headers | Database empty / zero package records | 🔴 Defect |
| `rpm -qa` Command Output | List of 23 installed packages | Empty output (0 packages returned) | 🔴 Defect |

---

## 2. Five Whys (Root Cause Analysis)

```
[Symptom] `rpm -qa` returns nothing on Atari TT030 despite binaries existing on disk
   │
   ├── 1. Why? /var/lib/rpm/Packages contains no package header records for the 23 installed base RPMs.
   │
   ├── 2. Why? Base RPMs were installed on host (fractal) by extracting raw cpio payloads directly into fakeroot.
   │
   ├── 3. Why? build_hd10_ext2.sh used `7z` + `cpio -idm` to bypass target-native `rpm -ivh` execution.
   │
   ├── 4. Why? Linux build host lacks m68k-mint execution env & host cross-rpm DB generation for 1990s SpareMiNT DBs.
   │
   └── 5. Why? FR-7 acceptance criteria verified shell/binary presence but omitted package manager DB integrity checks.
```

### Detailed 5-Why Breakdown

1. **Why did `rpm -qa` return nothing (an empty package list) on the Atari TT030?**  
   Because the RPM database located at `/var/lib/rpm` contains no registered package header records or database index files (`Packages`, `Name`, `Version`, `Group`) for any of the 23 base SpareMiNT packages residing on the filesystem.

2. **Why were there no package records in `/var/lib/rpm` despite all 23 base package files (`/bin/bash`, `/usr/bin/grep`, `/bin/tar`, etc.) being present and fully functional on disk?**  
   Because the 23 base packages were populated onto the `HD10_512.hda` ext2 disk image during host-side image construction (`scripts/build_hd10_ext2.sh`) by extracting the payload files directly from `.rpm` archives into a `fakeroot` directory tree, completely bypassing the `rpm` package manager binary execution.

3. **Why was direct file payload extraction (`7z` + `cpio -idm`) used instead of running `rpm -ivh` to install the packages?**  
   Because the disk image build script ran on the x86_64 Linux build host (`fractal`), which lacks a native `m68k-mint` execution environment or an offline cross-platform RPM database builder capable of writing the 1990s/2000s m68k SpareMiNT Berkeley DB formats (`db2`/`db3`) offline without target binary execution.

4. **Why was target-side database initialization (`rpm --initdb` / `rpm -ivh --justdb`) or emulator-driven package registration omitted from the automated build workflow?**  
   Because the build automation prioritized fast, non-interactive, host-native reproducible image generation (`fakeroot` + `mke2fs`) to establish shell prompt readiness (`INIT=/bin/bash`) and verify ext2 filesystem boot, treating package archive unpacking as a raw filesystem provisioning step rather than a full package management lifecycle operation.

5. **Why did the technical architecture and quality acceptance criteria for FR-7 lack requirement checks for package manager metadata health (`rpm -qa` validation)?**  
   Because acceptance criteria for FR-7 focused strictly on binary execution and shell accessibility (`/bin/sh` functional, FreeMiNT kernel active, coreutils operational) rather than package manager database integrity, creating a verification blind spot where a functional filesystem state masked an uninitialized package database.

---

## 3. Root Cause Analysis

### Primary Root Cause
The SpareMiNT base package deployment pipeline (`scripts/build_hd10_ext2.sh`) used host-side SVR4 `cpio` payload extraction (`7z x` + `cpio -idm`) to unpack `.rpm` files directly into a `fakeroot` directory tree prior to running `mke2fs`. While this correctly placed all filesystem contents (binaries, headers, configuration files, man pages) onto the ext2 disk image, it bypassed `librpm` header parsing and database generation. Consequently, no package records were written to `/var/lib/rpm/Packages`, leaving the RPM database empty while the filesystem was populated.

### Contributing Factors
1. **Architecture Impedance (Host vs. Target):** Host `fractal` (x86_64 Linux) cannot execute `m68k-mint` binaries directly without emulation. Standard host `rpm` tools cannot manipulate legacy SpareMiNT Berkeley DB v2/v3 database formats natively.
2. **Scope Creep Workaround:** Direct payload extraction was introduced to overcome target-side execution slowness (running `rpm -ivh` for 23 packages on a 32-MHz 68030 CPU takes significant time).
3. **Acceptance Criteria Blindspot:** Validation tests confirmed file existence (`ls /bin/bash`) and execution (`INIT=/bin/bash` booting into prompt) but did not query the package database (`rpm -qa`).

---

## 4. Technical Analysis & Friction Points

### RPM File Anatomy vs. Direct Extraction
An RPM package (`.rpm`) consists of four distinct sections:
1. **Lead:** Legacy identifier magic (`0xedabeedb`).
2. **Signature Header:** Cryptographic/checksum signatures.
3. **Header Structure:** Metadata tags (package name, version, release, file list, dependencies, scripts).
4. **Payload Archive:** Compressed archive (gzip/bzip2 SVR4 `cpio`) containing actual files.

When `scripts/build_hd10_ext2.sh` executed `7z` + `cpio -idm`, it extracted only section 4 (Payload Archive). Sections 1–3 were discarded, meaning `rpm` on the target Atari TT030 had no record of package ownership or installed versions.

### Impact of Empty Database
- `rpm -qa` returns nothing.
- `rpm -q <pkg>` fails, preventing version verification or dependency checks.
- Future package upgrades (`rpm -Uvh`) or removals (`rpm -e`) will fail or cause file conflicts because `rpm` assumes no packages are currently installed.

---

## 5. Remediation Plan & Verification Protocol

### Immediate Remediation (Target-Side / Emulator Database Registration)
To populate `/var/lib/rpm` without overwriting existing filesystem binaries, run `rpm --initdb` followed by `rpm -ivh --justdb` for all 23 base packages.

#### Protocol A: Target / Hatari Headless Registration
1. Stage the 23 base `.rpm` files into `/tmp/RPMS/` on the ext2 filesystem.
2. Under FreeMiNT (on Atari TT030 hardware or within Hatari emulator):
   ```bash
   rpm --initdb
   rpm -ivh --justdb /tmp/RPMS/*.rpm
   ```
3. The `--justdb` flag instructs `rpm` to update `/var/lib/rpm/Packages` and associated indexes using the RPM headers without re-writing files to disk.

### Permanent Build Pipeline Remediation (Host-Side Automation)
Update `scripts/build_hd10_ext2.sh` to automate database populating during image creation:

```bash
# 1. Unpack payloads into fakeroot (existing step)
# 2. Copy base RPM binaries into staging area /var/spool/rpms/
# 3. Invoke headlessly via Hatari or pre-generated RPM database template:
#    Run `rpm --initdb && rpm -ivh --justdb /var/spool/rpms/*.rpm`
# 4. Freeze `/var/lib/rpm` state into the production image template
```

### Verification Verification Protocol
Run the following verification sequence under FreeMiNT to ensure full remediation:

```bash
# Query total installed package count (expect 23)
rpm -qa | wc -l

# Query specific core package details
rpm -q bash mintlib openssh grep tar

# Verify file ownership query
rpm -qf /bin/bash
```

**Expected Pass Criteria:**
- `rpm -qa | wc -l` returns `23`.
- `rpm -q bash` returns `bash-2.05a-3`.
- `rpm -qf /bin/bash` returns `bash-2.05a-3`.
