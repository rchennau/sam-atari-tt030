/* Atari TT030 (32 MHz 68030) build, 2026-09-12: cheapest crypto only.
 * OpenSSH 5.6 on the TT could not finish even a 1024-bit DH kex inside 120 s, so no DH/RSA here. */
#define DROPBEAR_RSA 0
#define DROPBEAR_DSS 0
#define DROPBEAR_ECDSA 0
#define DROPBEAR_ED25519 1

#define DROPBEAR_CURVE25519 1
#define DROPBEAR_ECDH 0
#define DROPBEAR_DH_GROUP1 0
#define DROPBEAR_DH_GROUP14_SHA1 0
#define DROPBEAR_DH_GROUP14_SHA256 0
#define DROPBEAR_DH_GROUP16 0
#define DROPBEAR_SNTRUP761 0
#define DROPBEAR_MLKEM768 0

#define DROPBEAR_CHACHA20POLY1305 1
#define DROPBEAR_AES128 1
#define DROPBEAR_AES256 0
#define DROPBEAR_ENABLE_CTR_MODE 1
#define DROPBEAR_ENABLE_CBC_MODE 0
#define DROPBEAR_SHA1_HMAC 0
#define DROPBEAR_SHA2_256_HMAC 1
#define DROPBEAR_SHA2_512_HMAC 0

#define DROPBEAR_SVR_PASSWORD_AUTH 0
#define DROPBEAR_SVR_PUBKEY_AUTH 1
#define DROPBEAR_X11FWD 0
#define DROPBEAR_SVR_AGENTFWD 0
#define DROPBEAR_SVR_LOCALTCPFWD 0
#define DROPBEAR_SVR_REMOTETCPFWD 0

#define DSS_PRIV_FILENAME "/etc/dropbear/dropbear_dss_host_key"
#define RSA_PRIV_FILENAME "/etc/dropbear/dropbear_rsa_host_key"
#define ECDSA_PRIV_FILENAME "/etc/dropbear/dropbear_ecdsa_host_key"
#define ED25519_PRIV_FILENAME "/etc/dropbear/dropbear_ed25519_host_key"
#define DROPBEAR_PIDFILE "/var/run/dropbear.pid"
#define DEFAULT_PATH "/sbin:/bin:/usr/sbin:/usr/bin"
#define SFTPSERVER_PATH "/usr/libexec/openssh/sftp-server"
#define DROPBEAR_SVR_DROP_PRIVS 0   /* MiNTLib has no setresgid(); single-user TT runs sessions as root anyway */
#define DROPBEAR_SVR_MULTIUSER 1    /* MiNT is a multiuser kernel: MULTIUSER 0 exits "requires a non-multiuser kernel" (TT, 2026-09-12) */
#define DROPBEAR_SVR_LOCALSTREAMFWD 0   /* unix-socket forwarding off: needed with DROP_PRIVS 0 + MULTIUSER 1 */
#define DROPBEAR_SVR_REMOTESTREAMFWD 0
