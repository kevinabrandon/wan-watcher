#!/bin/sh
# Post all WAN metrics to ESP32 as JSON, only if status is fresh

SLEEP_SECS="$1"
ESP32_HOST="192.168.1.216"

sleep "$SLEEP_SECS"

echo "---- wan_watcher_post (sleep=${SLEEP_SECS}) ----"

# expand matching files
set -- /var/run/wan_watcher_wan*.status
if [ ! -e "$1" ]; then
    echo "No status files found matching /var/run/wan_watcher_wan*.status"
    exit 0
fi

for STATUS_FILE in "$@"; do
    echo "Checking $STATUS_FILE ..."

    if [ ! -s "$STATUS_FILE" ]; then
        echo "  -> File is empty, skipping."
        continue
    fi

    # last non-empty line
    LINE=$(grep -v '^[[:space:]]*$' "$STATUS_FILE" | tail -n 1)
    echo "  -> Last line: $LINE"

    # extract fields: timestamp state loss_pct latency_ms jitter_ms down_mbps up_mbps
    TS=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $1}')
    STATE=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $2}')
    LOSS_PCT=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $3}')
    LATENCY_MS=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $4}')
    JITTER_MS=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $5}')
    DOWN_MBPS=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $6}')
    UP_MBPS=$(printf '%s\n' "$LINE" | awk -F '\t' '{print $7}')

    case "$STATE" in
        up|degraded|down)
            echo "  -> State: $STATE"
            ;;
        *)
            echo "  -> Invalid state ($STATE), skipping."
            continue
            ;;
    esac

    # parse TS as UTC
    TS_EPOCH=$(date -j -u -f "%Y-%m-%dT%H:%M:%SZ" "$TS" "+%s" 2>/dev/null)
    if [ -z "$TS_EPOCH" ]; then
        echo "  -> Timestamp parse failed for: $TS"
        continue
    fi

    # compare in UTC
    NOW_EPOCH=$(date -u "+%s")
    AGE=$(( NOW_EPOCH - TS_EPOCH ))

    echo "  -> Timestamp age: ${AGE}s"

    # enforce freshness; also discard future timestamps
    if [ "$AGE" -gt 60 ] || [ "$AGE" -lt 0 ]; then
        echo "  -> Stale or future timestamp (age=${AGE}s), skipping."
        continue
    fi

    # derive WAN_ID from filename
    BASENAME=$(basename "$STATUS_FILE")
    WAN_ID=$(printf '%s\n' "$BASENAME" | sed -e 's/^wan_watcher_//' -e 's/\.status$//')

    # default values for missing metrics
    : "${LOSS_PCT:=100}"
    : "${LATENCY_MS:=0}"
    : "${JITTER_MS:=0}"
    : "${DOWN_MBPS:=0}"
    : "${UP_MBPS:=0}"

    # build JSON payload
    JSON=$(printf '{"state":"%s","loss_pct":%s,"latency_ms":%s,"jitter_ms":%s,"down_mbps":%s,"up_mbps":%s}' \
        "$STATE" "$LOSS_PCT" "$LATENCY_MS" "$JITTER_MS" "$DOWN_MBPS" "$UP_MBPS")

    echo "  -> WAN_ID: $WAN_ID"
    echo "  -> JSON: $JSON"
    echo "  -> Curling: POST http://${ESP32_HOST}/api/${WAN_ID}"

    # capture output and exit code
    CURL_OUTPUT=$(curl -s -m 2 -w " [http_code=%{http_code}]" \
        -X POST -H "Content-Type: application/json" \
        -d "$JSON" \
        "http://${ESP32_HOST}/api/${WAN_ID}" 2>&1)
    CURL_RC=$?

    echo "  -> curl rc=$CURL_RC output: ${CURL_OUTPUT}"
done

echo "---- done ----"
