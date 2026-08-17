# Assignment 3 - HTTP Client using TCP Sockets - Findings

## 1. What was built and what worked

Everything in the mandatory brief was built and tested, plus the optional
advanced (HTTPS/OpenSSL) part.

- `CN_Web_Server/index.html`, `notes.txt`, `sample.pdf`, `image.jpg` - the
  four files hosted by the Python server, exactly as the brief describes.
  - `sample.pdf` is a hand-built (but fully spec-valid) minimal 1-page PDF
    with real content text on the page (no PDF tools were available in
    WSL, so it is constructed directly with the correct object table /
    xref offsets computed by a small script). Confirmed valid with
    `file` -> `PDF document, version 1.4, 1 page(s)`.
  - `image.jpg` is a hand-built (from scratch, standard library only)
    64x64 baseline JFIF colour-bar test-pattern JPEG - real forward DCT,
    standard quantization tables, standard Huffman tables, proper marker
    structure. No `convert`/ImageMagick was available at the time it was
    needed (apt was locked by a parallel agent installing other
    packages), so a real encoder was written instead of faking bytes.
    Verified two independent ways: `file` reports
    `JPEG image data, JFIF standard 1.01, ... baseline, precision 8,
    64x64, components 3`, and a separate from-scratch Huffman *decoder*
    script (sharing no code with the encoder) fully decoded all 192
    blocks (64 blocks x 3 components) cleanly down to the EOI marker.
- `src/http_client.c` - the mandatory C TCP/HTTP client (parts a + b).
  Accepts any `http://host[:port]/path` URL, connects, sends a GET with
  `Host` + `Connection: close`, reads the raw response, splits
  headers/body on the blank line, saves the body as `downloaded_<name>`,
  and prints connection time, response time (time to first byte),
  download time (full transfer), byte count and throughput.
- `src/https_client.c` - the OPTIONAL advanced program. TCP socket +
  OpenSSL 3.5.5, TLS handshake, certificate verification (chain +
  hostname via `SSL_set1_host`), encrypted GET/response, and prints TCP
  connection time, SSL handshake time, round-trip time (TTFB) and data
  transfer rate. This was attempted and works - see section 4.

Test results, all real, all captured to `logs/`:
- Part (a), real external HTTP: `http://info.cern.ch/` gave 200 OK, body
  saved and byte-for-byte identical to the 646-byte `Content-Length` the
  server advertised. Also tried `http://example.com/`, which also
  returned 200 OK but uses `Transfer-Encoding: chunked` (via Cloudflare)
  - see the gotcha in section 5, that is a client limitation, not a bug
  in what is being measured. `http://neverssl.com/` returned 403
  Forbidden (the site appears to block the cloud/WSL outbound IP) but
  still proved the client mechanics (Content-Length parsing, body
  save, timing) work correctly on a real non-local server.
- Part (b), Python `http.server` on port 8000: all four files
  (`index.html`, `notes.txt`, `sample.pdf`, `image.jpg`) downloaded
  successfully and are **byte-for-byte identical** to the originals
  (`cmp`/`diff` confirmed, see section 2 for exact numbers).
- 404 case: requesting a file that does not exist on the server
  (`/does_not_exist.html`) gets back `HTTP/1.0 404 File not found` with
  Python standard small HTML error body. The client has no special
  handling for non-200 codes - it just saves whatever body came back
  (here, the 404 error page) under `downloaded_does_not_exist.html`.
  That is acceptable per the brief (404 handling is not required) but
  worth knowing about when demoing live.
- Advanced/optional HTTPS client: tested against both `example.com` and
  `www.google.com` on port 443. TLS 1.3 handshake succeeded in both
  cases, certificate verification passed (`OK (trusted chain + hostname
  match)`), and the encrypted GET/response worked end to end.

## 2. Measured performance numbers

### Part (b) - Python http.server on localhost:8000

