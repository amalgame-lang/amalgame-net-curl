/*
 * amalgame-net-curl — Amalgame.Net.Curl
 * Copyright (c) 2026 Bastien MOUGET
 * Licensed under the Apache License, Version 2.0.
 * https://github.com/amalgame-lang/amalgame-net-curl
 *
 * Thin libcurl binding — Http.Get/Post/Put/Delete/Patch with
 * automatic redirect handling, HTTPS support, custom headers,
 * optional timeout.
 *
 * Extracted from amc's bundled runtime (Amalgame_Net.h) in
 * v0.8.31 so amc itself has zero dependency on libcurl.
 *
 * Install:
 *   Debian/Ubuntu : sudo apt install libcurl4-openssl-dev
 *   macOS         : brew install curl
 *   Fedora/RHEL   : sudo dnf install libcurl-devel
 *   Windows/MSYS2 : pacman -S mingw-w64-x86_64-curl
 *
 * Link: -lcurl (handled automatically by amc via `libs = ["curl"]`
 * in amalgame.toml).
 *
 * If libcurl is not installed, every function returns a stub
 * AmalgameHttpResponse with Status=0 and a descriptive Error.
 */

#ifndef AMALGAME_NET_CURL_H
#define AMALGAME_NET_CURL_H

#include "_runtime.h"
#include "Amalgame_Collections.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
   AmalgameHttpResponse
   ================================================================ */

typedef struct {
    i64         Status;
    code_string Body;
    code_string Error;
    code_bool   Ok;
} AmalgameHttpResponse;

static inline AmalgameHttpResponse* _amcurl_resp_new(
        long status, code_string body, code_string err) {
    AmalgameHttpResponse* r =
        (AmalgameHttpResponse*) GC_MALLOC(sizeof(AmalgameHttpResponse));
    r->Status = (i64) status;
    r->Body   = body ? body : "";
    r->Error  = err  ? err  : NULL;
    r->Ok     = (status >= 200 && status < 300);
    return r;
}

/* ================================================================
   libcurl detection + implementation
   ================================================================ */

#ifdef __has_include
#  if __has_include(<curl/curl.h>)
#    define AMALGAME_HAS_CURL 1
#    include <curl/curl.h>
#  endif
#endif

#ifdef AMALGAME_HAS_CURL

typedef struct {
    char*  data;
    size_t size;
} _AmCurlBuffer;

static size_t _amcurl_write_cb(void* ptr, size_t size,
                                size_t nmemb, void* userdata) {
    _AmCurlBuffer* buf = (_AmCurlBuffer*) userdata;
    size_t total = size * nmemb;
    char* nd = (char*) GC_MALLOC(buf->size + total + 1);
    if (buf->size > 0) memcpy(nd, buf->data, buf->size);
    memcpy(nd + buf->size, ptr, total);
    nd[buf->size + total] = '\0';
    buf->data = nd;
    buf->size += total;
    return total;
}

static AmalgameHttpResponse* _amcurl_perform(
        const char*  method,
        code_string  url,
        code_string  body,
        AmalgameMap* headers,
        i64          timeoutMs) {

    CURL* curl = curl_easy_init();
    if (!curl) return _amcurl_resp_new(0, NULL, "curl_easy_init failed");

    _AmCurlBuffer buf = { NULL, 0 };
    long statusCode  = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _amcurl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Amalgame-Net-Curl/0.1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                     timeoutMs > 0 ? (long)timeoutMs : 30000L);

    if (getenv("AMALGAME_SSL_NOVERIFY") != NULL) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    struct curl_slist* curlHeaders = NULL;
    if (headers) {
        AmalgameList* keys = AmalgameMap_keys(headers);
        for (int i = 0; i < keys->size; i++) {
            code_string k = (code_string) keys->data[i];
            code_string v = (code_string) AmalgameMap_get(headers, k);
            if (v) {
                size_t hlen = strlen(k) + strlen(v) + 3;
                char*  h    = (char*) GC_MALLOC(hlen);
                snprintf(h, hlen, "%s: %s", k, v);
                curlHeaders = curl_slist_append(curlHeaders, h);
            }
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);
    }

    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                         body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         body ? (long)strlen(body) : 0L);
    } else if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
        }
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    if (curlHeaders) curl_slist_free_all(curlHeaders);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return _amcurl_resp_new(0, NULL, (code_string)curl_easy_strerror(res));
    return _amcurl_resp_new(statusCode, buf.data, NULL);
}

static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Get(code_string url) {
    return _amcurl_perform("GET", url, NULL, NULL, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_GetWithHeaders(
        code_string url, AmalgameMap* headers) {
    return _amcurl_perform("GET", url, NULL, headers, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_GetTimeout(
        code_string url, i64 ms) {
    return _amcurl_perform("GET", url, NULL, NULL, ms);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Post(
        code_string url, code_string body) {
    return _amcurl_perform("POST", url, body, NULL, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_PostJson(
        code_string url, code_string json) {
    AmalgameMap* h = AmalgameMap_new();
    AmalgameMap_set(h, "Content-Type", (void*)"application/json");
    return _amcurl_perform("POST", url, json, h, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_PostWithHeaders(
        code_string url, code_string body, AmalgameMap* headers) {
    return _amcurl_perform("POST", url, body, headers, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Put(
        code_string url, code_string body) {
    return _amcurl_perform("PUT", url, body, NULL, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Delete(code_string url) {
    return _amcurl_perform("DELETE", url, NULL, NULL, 0);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Patch(
        code_string url, code_string body) {
    return _amcurl_perform("PATCH", url, body, NULL, 0);
}

#else /* no libcurl */

#define _AMCURL_STUB_ERR \
    "amalgame-net-curl: libcurl not found. Install libcurl4-openssl-dev " \
    "(Debian/Ubuntu), curl (Homebrew), libcurl-devel (Fedora/RHEL), or " \
    "mingw-w64-x86_64-curl (MSYS2)."

static inline AmalgameHttpResponse* _amcurl_no_curl(code_string url) {
    (void) url;
    return _amcurl_resp_new(0, NULL, _AMCURL_STUB_ERR);
}
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Get(code_string u)
    { return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_GetWithHeaders(
        code_string u, AmalgameMap* h) { (void)h; return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_GetTimeout(
        code_string u, i64 t) { (void)t; return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Post(
        code_string u, code_string b) { (void)b; return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_PostJson(
        code_string u, code_string b) { (void)b; return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_PostWithHeaders(
        code_string u, code_string b, AmalgameMap* h)
    { (void)b; (void)h; return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Put(
        code_string u, code_string b) { (void)b; return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Delete(code_string u)
    { return _amcurl_no_curl(u); }
static inline AmalgameHttpResponse* Amalgame_Net_Curl_Http_Patch(
        code_string u, code_string b) { (void)b; return _amcurl_no_curl(u); }

#endif /* AMALGAME_HAS_CURL */

#endif /* AMALGAME_NET_CURL_H */
