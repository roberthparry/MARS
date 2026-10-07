"""Minimal offline HTTP/2 peer for unary gRPC framing tests, not a general server.

It consumes the client preface and DATA, acknowledges SETTINGS/PING, and emits
literal HPACK response headers. No third-party Python modules are needed.
"""
import socket
import ssl
import struct
import sys

def exact(conn, count):
    result = bytearray()
    while len(result) < count:
        chunk = conn.recv(count - len(result))
        if not chunk:
            raise EOFError()
        result.extend(chunk)
    return bytes(result)

def frame(conn, kind, flags, stream, payload=b""):
    conn.sendall(len(payload).to_bytes(3, "big") + bytes([kind, flags]) +
                 struct.pack("!I", stream) + payload)

def literal(name, value):
    name, value = name.encode("ascii"), value.encode("ascii")
    assert len(name) < 127 and len(value) < 127
    return b"\0" + bytes([len(name)]) + name + bytes([len(value)]) + value

def serve(conn):
    conn.settimeout(5)
    if exact(conn, 24) != b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n":
        return
    frame(conn, 4, 0, 0)
    body = bytearray()
    while True:
        head = exact(conn, 9)
        size, kind, flags = int.from_bytes(head[:3], "big"), head[3], head[4]
        stream = int.from_bytes(head[5:], "big") & 0x7fffffff
        if size > 65536:
            return
        data = exact(conn, size)
        if kind == 4 and not flags & 1:
            frame(conn, 4, 1, 0)
        elif kind == 6 and not flags & 1:
            frame(conn, 6, 1, 0, data)
        elif kind == 0:
            if flags & 8:
                return
            body.extend(data)
            if len(body) > 65536:
                return
        if stream and kind in (0, 1) and flags & 1:
            break
    case = body[6] if len(body) == 7 and body[5] == 8 else 0
    media = "text/plain" if case == 6 else "application/grpc+proto"
    # Advertise single-stream lifetime before completion, preventing connection reuse.
    frame(conn, 7, 0, 0, struct.pack("!II", stream, 0))
    frame(conn, 1, 4, stream, b"\x88" + literal("content-type", media))
    status = "0"
    output = bytes(body)
    message = ""
    if case == 1:
        status, output, message = "7", b"", "Permission%20denied"
    elif case == 2:
        output = output[:-1]
    elif case == 3:
        output += output
    elif case == 4:
        output = b"\x01" + output[1:]
    elif case == 5:
        status = "17"
    if output:
        frame(conn, 0, 0, stream, output)
    trailers = b"" if case == 7 else literal("grpc-status", status)
    if message:
        trailers += literal("grpc-message", message)
    frame(conn, 1, 5, stream, trailers)
    conn.shutdown(socket.SHUT_WR)
    # Drain pending SETTINGS acknowledgements so close cannot reset unread TCP data.
    conn.settimeout(0.2)
    while conn.recv(4096):
        pass

listener = socket.socket(fileno=int(sys.argv[1]))
context = None
if len(sys.argv) == 4:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(sys.argv[2], sys.argv[3])
    context.set_alpn_protocols(["h2"])
while True:
    connection, _ = listener.accept()
    try:
        if context:
            connection = context.wrap_socket(connection, server_side=True)
        with connection:
            serve(connection)
    except (OSError, EOFError):
        connection.close()
