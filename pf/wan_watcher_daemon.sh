#!/bin/sh
#
# wan_watcher_daemon.sh
#
# Combined poll + post daemon. Runs continuously, collecting WAN metrics
# and posting them to the ESP32 every INTERVAL seconds.
#
# Usage: wan_watcher_daemon.sh [interval_seconds]
#        Default interval is 15 seconds.
#

INTERVAL="${1:-15}"
ESP32_HOST="192.168.1.216"
STATE_DIR="/var/run"

###############################################################################
# Helpers
###############################################################################

normalize_state() {
    LOSS="$1"

    if [ -z "$LOSS" ]; then
        echo "unknown"
    elif [ "$LOSS" -eq 100 ] 2>/dev/null; then
        echo "down"
    elif [ "$LOSS" -gt 5 ] 2>/dev/null; then
        echo "degraded"
    else
        echo "up"
    fi
}

iface_for_ip() {
    IP="$1"
    ifconfig | awk -v ip="$IP" '
        /^[a-z0-9]/ { iface=$1 }
        $0 ~ ip { print iface }
    ' | sed 's/://'
}

local_ip_from_sock() {
    SOCK="$1"
    echo "$SOCK" | sed -E 's/.*~([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)~.*/\1/'
}

###############################################################################
# Poll a single WAN and post to ESP32
###############################################################################

poll_and_post_wan() {
    LABEL="$1"   # "wan1" or "wan2"
    SOCK="$2"

    USAGE_PREV_FILE="${STATE_DIR}/wan_watcher_${LABEL}.usage_prev"

    LOCAL_IP=$(local_ip_from_sock "$SOCK")
    IFACE=$(iface_for_ip "$LOCAL_IP")

    # Initialize all metrics
    STATE="unknown"
    LOSS=""
    LAT_MS=""
    STD_MS=""
    DOWN_Mbps=""
    UP_Mbps=""

    # Read from dpinger socket
    LINE=$(nc -U "$SOCK" 2>/dev/null)

    if [ -n "$LINE" ]; then
        LAT_US=$(echo "$LINE" | awk '{print $2}')
        STD_US=$(echo "$LINE" | awk '{print $3}')
        LOSS=$(echo "$LINE" | awk '{print $4}')

        if [ -n "$LAT_US" ] && [ -n "$STD_US" ]; then
            LAT_MS=$((LAT_US / 1000))
            STD_MS=$((STD_US / 1000))
        fi

        STATE=$(normalize_state "$LOSS")
    fi

    # Calculate bandwidth from interface counters
    if [ -n "$IFACE" ]; then
        RX_NOW=$(netstat -I "${IFACE}" -b -n 2>/dev/null | awk 'NR==2 {print $8}')
        TX_NOW=$(netstat -I "${IFACE}" -b -n 2>/dev/null | awk 'NR==2 {print $11}')
        NOW_SEC=$(date +%s)

        if [ -n "$RX_NOW" ] && [ -n "$TX_NOW" ]; then
            if [ -f "$USAGE_PREV_FILE" ]; then
                read PREV_SEC RX_PREV TX_PREV < "$USAGE_PREV_FILE"

                if [ -n "$RX_PREV" ] && [ -n "$TX_PREV" ] && [ -n "$PREV_SEC" ]; then
                    DELTA_RX=$((RX_NOW - RX_PREV))
                    DELTA_TX=$((TX_NOW - TX_PREV))
                    DELTA_T=$((NOW_SEC - PREV_SEC))

                    if [ "$DELTA_T" -gt 0 ] && [ "$DELTA_RX" -ge 0 ] 2>/dev/null && \
                       [ "$DELTA_TX" -ge 0 ] 2>/dev/null; then

                        DOWN_BPS=$((DELTA_RX * 8 / DELTA_T))
                        UP_BPS=$((DELTA_TX * 8 / DELTA_T))

                        DOWN_Mbps=$(awk -v bps="$DOWN_BPS" 'BEGIN { printf "%.1f", bps/1000000 }')
                        UP_Mbps=$(awk -v bps="$UP_BPS" 'BEGIN { printf "%.1f", bps/1000000 }')
                    fi
                fi
            fi

            # Store current counters for next interval
            echo "${NOW_SEC} ${RX_NOW} ${TX_NOW}" > "$USAGE_PREV_FILE"
        fi
    fi

    # Skip posting if state is unknown (couldn't read dpinger)
    if [ "$STATE" = "unknown" ]; then
        echo "  ${LABEL}: unknown state, skipping post"
        return
    fi

    # Default values for missing metrics
    : "${LOSS:=100}"
    : "${LAT_MS:=0}"
    : "${STD_MS:=0}"
    : "${DOWN_Mbps:=0}"
    : "${UP_Mbps:=0}"

    # Build JSON payload
    JSON=$(printf '{"state":"%s","loss_pct":%s,"latency_ms":%s,"jitter_ms":%s,"down_mbps":%s,"up_mbps":%s}' \
        "$STATE" "$LOSS" "$LAT_MS" "$STD_MS" "$DOWN_Mbps" "$UP_Mbps")

    echo "  ${LABEL}: ${STATE} loss=${LOSS}% lat=${LAT_MS}ms down=${DOWN_Mbps}Mbps up=${UP_Mbps}Mbps"

    # POST to ESP32
    CURL_OUTPUT=$(curl -s -m 5 -w " [%{http_code}]" \
        -X POST -H "Content-Type: application/json" \
        -d "$JSON" \
        "http://${ESP32_HOST}/api/${LABEL}" 2>&1)
    CURL_RC=$?

    if [ "$CURL_RC" -ne 0 ]; then
        echo "  ${LABEL}: curl failed rc=${CURL_RC}"
    fi
}

###############################################################################
# Main loop
###############################################################################

echo "wan_watcher_daemon starting (interval=${INTERVAL}s, esp32=${ESP32_HOST})"

while true; do
    SOCKETS=$(ls /var/run/dpinger_*.sock 2>/dev/null | sort)

    if [ -z "$SOCKETS" ]; then
        echo "No dpinger sockets found, waiting..."
    else
        IDX=1
        for SOCK in $SOCKETS; do
            case "$IDX" in
                1) poll_and_post_wan "wan1" "$SOCK" ;;
                2) poll_and_post_wan "wan2" "$SOCK" ;;
            esac
            IDX=$((IDX + 1))
        done
    fi

    sleep "$INTERVAL"
done
