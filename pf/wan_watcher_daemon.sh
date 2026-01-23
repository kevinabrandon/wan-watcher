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
ESP32_HOST="192.168.1.9"
STATE_DIR="/var/run"

# Auto-detect pfSense LAN IP (interface that reaches ESP32)
get_router_ip() {
    LAN_IFACE=$(route -n get "$ESP32_HOST" 2>/dev/null | awk '/interface:/ {print $2}')
    if [ -n "$LAN_IFACE" ]; then
        ifconfig "$LAN_IFACE" 2>/dev/null | awk '/inet / {print $2}'
    fi
}
ROUTER_IP=$(get_router_ip)
if [ -z "$ROUTER_IP" ]; then
    echo "WARNING: Could not detect router IP (route to ${ESP32_HOST} not found)"
fi

# Gateway-to-WAN mapping (match substring in socket filename)
WAN1_PATTERN="WAN_DHCP"      # PeakWifi
WAN2_PATTERN="WAN2_DHCP"     # Starlink

# Global variables for collected metrics
WAN1_JSON=""
WAN2_JSON=""

###############################################################################
# Helpers
###############################################################################

is_numeric() {
    case "$1" in
        ''|*[!0-9]*) return 1 ;;
        *) return 0 ;;
    esac
}

normalize_state() {
    LOSS="$1"
    LATENCY="$2"

    if ! is_numeric "$LOSS"; then
        echo "unknown"
    elif [ "$LOSS" -gt 50 ]; then
        echo "down"
    elif [ "$LOSS" -gt 5 ]; then
        echo "degraded"
    elif is_numeric "$LATENCY" && [ "$LATENCY" -gt 200 ]; then
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

monitor_ip_from_sock() {
    SOCK="$1"
    echo "$SOCK" | sed -E 's/.*~[0-9.]+~([0-9.]+)\.sock/\1/'
}

gateway_for_iface() {
    IFACE="$1"
    # Try routing table first (for default route interfaces)
    GW=$(netstat -rn | awk -v iface="$IFACE" '$NF == iface && $1 == "0.0.0.0" {print $2}')
    if [ -n "$GW" ]; then
        echo "$GW"
        return
    fi
    # Fall back to pf rules (for policy-routed interfaces)
    # Escape dots in interface name for regex matching
    IFACE_ESC=$(echo "$IFACE" | sed 's/\./\\./g')
    GW=$(grep -E "route-to \( ${IFACE_ESC} " /tmp/rules.debug 2>/dev/null | head -1 | sed -E "s/.*route-to \\( ${IFACE_ESC} ([0-9.]+) \\).*/\\1/")
    echo "$GW"
}

# Calculate alpha for EWMA based on actual elapsed time
# alpha = 1 - e^(-elapsed/window)
alpha_for_window() {
    local ELAPSED="$1" WINDOW="$2"
    awk -v t="$ELAPSED" -v w="$WINDOW" 'BEGIN { printf "%.8f", 1 - exp(-t/w) }'
}

# Calculate EWMA: ewma_calc <alpha> <current> <previous_ewma>
# Returns empty string if no valid computation possible (previous is empty)
ewma_calc() {
    local ALPHA="$1" CURRENT="$2" PREV_EWMA="$3"
    if [ -z "$PREV_EWMA" ]; then
        echo ""
        return
    fi
    awk -v a="$ALPHA" -v cur="$CURRENT" -v prev="$PREV_EWMA" \
        'BEGIN { printf "%.4f", a * cur + (1 - a) * prev }'
}

# Format value for JSON output (rounds to 2 decimal places)
# Returns default if value is empty
format_json_num() {
    local VAL="$1" DEFAULT="${2:-0}"
    if [ -z "$VAL" ]; then
        echo "$DEFAULT"
    else
        awk -v v="$VAL" 'BEGIN { printf "%.2f", v }'
    fi
}

###############################################################################
# Poll a single WAN (collect metrics only, no POST)
###############################################################################

poll_wan() {
    LABEL="$1"   # "wan1" or "wan2"
    SOCK="$2"

    LOCAL_IP=$(local_ip_from_sock "$SOCK")
    MONITOR_IP=$(monitor_ip_from_sock "$SOCK")
    IFACE=$(iface_for_ip "$LOCAL_IP")
    GATEWAY_IP=$(gateway_for_iface "$IFACE")

    # Use interface name in state file so mapping changes don't corrupt data
    USAGE_PREV_FILE="${STATE_DIR}/wan_watcher_${IFACE}.usage_prev"

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

        STATE=$(normalize_state "$LOSS" "$LAT_MS")
    fi

    # Calculate bandwidth from interface counters
    if [ -n "$IFACE" ]; then
        NETSTAT_LINE=$(netstat -I "${IFACE}" -b -n 2>/dev/null | awk 'NR==2')
        RX_NOW=$(echo "$NETSTAT_LINE" | awk '{print $8}')
        TX_NOW=$(echo "$NETSTAT_LINE" | awk '{print $11}')
        NOW_SEC=$(date +%s)

        if is_numeric "$RX_NOW" && is_numeric "$TX_NOW"; then
            if [ -f "$USAGE_PREV_FILE" ]; then
                read PREV_SEC RX_PREV TX_PREV PREV_DOWN_1M PREV_DOWN_5M PREV_DOWN_15M PREV_UP_1M PREV_UP_5M PREV_UP_15M PREV_EWMA_VALID < "$USAGE_PREV_FILE"

                if is_numeric "$RX_PREV" && is_numeric "$TX_PREV" && is_numeric "$PREV_SEC"; then
                    DELTA_RX=$((RX_NOW - RX_PREV))
                    DELTA_TX=$((TX_NOW - TX_PREV))
                    DELTA_T=$((NOW_SEC - PREV_SEC))

                    if [ "$DELTA_T" -gt 0 ] && [ "$DELTA_RX" -ge 0 ] && [ "$DELTA_TX" -ge 0 ]; then
                        DOWN_BPS=$((DELTA_RX * 8 / DELTA_T))
                        UP_BPS=$((DELTA_TX * 8 / DELTA_T))

                        # High precision for EWMA input
                        DOWN_Mbps=$(awk -v bps="$DOWN_BPS" 'BEGIN { printf "%.4f", bps/1000000 }')
                        UP_Mbps=$(awk -v bps="$UP_BPS" 'BEGIN { printf "%.4f", bps/1000000 }')

                        if [ "$PREV_EWMA_VALID" != "1" ]; then
                            # First valid sample - seed all windows
                            DOWN_1M="$DOWN_Mbps"
                            DOWN_5M="$DOWN_Mbps"
                            DOWN_15M="$DOWN_Mbps"
                            UP_1M="$UP_Mbps"
                            UP_5M="$UP_Mbps"
                            UP_15M="$UP_Mbps"
                        else
                            # Calculate dynamic alphas based on actual elapsed time
                            ALPHA_1M=$(alpha_for_window "$DELTA_T" 60)
                            ALPHA_5M=$(alpha_for_window "$DELTA_T" 300)
                            ALPHA_15M=$(alpha_for_window "$DELTA_T" 900)

                            # Calculate EWMA for bandwidth
                            DOWN_1M=$(ewma_calc "$ALPHA_1M" "$DOWN_Mbps" "$PREV_DOWN_1M")
                            DOWN_5M=$(ewma_calc "$ALPHA_5M" "$DOWN_Mbps" "$PREV_DOWN_5M")
                            DOWN_15M=$(ewma_calc "$ALPHA_15M" "$DOWN_Mbps" "$PREV_DOWN_15M")
                            UP_1M=$(ewma_calc "$ALPHA_1M" "$UP_Mbps" "$PREV_UP_1M")
                            UP_5M=$(ewma_calc "$ALPHA_5M" "$UP_Mbps" "$PREV_UP_5M")
                            UP_15M=$(ewma_calc "$ALPHA_15M" "$UP_Mbps" "$PREV_UP_15M")
                        fi
                        EWMA_VALID=1
                    fi
                fi
            fi

            # Store current counters and EWMA values for next interval
            if [ "$EWMA_VALID" = "1" ]; then
                # Valid delta - write new EWMA values
                echo "${NOW_SEC} ${RX_NOW} ${TX_NOW} ${DOWN_1M} ${DOWN_5M} ${DOWN_15M} ${UP_1M} ${UP_5M} ${UP_15M} 1" > "$USAGE_PREV_FILE"
            else
                # Invalid delta - preserve previous EWMA values unchanged
                echo "${NOW_SEC} ${RX_NOW} ${TX_NOW} ${PREV_DOWN_1M} ${PREV_DOWN_5M} ${PREV_DOWN_15M} ${PREV_UP_1M} ${PREV_UP_5M} ${PREV_UP_15M} ${PREV_EWMA_VALID:-0}" > "$USAGE_PREV_FILE"
            fi
        fi
    fi

    # Skip if state is unknown (couldn't read dpinger)
    if [ "$STATE" = "unknown" ]; then
        echo "  ${LABEL}: unknown state, skipping"
        return
    fi

    # Default values for missing metrics (only for non-bandwidth fields)
    : "${LOSS:=100}"
    : "${LAT_MS:=0}"
    : "${STD_MS:=0}"

    # Format bandwidth values for JSON (round to %.2f, default to 0)
    DOWN_Mbps_JSON=$(format_json_num "$DOWN_Mbps" "0")
    UP_Mbps_JSON=$(format_json_num "$UP_Mbps" "0")
    DOWN_1M_JSON=$(format_json_num "$DOWN_1M" "0")
    DOWN_5M_JSON=$(format_json_num "$DOWN_5M" "0")
    DOWN_15M_JSON=$(format_json_num "$DOWN_15M" "0")
    UP_1M_JSON=$(format_json_num "$UP_1M" "0")
    UP_5M_JSON=$(format_json_num "$UP_5M" "0")
    UP_15M_JSON=$(format_json_num "$UP_15M" "0")

    # Build JSON payload and store in global variable
    JSON=$(printf '{"state":"%s","loss_pct":%s,"latency_ms":%s,"jitter_ms":%s,"down_mbps":%s,"up_mbps":%s,"down_1m":%s,"down_5m":%s,"down_15m":%s,"up_1m":%s,"up_5m":%s,"up_15m":%s,"local_ip":"%s","gateway_ip":"%s","monitor_ip":"%s"}' \
        "$STATE" "$LOSS" "$LAT_MS" "$STD_MS" "$DOWN_Mbps_JSON" "$UP_Mbps_JSON" \
        "$DOWN_1M_JSON" "$DOWN_5M_JSON" "$DOWN_15M_JSON" "$UP_1M_JSON" "$UP_5M_JSON" "$UP_15M_JSON" \
        "$LOCAL_IP" "$GATEWAY_IP" "$MONITOR_IP")

    # Store in global variable based on label
    case "$LABEL" in
        wan1) WAN1_JSON="$JSON" ;;
        wan2) WAN2_JSON="$JSON" ;;
    esac
}

