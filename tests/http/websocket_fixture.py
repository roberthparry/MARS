"""Offline RFC 6455 fixture with optional TLS and deliberate malformed replies."""
import base64
import hashlib
import socket
import ssl
import struct
import sys
import time

def exact(conn, count):
    value = bytearray()
    while len(value) < count:
        part = conn.recv(count - len(value))
        if not part:
            raise EOFError()
        value.extend(part)
    return bytes(value)

def send(conn, kind, data=b"", final=True):
    head = bytes([(128 if final else 0) | kind])
    size = len(data)
    head += bytes([size]) if size < 126 else b"\x7e" + struct.pack("!H", size) if size < 65536 else b"\x7f" + struct.pack("!Q", size)
    conn.sendall(head + data)

def receive(conn):
    head = exact(conn, 2)
    size = head[1] & 127
    if size == 126:
        size = struct.unpack("!H", exact(conn, 2))[0]
    elif size == 127:
        size = struct.unpack("!Q", exact(conn, 8))[0]
    if not head[1] & 128 or size > 8388608:
        raise ValueError("invalid client frame")
    mask = exact(conn, 4)
    data = exact(conn, size)
    return head[0] & 15, bytes(byte ^ mask[i % 4] for i, byte in enumerate(data))

def serve(conn):
    conn.settimeout(5)
    head = bytearray()
    while not head.endswith(b"\r\n\r\n"):
        head.extend(exact(conn, 1))
        if len(head) > 65536:
            return
    lines = head.decode("ascii").split("\r\n")
    path = lines[0].split(" ")[1]
    headers = dict((name.lower(), value.strip()) for name, value in
                   (line.split(":", 1) for line in lines[1:] if ":" in line))
    if path == "/reject":
        conn.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
        return
    key = headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    accept = base64.b64encode(hashlib.sha1(key.encode("ascii")).digest()).decode("ascii")
    extra = "Sec-WebSocket-Extensions: permessage-deflate\r\n" if path == "/extension" else ""
    conn.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  f"Sec-WebSocket-Accept: {accept}\r\n{extra}\r\n").encode("ascii"))
    if path == "/fragment":
        send(conn, 1, b"caf\xc3", False)
        send(conn, 9, b"ping")
        send(conn, 0, b"\xa9", True)
    elif path == "/invalid":
        send(conn, 1, b"\xff")
    elif path == "/large":
        send(conn, 2, b"x" * 16384)
    elif path == "/drop":
        return
    elif path == "/silent":
        time.sleep(0.6)
        return
    elif path == "/peer-close":
        send(conn, 8, b"\x03\xe8")
    while True:
        kind, body = receive(conn)
        if kind == 8:
            send(conn, 8, body)
            return
        if kind == 10:
            continue
        send(conn, kind, body)

listener = socket.socket(fileno=int(sys.argv[1]))
context = None
if len(sys.argv) == 4:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(sys.argv[2], sys.argv[3])
while True:
    connection, _ = listener.accept()
    try:
        if context:
            connection = context.wrap_socket(connection, server_side=True)
        with connection:
            serve(connection)
    except (OSError, EOFError, ValueError, KeyError):
        connection.close()
