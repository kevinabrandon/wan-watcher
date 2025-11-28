#!/bin/sh
#
# wan_watcher_poll.sh
#
# Usage: wan_watcher_poll.sh <delay_seconds>
#

DELAY="$1"
[ -n "$DELAY" ] || DELAY=0
sleep "$DELAY"

STATE_DIR="/var/run"

# Tab-separated header for snapshot file
HEADER_FIELDS="timestamp state loss_pct lat_ms std_ms down_mbps up_mbps"

###############################################################################
# Helpers
###############################################################################

normalize_state() {
  LOSS="$1"

  if [ -z "$LOSS" ]; then
    echo "unknown"
  elif [ "$LOSS" -eq 100 ] 2>/dev/null; then
    echo "down"
  elif [ "$LOSS" -gt 0 ] 2>/dev/null; then
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

poll_wan() {
  LABEL="$1"   # "wan1" or "wan2"
  SOCK="$2"

  STATUS_FILE="${STATE_DIR}/wan_watcher_${LABEL}.status"
  TMP_FILE="${STATUS_FILE}.$$"
  USAGE_PREV_FILE="${STATE_DIR}/wan_watcher_${LABEL}.usage_prev"

  LOCAL_IP=$(local_ip_from_sock "$SOCK")
  IFACE=$(iface_for_ip "$LOCAL_IP")

  # Initialize all metrics as empty
  STATE="unknown"
  LOSS=""
  LAT_MS=""
  STD_MS=""
  DOWN_Mbps=""
  UP_Mbps=""

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

  DATE_STR=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

  if [ -n "$IFACE" ]; then
    # netstat header on pfSense:
    # Name Mtu Network Address Ipkts Ierrs Idrop Ibytes Opkts Oerrs Obytes Coll
    RX_NOW=$(netstat -I "${IFACE}" -b -n 2>/dev/null | awk 'NR==2 {print $8}')
    TX_NOW=$(netstat -I "${IFACE}" -b -n 2>/dev/null | awk 'NR==2 {print $11}')
    NOW_SEC=$(date +%s)

    if [ -n "$RX_NOW" ] && [ -n "$TX_NOW" ]; then
      if [ -f "$USAGE_PREV_FILE" ]; then
        # usage_prev: "<epoch_sec> <rx_bytes> <tx_bytes>"
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

      # store current counters for next interval:
      echo "${NOW_SEC} ${RX_NOW} ${TX_NOW}" > "$USAGE_PREV_FILE"
    fi
  fi

  # Atomic snapshot: write header + single data line to temp file, then mv
  {
    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" $HEADER_FIELDS
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$DATE_STR" "$STATE" "$LOSS" "$LAT_MS" "$STD_MS" "$DOWN_Mbps" "$UP_Mbps"
  } > "$TMP_FILE"

  mv "$TMP_FILE" "$STATUS_FILE"
}

###############################################################################
# Main
###############################################################################

SOCKETS=$(ls /var/run/dpinger_*.sock 2>/dev/null | sort)

IDX=1
for SOCK in $SOCKETS; do
  case "$IDX" in
    1) poll_wan "wan1" "$SOCK" ;;
    2) poll_wan "wan2" "$SOCK" ;;
  esac
  IDX=$((IDX + 1))
done

exit 0
