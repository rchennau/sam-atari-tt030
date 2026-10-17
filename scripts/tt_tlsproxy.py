#!/usr/bin/env python3
"""tt_tlsproxy.py — TLS-terminating forward proxy for the TT030 (HighWire transputer evaluation, operator 2026-10-17).

The TT speaks plain HTTP to this proxy; the proxy speaks verified HTTPS upstream, so the 68030 does no TLS at all.
A client sends absolute URIs (HighWire's HTTP_PROXY, wget's http_proxy):
  GET https://host/path   fetched over HTTPS
  GET http://host/path    upgraded to HTTPS first; plain HTTP only if the TLS connect fails (--no-upgrade: as asked)
Upstream certificates are verified against the system store, plus --cafile if given. A failed verify is a 502,
never a silent downgrade. Bind it to the LAN address only: the TT->proxy hop is cleartext.
Prototype: GET/HEAD/POST, whole-body buffering, no CONNECT.
  ponytail: buffers each response; stream if pages exceed a few MB.
Usage: tt_tlsproxy.py --listen 192.168.0.239:3128 [--cafile CA.pem] [--no-upgrade]
"""
import argparse
import http.client
import http.server
import socket
import ssl
import sys
import time
from urllib.parse import urlsplit

HOP = {"connection", "keep-alive", "proxy-connection", "proxy-authorization", "te", "trailer",
       "transfer-encoding", "upgrade"}


class Proxy(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"         # one request per connection: the simplest thing old clients all speak
    ctx = None
    upgrade = True

    def _fetch(self, scheme, host, port, path, body, headers):
        if scheme == "https":
            conn = http.client.HTTPSConnection(host, port or 443, context=self.ctx, timeout=30)
        else:
            conn = http.client.HTTPConnection(host, port or 80, timeout=30)
        conn.request(self.command, path, body=body, headers=headers)
        return conn.getresponse()

    def _handle(self):
        u = urlsplit(self.path)
        if u.scheme not in ("http", "https") or not u.hostname:
            self.send_error(400, "absolute http(s) URI required (use this as an HTTP proxy)")
            return
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n) if n else None
        headers = {k: v for k, v in self.headers.items() if k.lower() not in HOP}
        path = (u.path or "/") + (f"?{u.query}" if u.query else "")
        t0 = time.monotonic()
        scheme = u.scheme
        try:
            if scheme == "http" and self.upgrade:
                try:
                    r = self._fetch("https", u.hostname, None if u.port in (None, 80) else u.port, path, body, headers)
                    scheme = "https"
                except ssl.SSLCertVerificationError:
                    raise
                except (OSError, ssl.SSLError):
                    r = self._fetch("http", u.hostname, u.port, path, body, headers)
            else:
                r = self._fetch(scheme, u.hostname, u.port, path, body, headers)
            data = r.read()
        except ssl.SSLCertVerificationError as e:
            self.send_error(502, f"upstream certificate rejected: {e.verify_message}")
            return
        except (OSError, http.client.HTTPException) as e:
            self.send_error(502, f"upstream failed: {e}")
            return
        self.send_response(r.status, r.reason)
        for k, v in r.getheaders():
            if k.lower() not in HOP and k.lower() != "content-length":
                self.send_header(k, v)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)
        sys.stderr.write(f"{self.client_address[0]} {self.command} {self.path} -> {scheme} {r.status} "
                         f"{len(data)} B {1000 * (time.monotonic() - t0):.0f} ms\n")

    do_GET = do_HEAD = do_POST = _handle

    def log_message(self, fmt, *args):     # one line per request comes from _handle
        pass


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--listen", required=True, help="HOST:PORT, a LAN address (never 0.0.0.0 on a routed host)")
    ap.add_argument("--cafile", help="extra CA to trust upstream (tests)")
    ap.add_argument("--no-upgrade", action="store_true", help="fetch http:// URLs as plain HTTP")
    a = ap.parse_args()
    Proxy.ctx = ssl.create_default_context(cafile=None)
    if a.cafile:
        Proxy.ctx.load_verify_locations(a.cafile)
    Proxy.upgrade = not a.no_upgrade
    host, port = a.listen.rsplit(":", 1)
    srv = http.server.ThreadingHTTPServer((host, int(port)), Proxy)
    srv.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sys.stderr.write(f"tt_tlsproxy: listening on {a.listen}\n")
    srv.serve_forever()


if __name__ == "__main__":
    main()