| File | Status | Body size | Connection time | Response time (TTFB) | Download time | Throughput |
|---|---|---|---|---|---|---|
| index.html | 200 OK | 168 B | 0.137 ms | 4.317 ms | 6.220 ms | 27009.71 B/s (26.38 KB/s) |
| notes.txt | 200 OK | 1022 B | 0.255 ms | 2.442 ms | 3.559 ms | 287142.45 B/s (280.41 KB/s) |
| sample.pdf | 200 OK | 642 B | 0.276 ms | 2.177 ms | 3.169 ms | 202605.28 B/s (197.86 KB/s) |
| image.jpg | 200 OK | 931 B | 0.509 ms | 1.930 ms | 3.066 ms | 303620.98 B/s (296.50 KB/s) |
| does_not_exist.html | 404 File not found | 460 B | 0.126 ms | 1.512 ms | 1.614 ms | 284988.36 B/s (278.31 KB/s) |

All four real files verified byte-for-byte identical to the originals
in `CN_Web_Server/` via `cmp`/`diff` after download.

Numbers are small (localhost, sub-1KB files) so they are dominated by
syscall/loopback overhead rather than anything meaningful about network
bandwidth - that is expected and fine for this assignment; the point is
that the instrumentation works and produces sane, consistent numbers.

### Part (a) - real external HTTP server

| URL | Status | Body size | Connection time | Response time (TTFB) | Download time | Throughput |
|---|---|---|---|---|---|---|
| http://info.cern.ch/ | 200 OK | 646 B (matches Content-Length exactly) | 184.566 ms | 186.698 ms | 186.714 ms | 3459.85 B/s (3.38 KB/s) |
| http://example.com/ | 200 OK | 571 B (chunked, see gotcha) | 5.434 ms | 10.748 ms | 11.689 ms | 48849.68 B/s (47.70 KB/s) |

### Optional advanced program - HTTPS/OpenSSL client

| Host | TLS version / cipher | Cert verification | TCP conn time | SSL handshake time | RTT (TTFB) | Data received | Transfer rate |
|---|---|---|---|---|---|---|---|
| example.com | TLSv1.3 / TLS_AES_256_GCM_SHA384 | OK (trusted chain + hostname match) | 19.884 ms | 22.872 ms | 84.610 ms | 868 B | 10257.27 B/s (10.02 KB/s) |
| www.google.com | TLSv1.3 / TLS_AES_256_GCM_SHA384 | OK (trusted chain + hostname match) | 3.400 ms | 35.935 ms | 84.841 ms | 90011 B | 948879.30 B/s (926.64 KB/s) |

## 3. Exact commands to reproduce live (for screenshots)

Working directory for all of this: `C:\projects\PCN-assign\as3` on
Windows, driven through WSL Ubuntu.

Step 1 - compile the client(s):
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && gcc -Wall -Wextra -O2 -o http_client http_client.c"
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && gcc -Wall -Wextra -O2 -o https_client https_client.c -lssl -lcrypto"
```
(`https_client` needs `libssl-dev` + `pkg-config` installed in the WSL
Ubuntu instance - already installed by this run:
`sudo apt-get install -y libssl-dev pkg-config`.)

Step 2 - start the Python web server (background, in its own
terminal/window for the live screenshot so its "Serving HTTP..." banner
is visible):
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/CN_Web_Server && python3 -m http.server 8000"
```
Leave this running in its own window. It will print
`Serving HTTP on 0.0.0.0 port 8000 ...` and then a log line per request.

Step 3 - run the client for each file (part b), from a second
terminal. Each invocation prompts `Enter URL:` - type/pipe exactly one
of these URLs:
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && ./http_client"
Enter URL: http://localhost:8000/index.html

Enter URL: http://localhost:8000/notes.txt

Enter URL: http://localhost:8000/sample.pdf

Enter URL: http://localhost:8000/image.jpg
```
Downloaded files land in the current directory
(`as3/src/downloaded_index.html`, `downloaded_notes.txt`,
`downloaded_sample.pdf`, `downloaded_image.jpg`) - delete them after the
screenshot if you do not want them committed.

To pipe the URL non-interactively instead of typing it by hand:
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && echo http://localhost:8000/sample.pdf | ./http_client"
```

Step 4 - part (a), a real external URL (no server needed):
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && echo http://info.cern.ch/ | ./http_client"
```

Step 5 - 404 demo (server from step 2 must still be running):
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && echo http://localhost:8000/does_not_exist.html | ./http_client"
```

