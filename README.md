# amalgame-net-curl

Thin libcurl binding for Amalgame. Brings the convenient `Http.Get` /
`Http.Post` / etc. API that lived in amc's bundled stdlib through
v0.8.30 — extracted as a separate package in v0.8.31 so amc itself
has zero dependency on libcurl.

## When to use this vs `amalgame-net-http`

| Use this package | Use `amalgame-net-http` |
|---|---|
| Calling REST/JSON APIs as a client | Writing an HTTP server |
| Need HTTPS out of the box | Pure-AM HTTP/1.1 parsing |
| Need redirect / cookie handling | Want zero external C deps |
| Trust libcurl over your own parser | Need the lowest-level access |

The two packages are **complementary**, not competing. A typical
Mosaic app uses both: `amalgame-net-http` for serving requests,
`amalgame-net-curl` for outbound API calls.

## Install

```bash
amc package add net-curl
```

You also need libcurl development headers:

| Platform | Command |
|---|---|
| Debian / Ubuntu | `sudo apt install libcurl4-openssl-dev` |
| Fedora / RHEL | `sudo dnf install libcurl-devel` |
| macOS (Homebrew) | `brew install curl` |
| Windows (MSYS2) | `pacman -S mingw-w64-x86_64-curl` |

## API

```amalgame
import Amalgame.Net.Curl

let resp = Http.Get("https://api.example.com/users")
Console.WriteLine("Status: " + String_FromInt(resp.Status))
Console.WriteLine("Body:   " + resp.Body)
if (!resp.Ok) {
    Console.WriteLine("Error: " + resp.Error)
}

// POST JSON
let r = Http.PostJson(
    "https://api.example.com/users",
    "{\"name\":\"Alice\"}")

// Custom headers
let headers = new Map<string, string>()
headers.Set("Authorization", "Bearer token123")
headers.Set("Accept", "application/json")
let auth = Http.GetWithHeaders("https://api.example.com/me", headers)

// Timeout (milliseconds)
let slow = Http.GetTimeout("https://slow.example.com/", 5000)
```

## Functions

| Function | Purpose |
|---|---|
| `Http.Get(url)` | Simple GET |
| `Http.GetWithHeaders(url, headers)` | GET with custom headers |
| `Http.GetTimeout(url, ms)` | GET with timeout in ms |
| `Http.Post(url, body)` | POST with raw body |
| `Http.PostJson(url, jsonBody)` | POST with `Content-Type: application/json` set |
| `Http.PostWithHeaders(url, body, headers)` | POST with custom headers |
| `Http.Put(url, body)` | PUT |
| `Http.Delete(url)` | DELETE |
| `Http.Patch(url, body)` | PATCH |

All return an `HttpResponse` with:
- `Status` (int) — HTTP status code, 0 on connect/curl error
- `Body` (string) — response body
- `Error` (string) — non-null when something went wrong
- `Ok` (bool) — true when `200 <= Status < 300`

## Features inherited from libcurl

- HTTPS (TLS via the system's OpenSSL / SChannel / SecureTransport)
- Redirect following (max 10 hops)
- HTTP/2 if libcurl was compiled with nghttp2
- Connection reuse / keep-alive
- Compression (gzip, deflate) auto-handled when peer advertises it
- Built-in TCP timeout + DNS resolution timeout

Skip TLS verification (debugging only) via env var:

```bash
AMALGAME_SSL_NOVERIFY=1 ./your-app
```

## License

Apache-2.0.
