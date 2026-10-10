Files copied over the SpareMiNT rootfs by scripts/build_hd10_ext2.sh. SSH host keys and
root/.ssh/authorized_keys are generated at build time (build/hd10-keys/, gitignored), not stored here.
sbin/ifconfig, sbin/route: the FreeMiNT 1-19-4eb network tools, here so mint.cnf runs them from /sbin
(tt030-fhs-layout FR-1); the sysdir copies on C: stay as a fallback.
