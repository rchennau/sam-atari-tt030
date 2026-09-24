sam-built RPMs (mawk, ttmqtt, yum-tt 1.10, t425-compress 1.0) unpacked into the card image by scripts/build_hd10_ext2.sh (after the SpareMiNT base)
so a fresh card can run yum. The per-card build-request key /etc/yum.tt/request.key is NOT here:
install it after flashing (docs/runbooks/tt030-packages.md, Key rotation). Refresh with the mirror's built/.
