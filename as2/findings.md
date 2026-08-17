# Assignment 2 - build/test notes

Everything below was built and tested headlessly in WSL Ubuntu (`wsl -d Ubuntu`). All 5 required
test cases passed. Logs are in `as2/logs/`. Binaries were deleted after testing - only `.c` files
and `students.csv` are committed.

## 1c. TCP vs UDP comparison (based on what was actually observed)

**Connection**
- TCP: explicit handshake required. `tcp_server` does `socket -> bind -> listen -> accept`, and
  `tcp_client` has to `connect()` before it can send anything. You can literally see this in the
  server log (`06_tcp_server_side_log.txt`) - each client shows up as a distinct
  "Client connected: 127.0.0.1:PORT" line the moment `accept()` returns.
- UDP: no connection at all. `udp_server` just `bind()`s and calls `recvfrom()` in a loop; the
  client fires a single `sendto()` with no setup step. There is nothing analogous to "Client
  connected" in the UDP server log - it only logs a query the instant a datagram lands.

**Reliability**
- TCP: guaranteed delivery (retransmission handled by the kernel's TCP stack) - every request we
  sent got an answer, and there is no code in `tcp_server.c`/`tcp_client.c` that handles a lost
  packet because the transport already guarantees it arrives.
- UDP: no delivery guarantee whatsoever. `udp_client.c` calls `recvfrom()` once and just prints
  "No response from server." if nothing comes back - there is no retry/timeout logic, so on a lossy
  network a query could silently vanish. On localhost loopback (what we tested on) this never
  actually happened, but the client code path exists because it has to be assumed possible.

**Ordering**
- TCP: byte stream is ordered by the protocol - irrelevant here anyway since each exchange is a
  single request/response over a fresh connection, but in general TCP guarantees the client reads
  bytes in the order the server wrote them.
- UDP: datagrams can in principle arrive out of order or be duplicated. Does not show up in our
  single-request-single-reply test pattern, but it is a real property of the protocol - nothing in
  `udp_server.c` reorders or dedupes anything, because for one datagram in / one datagram out
  there is nothing to reorder.

**Error recovery**
- TCP: `connect()` fails loudly and immediately if the server is not listening (`perror("connect")`
  + exit) - this is exactly the "port already in use / server not up" gotcha noted below.
  Read/write errors on an established connection are also detectable via return values.
- UDP: `sendto()` "succeeds" even if nothing is listening on the other end (there is no connection
  to fail) - the client only finds out something is wrong indirectly, by `recvfrom()` never
  returning. There is no timeout implemented in `udp_client.c`, so a truly unreachable server would
  just hang the client forever. That is a real, observable weakness of UDP compared to TCP's
  immediate connect() failure.

**Communication**
- TCP: stream-oriented, connection-scoped. One thread per client (`pthread_create` +
  `pthread_detach` in `handle_client`) so concurrent clients are handled genuinely in parallel, not
  serialized - proven in `03_tcp_concurrent.txt` where two clients launched back-to-back both got
  correct, independent replies and the server log shows both `accept()`s going through close
  together.
- UDP: message-oriented, single call per exchange (`sendto`/`recvfrom`), stateless - the server
  never "remembers" a client between datagrams. Because it is just one thread in a loop, requests
  are handled one at a time; there is no per-client thread since there is no connection to hand off
  to one. For this assignment's tiny payload (roll number + name only) that is a completely
  reasonable tradeoff - the extra weight of TCP's handshake is not needed to reliably deliver ~30
  bytes.

Practical summary: TCP is heavier (handshake, per-client thread, connection state) but guarantees
the client gets a correct, in-order, complete profile every time. UDP is lighter and faster for the
one-shot "just confirm this roll number is valid" case in 1(b), at the cost of no delivery
guarantee - acceptable here because it is a verify-only lookup, not a full profile fetch.

## Exact reproduction commands (for live terminal + screenshot re-run)

Working directory for every command below: `/mnt/c/projects/PCN-assign/as2/src` (i.e. from
Windows: `C:\projects\PCN-assign\as2\src`, opened inside WSL Ubuntu). Binaries need to be rebuilt
first since they were deleted after testing:

```bash
cd /mnt/c/projects/PCN-assign/as2/src
gcc -Wall -o tcp_server tcp_server.c -lpthread
gcc -Wall -o tcp_client tcp_client.c
gcc -Wall -o udp_server udp_server.c
gcc -Wall -o udp_client udp_client.c
```

### Test 1 - TCP, existing roll number
Terminal A:
```bash
cd /mnt/c/projects/PCN-assign/as2/src
./tcp_server
```
(leave running - it prints `Loaded 10 student records from students.csv` then
`TCP server listening on port 6060...`)

Terminal B:
```bash
cd /mnt/c/projects/PCN-assign/as2/src
./tcp_client
```
At the `Enter Roll Number:` prompt, type: `3122247001062` and press Enter.
Expected: full record for Simiyon Vinscent Samuel L (Computer Science, Semester 5, CGPA 8.32).

### Test 2 - TCP, nonexistent roll number
Same server (Terminal A) still running. Terminal B:
```bash
cd /mnt/c/projects/PCN-assign/as2/src
./tcp_client
```
At the prompt, type: `9999999999999` and press Enter.
Expected: `Student Record Not Found`.

### Test 3 - TCP, two concurrent clients
Same server (Terminal A) still running. Open Terminal B and Terminal C and run these as close
together as possible (or background both from one terminal):
```bash
cd /mnt/c/projects/PCN-assign/as2/src && ./tcp_client
```
Terminal B types: `3122247001064` (expect Lakshmi Priya V, CGPA 9.01)
Terminal C types: `3122247001068` (expect Ramya Devi K, CGPA 8.45)
Watch Terminal A (the server) - it should print two "Client connected: 127.0.0.1:PORT" lines in
quick succession, one per client, proving both are being accepted and handled without one blocking
the other.

### Test 4 - UDP, existing roll number
Terminal A:
```bash
cd /mnt/c/projects/PCN-assign/as2/src
./udp_server
```
(prints `Loaded 10 student records from students.csv` then `UDP server listening on port 6061...`)

Terminal B:
```bash
cd /mnt/c/projects/PCN-assign/as2/src
./udp_client
```
At the prompt, type: `3122247001062` and press Enter.
Expected: only `Roll Number: 3122247001062` and `Name: Simiyon Vinscent Samuel L` - no department,
semester, or CGPA (that is intentional per the 1(b) spec).

### Test 5 - UDP, nonexistent roll number
Same server (Terminal A) still running. Terminal B:
```bash
cd /mnt/c/projects/PCN-assign/as2/src
./udp_client
```
At the prompt, type: `9999999999999` and press Enter.
Expected: `Student Record Not Found`.

### Cleanup after the live screenshot session
```bash
# Ctrl+C both server terminals, then:
cd /mnt/c/projects/PCN-assign/as2/src
rm -f tcp_server tcp_client udp_server udp_client
```

## Gotchas hit while building/testing

1. **Background job + `cd` does not apply to sibling jobs.** When testing concurrency headlessly
   we ran `cd /path && (cmd1) & (cmd2) & wait` in one `bash -c`. Because `&&` binds tighter than
   `&`, only `cmd1`'s subshell inherited the `cd` - `cmd2` started in the original (wrong)
   directory and failed with "No such file or directory". Fix: give each backgrounded subshell its
   own explicit `cd`, e.g. `(cd /path && cmd1) & (cd /path && cmd2) & wait`. Not a bug in the C
   code, purely a headless-testing shell-scripting trap - irrelevant when running two normal
   interactive terminals for the live screenshot re-run.
2. **`pkill -f <full path>` did not match.** The server was launched as `./tcp_server` (relative),
   so `pkill -f '/mnt/c/.../tcp_server'` found nothing even though the process was alive -
   `pkill -f` matches against the actual command line, which was the short relative form. Had to
   find the PID with `pgrep -fa tcp_server` and `kill -9 <pid>` directly. Worth knowing if the main
   session needs to stop a stray server between takes.
3. **CSV header row.** `students.csv` has a header line (`RollNumber,Name,Department,Semester,CGPA`)
   for readability/realism. Both servers explicitly skip the first line with a throwaway `fgets()`
   call before parsing data rows - if the CSV is ever edited by hand, keep the header line in place
   or that skip will eat the first real record.
4. **Ports.** Used TCP 6060 / UDP 6061 as instructed (avoiding as1's 5050) - no conflicts hit
   during testing, both bound cleanly on first try each run.
5. **Trailing newline from `fgets()`** on both the roll-number input and the CSV line-read needed
   explicit stripping (`strcspn(buf, "\r\n")` trick) - otherwise the roll number sent over the
   socket would have a trailing `\n` that never `strcmp`-matched the CSV's stripped roll number.
   Handled once in each client/server and it has been reliable since.

## Test results summary (all 5 passed)

| # | Case | Result |
|---|------|--------|
| 1 | TCP, roll 3122247001062 (exists) | Full record returned correctly - `01_tcp_found.txt` |
| 2 | TCP, roll 9999999999999 (does not exist) | `Student Record Not Found` - `02_tcp_notfound.txt` |
| 3 | TCP, 2 concurrent clients (3122247001064 + 3122247001068) | Both got correct, independent, non-serialized replies - `03_tcp_concurrent.txt` (+ server-side proof in `06_tcp_server_side_log.txt`) |
| 4 | UDP, roll 3122247001062 (exists) | Only Roll Number + Name returned, no other fields - `04_udp_found.txt` |
| 5 | UDP, roll 9999999999999 (does not exist) | `Student Record Not Found` - `05_udp_notfound.txt` |
