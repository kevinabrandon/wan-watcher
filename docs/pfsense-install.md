## Installation (pfSense)

### 1. Install the daemon

```sh
cp pf/wan_watcher_daemon.sh /usr/local/bin/
chmod +x /usr/local/bin/wan_watcher_daemon.sh
```

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
  /usr/sbin/daemon -f -p /var/run/wan_watcher.pid sh -c '/usr/local/bin/wan_watcher_daemon.sh >> /var/log/wan_watcher.log 2>&1'
  ```
* **Type:** `shellcmd`

Save.

### 4. Start it now (optional)

Shellcmd runs at boot. To start without rebooting:

```sh
/usr/sbin/daemon -f -p /var/run/wan_watcher.pid sh -c '/usr/local/bin/wan_watcher_daemon.sh >> /var/log/wan_watcher.log 2>&1'
```

### 5. Verify

```sh
cat /var/run/wan_watcher.pid
ps -p $(cat /var/run/wan_watcher.pid)
tail -f /var/log/wan_watcher.log
```

#### Stop / restart

```sh
kill "$(cat /var/run/wan_watcher.pid)"
/usr/sbin/daemon -f -p /var/run/wan_watcher.pid sh -c '/usr/local/bin/wan_watcher_daemon.sh >> /var/log/wan_watcher.log 2>&1'
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
