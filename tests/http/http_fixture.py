"""Offline HTTP/HTTPS fixture; launched with a listening socket by test_http.c."""
import gzip
import http.server
import json
import socket
import ssl
import sys
import time

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError):
            # Cancellation and TLS-rejection tests deliberately close connections.
            pass

    def handle_request(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        path = self.path
        status = 200
        headers = []
        if path == "/get?message=MARS":
            body = b'{"args":{"message":"MARS"}}'
            headers.append(("Content-Type", "application/json"))
        elif path == "/post":
            text = body.decode("utf-8")
            value = json.loads(text) if self.headers.get("Content-Type") == "application/json" else None
            body = json.dumps({"data": text, "json": value}).encode("utf-8")
            headers.append(("Content-Type", "application/json"))
        elif path == "/json":
            body = b'{"answer":42}'
            headers.append(("Content-Type", "application/json"))
        elif path == "/xml":
            body = b"<answer>42</answer>"
            headers.append(("Content-Type", "application/xml"))
        elif path == "/binary":
            body = bytes([0, 255, 254, 65])
        elif path == "/headers":
            body = b"headers"
            headers.extend([("Set-Cookie", "one=1"), ("set-cookie", "two=2")])
        elif path == "/redirect":
            status, body = 302, b"not followed"
            headers.append(("Location", "/json"))
        elif path == "/error":
            status, body = 404, b'{"error":"missing"}'
        elif path == "/large":
            body = b"x" * 4096
        elif path == "/gzip":
            body = gzip.compress(b"x" * 4096)
            headers.append(("Content-Encoding", "gzip"))
        elif path == "/bigheader":
            headers.append(("X-Large", "x" * 4096))
        elif path == "/slow":
            time.sleep(0.3)
            body = b"late"
        elif path == "/truncated":
            self.send_response(200)
            self.send_header("Content-Length", "100")
            self.end_headers()
            self.wfile.write(b"short")
            self.close_connection = True
            return
        elif path == "/chunks":
            self.wfile.write(b"HTTP/1.1 103 Early Hints\r\nX-Early: discarded\r\n\r\n"
                             b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                             b"Trailer: X-End\r\n\r\n3\r\none\r\n3\r\ntwo\r\n"
                             b"0\r\nX-End: yes\r\n\r\n")
            return
        elif path == "/badheader":
            self.wfile.write(b"HTTP/1.1 200 OK\r\nX-Invalid: \xff\r\nContent-Length: 0\r\n\r\n")
            return
        elif path == "/folded":
            self.wfile.write(b"HTTP/1.1 200 OK\r\nX-Fold: one\r\n two\r\nContent-Length: 0\r\n\r\n")
            return
        elif path == "/switch":
            self.wfile.write(b"HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\n\r\n")
            return
        elif path == "/echo":
            headers.extend([("X-Method", self.command),
                            ("X-Type", self.headers.get("Content-Type", "")),
                            ("X-Custom", self.headers.get("X-Custom", "")),
                            ("X-Auth", self.headers.get("Authorization", ""))])
        else:
            status, body = 404, b"unknown"
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        for name, value in headers:
            self.send_header(name, value)
        self.end_headers()
        if self.command != "HEAD":
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass

    do_GET = do_POST = do_PUT = do_PATCH = do_DELETE = do_HEAD = do_OPTIONS = handle_request

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler, bind_and_activate=False)
server.socket.close()
server.socket = socket.socket(fileno=int(sys.argv[1]))
server.server_address = server.socket.getsockname()
server.daemon_threads = True
if len(sys.argv) == 4:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(sys.argv[2], sys.argv[3])
    server.socket = context.wrap_socket(server.socket, server_side=True)
server.serve_forever()
