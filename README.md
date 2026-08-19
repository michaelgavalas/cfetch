# cfetch

![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)
![POSIX](https://img.shields.io/badge/POSIX-1003.1--2008-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey)
![Build](https://img.shields.io/badge/build-make-brightgreen)
![License](https://img.shields.io/badge/license-MIT-yellow)
![Dependencies](https://img.shields.io/badge/dependencies-0-success)

A tiny HTTP client written in C with nothing but POSIX sockets. No libcurl, no OpenSSL, no framework, no 400MB of node_modules. Just `getaddrinfo`, a file descriptor, and a string that ends in `\r\n\r\n`.

It resolves a hostname, opens a TCP connection, writes a raw HTTP/1.1 request by hand, reads the response back in 4KB chunks, throws away the headers, and dumps the body into `output/output.html`.

That's it. That's the whole program.

## Why?

I started this project with libcurl. Two commits later I deleted libcurl, because it turns out "make an HTTP request" is one of those things you use every single day and understand roughly zero percent of.

So I went down a level. Then another one. Now I know what a socket descriptor actually is, why `AF_INET` and `SOCK_STREAM` are separate arguments, and what happens if you forget to `freeaddrinfo()`. Worth it.

If you want a real HTTP client, use curl. If you want to see what curl is doing under all that abstraction, keep reading.

## What it actually does

```
neverssl.com  ->  getaddrinfo()  ->  socket()  ->  connect()
                                                      |
                             send() raw GET request <-+
                                                      |
                        recv() in a loop until EOF  <-+
                                                      |
                    split on \r\n\r\n, write body  ->  output/output.html
```

Every step prints what it's doing, so running it feels less like a black box and more like watching a network stack do its job.

## Build

```bash
make
```

That's the entire toolchain. One compiler, one Makefile, done.

The build runs with `-Wall -Wextra -Werror` and `-D_FORTIFY_SOURCE=2 -O2`, so every warning is a hard error. `CC` and `CFLAGS` are overridable if you want to swap in clang or take the training wheels off:

```bash
make CC=clang
```

Clean up after yourself:

```bash
make clean
```

## Run

```bash
./app
```

The `output/` folder is created for you if it isn't there. Open `output/output.html` in a browser afterwards and enjoy the least impressive webpage ever fetched with this much effort.

## Why neverssl.com?

Because this program speaks plain HTTP on port 80 and absolutely nothing else. Point it at a modern site and you'll get a `301` redirect to `https://` that it has no idea what to do with. `neverssl.com` exists specifically to serve unencrypted content forever, which makes it the perfect punching bag for a project like this.

Adding TLS means adding OpenSSL, and adding OpenSSL means this stops being a pure "raw sockets" project. That tradeoff is phase 2 of the roadmap below, and I've made my peace with it.

## Design notes

A few things in here I want to call out, mostly because they were the parts that took me the longest to get right.

**Header and body splitting.** HTTP separates headers from the body with a `\r\n\r\n` sequence. The receive loop tracks a `headers_passed` flag: before the split is found it searches each chunk with `strstr`, and once it finds it, it does pointer arithmetic to figure out exactly how many bytes belong to the body and writes only those. After that the loop flips into pure body mode and every byte goes straight to disk.

**`Connection: close`.** This is the cheat code that makes the whole thing work. Without it the server holds the connection open with keep-alive, `recv()` blocks forever, and the program just sits there looking thoughtful. Asking the server to hang up means `recv()` returns 0 and the loop exits on its own, no `Content-Length` parsing required.

**Cleanup on every error path.** Every early return closes its socket and frees its `addrinfo` list first. Yes, the OS reclaims all of it when the process exits. No, that's not an excuse.

**Truncation check on `snprintf`.** The return value is checked against the buffer size, because `snprintf` will happily tell you it *wanted* to write 900 bytes into your 512 byte buffer and then send a malformed request on your behalf.

## Known limitations

Being honest about the sharp edges, since this is a learning project and not a library:

- **The URL is hardcoded.** Host and port live at the top of `main()`. Command line arguments are the obvious next step.
- **HTTP only, no TLS.** See above.
- **IPv4 only.** `hints.ai_family` is pinned to `AF_INET`. Setting it to `AF_UNSPEC` and walking the `addrinfo` list is the correct fix.
- **It only tries the first address.** `getaddrinfo` hands back a linked list of candidates for a reason, and this uses exactly one of them.
- **`strstr` on the header split is fragile.** If a response ever managed to straddle `\r\n\r\n` across two 4KB reads, the split would be missed. Unlikely with real headers, still wrong.
- **Redirects are ignored.** A `301` is written to `output/output.html` as-is, which is technically the body, just not the one you wanted.
- **Output filename is fixed.** Everything lands in `output/output.html`, every time.
- **Strictly single threaded and blocking.** One socket, one 4KB read at a time, and the program does nothing at all while it waits for packets. Fine for a small HTML page, painful for anything large.
- **Nothing times out, ever.** No bound on `connect()` and no bound on `recv()`. A server that accepts the connection and then says nothing will hang this program until you get bored and hit Ctrl+C.
- **`write()` results are added up but never inspected.** The return value feeds a running total that gets printed at the end, which keeps the compiler happy, but a `-1` on error or a short write on a full disk would sail straight through and quietly poison the count.
- **Chunked responses would be garbage.** An HTTP/1.1 server can use `Transfer-Encoding: chunked` whenever it feels like it, without asking. This client would faithfully write the hex length prefixes into the output file and call it a day.

## Roadmap

The goal is to drag this from "fun weekend" to "thing I'd actually install," in roughly this order. Ordering matters more than it looks: half of these are blocked on the parser refactor, so that goes first.

### Phase 1: stop lying to the user

The features that separate a demo from a client you can trust with a real URL.

- [ ] **Rewrite the receive loop as an incremental parser.** This is the unglamorous keystone. Instead of running `strstr` on each isolated 4KB chunk, accumulate into a growable buffer and run a small state machine over it: status line, then headers, then body. Doing this kills the chunk-boundary bug and unlocks chunked encoding, keep-alive, redirects, and resume all at once. Every item below is easier after it exists, which is exactly why it should not be done last.
- [ ] **Timeouts on everything.** `SO_RCVTIMEO` and `SO_SNDTIMEO` for reads and writes, plus a non-blocking `connect()` supervised by `poll()` so a dead host fails in seconds instead of never. Also a `--max-time` for the whole operation.
- [ ] **Chunked transfer encoding.** Not optional, not a nice-to-have. Servers send it uninvited.
- [ ] **Respect `Content-Length`** and know when a response is genuinely finished, rather than relying on the server hanging up.
- [ ] **Accept a URL from `argv`.** A real parser: scheme, optional userinfo, host, optional port, path, query, fragment. Percent-encoding handled properly, IPv6 literals in brackets handled properly.
- [ ] **`AF_UNSPEC` with fallback.** Walk the whole `addrinfo` list and try each candidate instead of betting everything on the first one. IPv6 comes free with this.
- [ ] **Follow redirects**, with a hop limit.
- [ ] **`gzip` and `deflate` support.** Send `Accept-Encoding`, then actually decompress what comes back. Most servers will hand you a fraction of the bytes if you just ask.

### Phase 2: HTTPS

- [ ] **Wrap the socket in OpenSSL.** The big one, and the point where the "zero dependencies" badge quietly retires.
- [ ] **Verify certificates correctly, which is the part people get wrong.** `SSL_CTX_set_default_verify_paths()` and `SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL)` get you a validated chain, but a valid chain says nothing about *who* you're talking to. Hostname verification needs `X509_VERIFY_PARAM_set1_host()`, and SNI needs `SSL_set_tlsext_host_name()` so the server knows which cert to send in the first place. Miss those last two and you have built an encrypted, authenticated, thoroughly professional connection to whoever is on the wire. This gets a test.

### Phase 3: make it fast

Right now it's one socket, one thread, one 4KB `recv()` at a time, so the CPU spends nearly its whole life waiting on packets. Two different fixes for two different problems, and I want to build both.

- [ ] **pthreads with ranged requests.** Send `Range: bytes=0-999999` per thread, have each write its slice at the right offset with `pwrite()`, then join. This is roughly how download managers get their numbers. Requires the server to advertise `Accept-Ranges: bytes`, so it needs a graceful fall back to the plain serial path when it doesn't.
- [ ] **Non-blocking I/O with `epoll` or `poll`.** One thread supervising many sockets, instead of many threads each blocked on one. This is the right answer for fetching lots of URLs at once, which is a different problem from fetching one big file fast.
- [ ] **Connection reuse.** Drop `Connection: close`, pool keep-alive sockets per host. Depends on `Content-Length` and chunked being handled, since without them there's no way to know where one response stops and the next begins.
- [ ] **Resume with `-C -`.** Shares all its plumbing with the ranged-download work above.
- [ ] **Benchmark honestly against `curl`** and publish the numbers even when they're embarrassing.

### Phase 4: turn it into a library with a CLI on top

- [ ] **Split the monolith.** `url.c`, `http.c`, `tls.c`, `buffer.c` as a small `libcfetch`, with `src/main.c` reduced to argument parsing and output. Right now the entire program lives in `main()`, which is fine for 200 lines and a disaster at 2000.
- [ ] **A write-callback interface**, in the spirit of curl's `CURLOPT_WRITEFUNCTION`, so callers can stream a response wherever they want instead of into a file descriptor I picked for them.
- [ ] **Real error handling.** An error enum and a `cfetch_strerror()`, rather than returning `-1` from seven different places and hoping the message on stderr was enough.

### Phase 5: prove it works

This is the part that actually separates engineering from hobby code.

- [ ] **Tests against a local server** spun up by the test harness itself. Never against `neverssl.com`. Network tests in CI are flaky, and hammering someone else's server on every push is rude.
- [ ] **Fuzz the URL and header parsers** with libFuzzer or AFL++. For C code that parses untrusted input off the network, this is the single highest-value item on this entire page, and hardly anyone bothers at this scale.
- [ ] **ASan, UBSan, and valgrind** wired into CI. Memory bugs in a network client are not a theoretical concern.
- [x] **`-Wall -Wextra -Werror`** plus `-D_FORTIFY_SOURCE=2` in the Makefile. Builds clean, which was less painful than expected.
- [ ] **GitHub Actions** across gcc and clang, on Linux and macOS.
- [x] A `LICENSE` file, so the badge at the top isn't writing checks the repo can't cash.
- [ ] An `install` target and a man page, so it can be installed like software instead of copied like a snippet.

### Phase 6: the polish that makes it feel real

- [ ] **`getopt_long` and a proper flag surface:** `-o`, `-O`, `-L`, `-H`, `-X`, `-d`, `-s`, `-v`, `--max-time`.
- [ ] **A progress bar** with transfer rate and ETA. Nothing else on this list will make it *look* finished as quickly.
- [ ] **Meaningful exit codes**, so it's usable inside a shell script.
- [ ] **Clean stdout and stderr separation**, so the body can be piped somewhere while the diagnostics stay readable.

## Security notes

Mostly forward-looking, since a client that only speaks plaintext HTTP to one hardcoded host has a refreshingly small attack surface. Once phases 1 and 2 land, that changes, and these stop being optional:

- **Cap the redirect count.** Otherwise a server can bounce you around a loop indefinitely.
- **Refuse scheme downgrades.** Following an `https://` URL to an `http://` redirect silently drops the user out of TLS.
- **Strip `Authorization` headers when a redirect crosses to a different host.** curl has had CVEs here. So has almost everyone else.
- **Cap the response size.** A hostile or broken server should not be able to fill the disk just because nobody bounded the write loop.
- **Never build an output path from server-controlled data** without sanitizing it. A `Content-Disposition` filename containing `../` is a classic, and it's a classic because it keeps working.

## Project layout

Today:

```
cfetch/
├── src/
│   └── main.c    the entire program, ~200 lines
├── build/        object files and depfiles, gitignored
├── output/       created at runtime, gitignored
├── Makefile      warnings as errors, objects out of the source tree
├── LICENSE
└── .gitignore
```

Where phase 4 is headed:

```
cfetch/
├── src/
│   ├── main.c      CLI: argv, flags, output
│   ├── url.c       scheme/host/port/path parsing
│   ├── http.c      request building, incremental response parser
│   ├── tls.c       OpenSSL wrapper, cert and hostname verification
│   └── buffer.c    growable byte buffer
├── include/
├── tests/
└── Makefile
```

## License

MIT, see [LICENSE](LICENSE). Do whatever you want with it.

---

Built while learning systems programming. If you spot something wrong, you're probably right, open an issue.
