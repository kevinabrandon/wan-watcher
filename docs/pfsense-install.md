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
