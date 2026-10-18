"""Serve a directory over HTTP or HTTPS (cert.pem/key.pem in it) for the EonNav host tests.

HTTP/1.0 on purpose: the server closes the connection after each body, which is how the
TLS end-of-stream handling gets exercised.
"""
import http.server
import os
import ssl
import sys
from functools import partial

mode, port, root = sys.argv[1], int(sys.argv[2]), sys.argv[3]
handler = partial(http.server.SimpleHTTPRequestHandler, directory=root)
httpd = http.server.HTTPServer(("127.0.0.1", port), handler)
if mode == "https":
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(root, "cert.pem"), os.path.join(root, "key.pem"))
    httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
httpd.serve_forever()
