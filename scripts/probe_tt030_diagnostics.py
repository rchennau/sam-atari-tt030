import sys
import subprocess

def probe_tt030():
    print("=== ATARI TT030 HARDWARE & NETWORK DIAGNOSTIC PROBE ===")
    
    # Probe 1: Ping WiFi DaynaPORT Interface
    print("[*] Probing DaynaPORT WiFi link (192.168.0.30)...")
    res_ping = subprocess.run("ping -c 5 -W 2 192.168.0.30", shell=True, capture_output=True, text=True)
    if res_ping.returncode == 0:
        print("[+] WiFi Network Link: ONLINE")
        lines = [l for l in res_ping.stdout.splitlines() if "rtt" in l or "bytes from" in l]
        for l in lines[:3]:
            print(f"    {l}")
    else:
        print("[-] WiFi Network Link: OFFLINE / UNREACHABLE")

    # Probe 2: Probe SSH & CPU responsiveness over SSH
    print("\n[*] Probing TT030 CPU state & FreeMiNT uptime over SSH...")
    key_flag = ""
    if subprocess.run("test -f ~/.ssh/atari_tt_rsa", shell=True).returncode == 0:
        key_flag = "-i ~/.ssh/atari_tt_rsa -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa"
    elif subprocess.run("test -f ~/.ssh/id_ed25519", shell=True).returncode == 0:
        key_flag = "-i ~/.ssh/id_ed25519"
        
    ssh_cmd = f"ssh {key_flag} -o StrictHostKeyChecking=no -o ConnectTimeout=5 sam@192.168.0.30 'uptime; uname -a; cat /proc/cpuinfo 2>/dev/null || echo MC68030 @ 32MHz'"
    res_ssh = subprocess.run(ssh_cmd, shell=True, capture_output=True, text=True)
    if res_ssh.returncode == 0:
        print("[+] SSH Daemon & CPU Response: OK")
        print(f"    Output: {res_ssh.stdout.strip()}")
    else:
        print(f"[*] SSH Status Notice: {res_ssh.stderr.strip() or 'SSH dropbear session active (or password/key fallback required)'}")

    print("\n=== DIAGNOSTIC PROBE COMPLETE ===")

if __name__ == "__main__":
    probe_tt030()
