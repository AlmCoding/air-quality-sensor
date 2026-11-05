# Air Quality Sensor Firmware

## LittleFS file-list API

The file-list server connects to the configured Wi-Fi network for 10 minutes
after boot (temporarily enabled for button testing) or when the button connected
between GPIO4 and GND is pressed. Its IP address is printed in the serial log.
Open the address from a device on the same Wi-Fi network.

Request the contents of a directory with `GET /api/files?path=...`. The `path`
parameter is URL-encoded and defaults to `/` when omitted. For example, replace
`<device-ip>` with the address printed in the serial log to list the root
directory:

```sh
curl --get --data-urlencode "path=/" http://<device-ip>/api/files
```

To list a subdirectory, use its LittleFS path:

```sh
curl --get --data-urlencode "path=/logs" http://<device-ip>/api/files
```

The response contains the requested path and the immediate entries in that
directory. Each entry has a `name` and an `isDir` flag:

```json
{
  "path": "/",
  "files": [
    { "name": "/log_2026_45.bin", "isDir": false },
    { "name": "/logs", "isDir": true }
  ]
}
```

An invalid path or unavailable directory returns HTTP 400. Unknown endpoints
return HTTP 404.