Step 6 - optional advanced HTTPS client (no local server needed, goes
straight to the real internet):
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && echo example.com | ./https_client"
```
or type `www.google.com` at the `Enter HTTPS host` prompt instead.

Step 7 - cleanup after the screenshots (do not commit binaries or
downloaded test files):
```
wsl -d Ubuntu -- bash -c "cd /mnt/c/projects/PCN-assign/as3/src && rm -f http_client https_client downloaded_*"
```
Also Ctrl+C the `python3 -m http.server` window from step 2.

## 4. Optional OpenSSL/HTTPS advanced part - status

Attempted, and it works. `libssl-dev` and `pkg-config` were not
initially installed in the WSL Ubuntu image; a parallel agent (running
the Assignment 2 lab in the same WSL instance at the same time) was
mid-`apt-get install` and held the apt lock for a while, so the first
install attempt was deferred. Once that other process finished and the
lock cleared, `apt-get install -y libssl-dev pkg-config` succeeded
(OpenSSL 3.5.5), and `src/https_client.c` was written, compiled, and
tested against both `example.com` and `www.google.com` (see section 2
for the numbers). It performs a real TLS 1.3 handshake, verifies the
certificate chain and hostname (`SSL_set1_host`), sends the GET over
the encrypted channel, and reports all four required metrics (TCP
connection time, SSL handshake time, round trip time, data transfer
rate). Nothing was skipped here.

## 5. Gotchas hit and how they were fixed

- No ImageMagick/wkhtmltopdf/enscript/ps2pdf/PIL available, and apt
  was locked for a long stretch by a parallel agent's `apt-get install
  wireshark epiphany-browser` running in the same shared WSL Ubuntu
  distro. Rather than block waiting on that lock, `sample.pdf` was
  built by hand (a small Python script writes the PDF objects and
  computes exact xref byte offsets) and `image.jpg` was built with a
  from-scratch pure-Python baseline JPEG encoder (real DCT, standard
  JPEG quantization/Huffman tables). Both were independently verified
  valid (see section 1) rather than just assumed correct.
- `memmem()` needs `_GNU_SOURCE` defined before the includes in both
  C files, otherwise it is not declared under strict POSIX headers on
  this gcc/glibc combination - added `#define _GNU_SOURCE` as the very
  first line of both `.c` files.
- `http://example.com/` uses chunked transfer-encoding (served via
  Cloudflare), which this client does not decode (the assignment brief
  only models plain `Content-Length` responses, matching Python's
  `http.server`, which is what part (b) actually needs). The client
  still connects, sends the request, and receives the response
  correctly - the raw chunk-size markers just end up inside the saved
  body file for that specific case. `http://info.cern.ch/` was used
  instead for a clean part (a) demo since it replies with a normal
  `Content-Length` and the saved file matched exactly. If a clean
  chunked demo is not needed, no code change is required; if it becomes
  necessary, dechunking is a small addition (read chunk-size lines,
  strip them, stop at the `0\r\n\r\n` terminator).
- `http://neverssl.com/` returned 403 Forbidden to the WSL
  environment outbound IP (their site appears to filter certain
  traffic/IP ranges) - not a client bug, just a bad external test
  target; `info.cern.ch` was used instead.
- Self-inflicted process kill while cleaning up: `pkill -f
  "http.server 8000"` matched not just the actual Python server process
  but also the wrapping `bash -c "..."` invocation itself (because
  that invocation own command-line text contained the same search
  string), so it killed its own shell mid-script before the following
  `rm -rf` cleanup line ran. Fixed by re-running the cleanup as its own
  isolated command afterward. Worth remembering for next time: avoid
  `pkill -f` patterns that appear verbatim inside the invoking command
  line.
- A first attempt to verify downloaded files against the originals with
  a shell variable holding the original directory path, run inside a
  `wsl bash -c "..."` string, silently expanded that variable in the
  outer Windows/Git-Bash shell (not inside WSL), so it came out empty
  and every comparison falsely reported "DIFFERS". Fixed by hardcoding
  full paths instead of using shell variables across the Windows to
  WSL command boundary - re-ran and got genuine "IDENTICAL"/byte-for-
  byte matches for all four files.
