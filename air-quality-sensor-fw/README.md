# Air Quality Sensor Firmware

## Flash partitions

The custom [partitions.csv](./partitions.csv) layout is for a 4 MB ESP32-C3:
it provides a 2 MB LittleFS partition and a single application partition for
firmware. The single application layout does not support OTA updates. Build and
upload the filesystem image with PlatformIO's `buildfs` and `uploadfs` targets.

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

Read a file as raw binary data with `GET /api/files/read?path=...`. The `path`
parameter is URL-encoded, and the response uses the
`application/octet-stream` content type. For example, save a file from the
device with:

```sh
curl --get --data-urlencode "path=/log_2026_45.bin" \
  http://<device-ip>/api/files/read --output log_2026_45.bin
```

An invalid path, directory, or unreadable file returns HTTP 400.

Delete a file by sending `DELETE /api/files?path=...` with the same URL-encoded
LittleFS path. Directories cannot be deleted. A successful deletion returns
`{"deleted":true}`; an invalid path or directory returns HTTP 400, a missing
file returns HTTP 404, and a filesystem failure returns HTTP 500. For example:

```sh
curl -X DELETE "http://<device-ip>/api/files?path=%2Flog_2026_45.bin"
```