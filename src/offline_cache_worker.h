#pragma once

const char OFFLINE_CACHE_WORKER[] PROGMEM = R"SW(
const CACHE_NAME = 'soccer-turret-network-assets-v1';
const ALLOWED_HOSTS = new Set([
  'cdn.jsdelivr.net',
  'storage.googleapis.com'
]);

self.addEventListener('install', event => {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', event => {
  const request = event.request;
  const url = new URL(request.url);
  if (request.method !== 'GET' || !ALLOWED_HOSTS.has(url.hostname) || request.headers.has('range')) return;

  event.respondWith((async () => {
    const cache = await caches.open(CACHE_NAME);
    const cached = await cache.match(request);
    if (cached) return cached;

    const response = await fetch(request);
    if (!response.ok || (response.type !== 'cors' && response.type !== 'basic')) return response;
    try {
      await cache.put(request, response.clone());
    } catch (error) {
      const client = event.clientId ? await self.clients.get(event.clientId) : null;
      client?.postMessage({
        type: 'offline-cache-write-failed',
        host: url.hostname,
        error: String(error?.message || error)
      });
    }
    return response;
  })());
});
)SW";
