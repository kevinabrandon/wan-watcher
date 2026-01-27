## TODOs

TODOs and other planned enhancements:
 - Improve the LAN bandwidth metric
   - Currently LAN bandwidth is simply the sum of the two WAN bandwidths.
   - Instead we should measure the Mbits transferred on the LAN interface according to pfSense in the same way we do the WANs
 - Improve the WAN state determination
   - Currently we make the UP/DEGRADED/DOWN determination by examining the dpinger stats and thresholding it.
   - Instead we should simply read the state as reported by the pfSense router, based on its configured gateway group rules.
 - Add API authentication for `POST /api/wans`
   - Require `X-WW-Token` header for this endpoint; return HTTP 403 if missing or invalid.
   - The ESP32 loads the shared secret from a local file on its filesystem (not committed to the repo).
   - The pfSense script loads the same secret from a root-only file on the router and includes it in each request.
   - Do not require authentication for read-only endpoints (Web UI + status APIs).
   - Define `X-WW-Token` as an OpenAPI `apiKey` (header) security scheme.
   - Allow setting the token via the RapiDoc "Authorize" dialog so the endpoint can be tested interactively.
 - Add photos of the installed system to the main README