###############################################################################
# Post all collected WANs to ESP32 (batch)
###############################################################################

post_all_wans() {
    # Get current timestamp in ISO 8601 format
    TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Build batch JSON payload with top-level fields
    BATCH_JSON="{\"router_ip\":\"${ROUTER_IP}\",\"timestamp\":\"${TIMESTAMP}\""

    if [ -n "$WAN1_JSON" ]; then
        BATCH_JSON="${BATCH_JSON},\"wan1\":${WAN1_JSON}"
    fi

    if [ -n "$WAN2_JSON" ]; then
        BATCH_JSON="${BATCH_JSON},\"wan2\":${WAN2_JSON}"
    fi

    BATCH_JSON="${BATCH_JSON}}"

    # Skip if no WANs collected
    if [ -z "$WAN1_JSON" ] && [ -z "$WAN2_JSON" ]; then
        echo "  No WAN data collected, skipping POST"
        return
    fi

    # POST to ESP32
    HTTP_CODE=$(curl -s -o /dev/null -m 5 -w "%{http_code}" \
        -X POST -H "Content-Type: application/json" \
        -d "$BATCH_JSON" \
        "http://${ESP32_HOST}/api/wans" 2>&1)
    CURL_RC=$?

    if [ "$CURL_RC" -ne 0 ]; then
        echo "${BATCH_JSON} -> POST failed rc=${CURL_RC}"
    else
        echo "${BATCH_JSON} -> POST OK (${HTTP_CODE})"
    fi
}

###############################################################################
# Main loop
###############################################################################

echo "wan_watcher_daemon starting (interval=${INTERVAL}s, esp32=${ESP32_HOST})"

while true; do
    # Reset global JSON variables
    WAN1_JSON=""
    WAN2_JSON=""

    # Check if any dpinger sockets exist (using glob, not ls parsing)
    set -- /var/run/dpinger_*.sock
    if [ ! -e "$1" ]; then
        echo "No dpinger sockets found, waiting..."
    else
        # Collect metrics from all WANs
        for SOCK do
            # Check WAN2 first (more specific pattern)
            case "$SOCK" in
                *"$WAN2_PATTERN"*)
                    poll_wan "wan2" "$SOCK"
                    ;;
                *"$WAN1_PATTERN"*)
                    poll_wan "wan1" "$SOCK"
                    ;;
                *)
                    echo "Unknown socket (no pattern match): $SOCK"
                    ;;
            esac
        done

        # Post all collected WANs in a single batch request
        post_all_wans
    fi

    sleep "$INTERVAL"
done
