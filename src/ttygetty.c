/* ttygetty — minimal getty for FreeMiNT: open a serial device at a given speed, make it the
 * controlling terminal, and run a login shell on it; respawn when the shell exits.
 *
 * Why: SpareMiNT's mgetty 1.1.22, stty and login (mintlib 0.59) all fail tcgetattr with ENOSYS
 * on MiNT 1.19 (Hatari, 2026-09-12), so the line is never configured. Built with mintlib 0.60.
 * No password: the TT console is a local null-modem cable only.
 *
 * Usage: ttygetty DEVICE SPEED SHELL     e.g. ttygetty /dev/modem2 57600 /c/mint/1-19-4eb/bash
 * Build: m68k-atari-mint-gcc -m68020-60 -Os -s -o ttygetty src/ttygetty.c
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static speed_t speed(const char *s)
{
    switch (atoi(s)) {
    case 9600:  return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    default:    return B115200;
    }
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: ttygetty DEVICE SPEED SHELL\n");
        return 2;
    }
    for (;;) {
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            /* O_NONBLOCK: a blocking open waits for carrier before CLOCAL is set, and this cable gives
             * none — every respawn after the first sat in state D forever (TT, 2026-09-13). */
            int fd = open(argv[1], O_RDWR | O_NONBLOCK);
            if (fd < 0) {
                perror(argv[1]);
                _exit(1);
            }
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
            ioctl(fd, TIOCSCTTY, 0);
            struct termios t;
            if (tcgetattr(fd, &t) == 0) {
                cfsetispeed(&t, speed(argv[2]));
                cfsetospeed(&t, speed(argv[2]));
                /* No flow control. CRTSCTS was on by default: the TT then transmits only while CTS
                 * (fractal's RTS) is up, and fractal drops RTS whenever a program closes the port —
                 * output froze mid-stream, the shell spun, and each respawn sat in state D waiting to
                 * drain. `stty -crtscts` on the TT released it at once (2026-09-13). No IXON either:
                 * a stray ^S in serial data would stop output the same way. */
                t.c_iflag = ICRNL;
                t.c_oflag = OPOST | ONLCR;
                t.c_cflag = (t.c_cflag & ~(CSIZE | PARENB | CSTOPB | CRTSCTS)) | CS8 | CREAD | CLOCAL;
                t.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
                if (tcsetattr(fd, TCSANOW, &t) != 0)
                    perror("ttygetty: tcsetattr");
            } else {
                perror("ttygetty: tcgetattr");
            }
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2)
                close(fd);
            setenv("TERM", "vt100", 1);
            execl(argv[3], "-bash", (char *)NULL);
            perror(argv[3]);
            _exit(1);
        }
        if (pid > 0)
            waitpid(pid, NULL, 0);
        sleep(2);
    }
}
