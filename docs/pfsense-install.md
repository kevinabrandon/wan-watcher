## Installation (pfSense)

### 1. Install the scripts

```sh
cp pf/wan_watcher_daemon.sh pf/wan_watcher_start.sh /usr/local/bin/
chmod +x /usr/local/bin/wan_watcher_daemon.sh /usr/local/bin/wan_watcher_start.sh
```

`wan_watcher_daemon.sh` is the collector. `wan_watcher_start.sh` launches it under `daemon(8)` and installs the log rotation rule (see below).

### 2. Install Shellcmd

pfSense Web UI:

* **System → Package Manager → Available Packages**
* Install **Shellcmd**

### 3. Add a startup entry

pfSense Web UI:

* **Services → Shellcmd → Add**

Set:

* **Command:**

  ```sh
  /usr/local/bin/wan_watcher_start.sh
  ```
* **Type:** `shellcmd`

Save.

### 4. Start it now (optional)

Shellcmd runs at boot. To start without rebooting:

```sh
/usr/local/bin/wan_watcher_start.sh
```

### 5. Verify

```sh
cat /var/run/wan_watcher.pid
ps -p $(cat /var/run/wan_watcher.pid)
tail -f /var/log/wan_watcher.log
```

#### Stop / restart

```sh
kill "$(cat /var/run/wan_watcher_sup.pid)" "$(cat /var/run/wan_watcher.pid)"
/usr/local/bin/wan_watcher_start.sh
```

### Log rotation

The daemon logs one line per cycle (about 4 MB/day). `wan_watcher_start.sh` writes `/var/etc/newsyslog.conf.d/wan_watcher.conf`, which makes pfSense's `newsyslog` (run from cron every minute) rotate `/var/log/wan_watcher.log` at 10 MB and keep 7 gzip'd copies. After each rotation newsyslog sends `SIGHUP` to the `daemon(8)` supervisor (pid in `/var/run/wan_watcher_sup.pid`), which reopens the new file; this is why the daemon is started with `daemon -o` rather than a shell `>>` redirect.

The rule is rewritten on every start because pfSense regenerates `/var/etc`. To rotate by hand:

```sh
newsyslog -F /var/log/wan_watcher.log
```

### Environment overrides

| Variable | Default | Purpose |
|----------|---------|---------|
| `WW_ESP32_HOST` | `192.168.1.9` | ESP32 address to POST to |
| `WW_STATE_DIR` | `/var/run` | Where the daemon keeps its counter/EWMA state files |
| `WW_TOP_TALKERS_N` | `5` | Hosts reported per window in `top_talkers` (`0` disables the feature) |
| `WW_DEBUG` | unset | Set to `1` to log the full POST body each cycle |

### Testing a modified daemon

A second copy can run next to the installed daemon as long as it uses its own state directory, otherwise the two instances overwrite each other's counters:

```sh
mkdir -p /tmp/wwtest
WW_DEBUG=1 WW_STATE_DIR=/tmp/wwtest sh /tmp/wan_watcher_daemon.sh 15
```

Posting twice per cycle to the ESP32 is harmless.
