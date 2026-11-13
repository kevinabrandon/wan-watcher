#!/bin/sh
#
# wan_watcher_start.sh
#
# Boot-time launcher for wan_watcher_daemon.sh on pfSense. It installs the
# newsyslog rotation rule for the daemon's log and starts the daemon under
# daemon(8) in a way that lets the log be rotated and reopened.
#
# Use this as the Shellcmd command (see docs/pfsense-install.md). Running it
# while wan-watcher is already up is a no-op.
#

DAEMON="/usr/local/bin/wan_watcher_daemon.sh"
LOG="/var/log/wan_watcher.log"
PIDFILE="/var/run/wan_watcher.pid"                 # the daemon script itself
SUPERVISOR_PIDFILE="/var/run/wan_watcher_sup.pid"  # daemon(8); gets SIGHUP on rotation
NEWSYSLOG_CONF="/var/etc/newsyslog.conf.d/wan_watcher.conf"

# Log rotation rule: rotate at 10 MB, keep 7 gzip'd copies, create the file if
# missing (C), and send SIGHUP to the supervisor so it reopens the new file.
# pfSense's /etc/newsyslog.conf only includes /var/etc/newsyslog.conf.d/*, and
# pfSense regenerates that directory, so the rule is rewritten at every start
# instead of being installed once. newsyslog runs from cron every minute.
mkdir -p "$(dirname "$NEWSYSLOG_CONF")"
printf '%s\t\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$LOG" "root:wheel" 644 7 10240 "*" ZC "$SUPERVISOR_PIDFILE" 1 > "$NEWSYSLOG_CONF"

# Already running? (supervisor pidfile names a live daemon(8) process)
if [ -f "$SUPERVISOR_PIDFILE" ] && kill -0 "$(cat "$SUPERVISOR_PIDFILE")" 2>/dev/null; then
    echo "wan-watcher already running (supervisor pid $(cat "$SUPERVISOR_PIDFILE"))"
    exit 0
fi

# -f detach, -o send the child's stdout/stderr to LOG, -H reopen LOG on SIGHUP
exec /usr/sbin/daemon -f -H -o "$LOG" -P "$SUPERVISOR_PIDFILE" -p "$PIDFILE" "$DAEMON"
