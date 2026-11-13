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
# WW_ESP32_HOST / WW_STATE_DIR can be overridden from the environment, which
# lets a test instance run alongside the installed daemon without sharing
# its state files.
ESP32_HOST="${WW_ESP32_HOST:-192.168.1.9}"
STATE_DIR="${WW_STATE_DIR:-/var/run}"

# Top talkers: number of LAN hosts reported per time window (0 disables)
TOP_TALKERS_N="${WW_TOP_TALKERS_N:-5}"

# Auto-detect pfSense LAN interface and IP (interface that reaches ESP32)
LAN_IFACE=$(route -n get "$ESP32_HOST" 2>/dev/null | awk '/interface:/ {print $2}')
ROUTER_IP=""
if [ -n "$LAN_IFACE" ]; then
    ROUTER_IP=$(ifconfig "$LAN_IFACE" 2>/dev/null | awk '/inet / {print $2; exit}')
fi
if [ -z "$ROUTER_IP" ]; then
    echo "WARNING: Could not detect router IP (route to ${ESP32_HOST} not found)"
fi

# Gateway-to-WAN mapping (match substring in socket filename)
WAN1_PATTERN="WAN_DHCP"      # PeakWifi
WAN2_PATTERN="WAN2_DHCP"     # Starlink

# Global variables for collected metrics
WAN1_JSON=""
WAN2_JSON=""
WAN1_IFACE=""
WAN2_IFACE=""
TOP_TALKERS_JSON=""

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

    # Remember which interface backs each WAN (used for top-talker attribution)
    case "$LABEL" in
        wan1) WAN1_IFACE="$IFACE" ;;
        wan2) WAN2_IFACE="$IFACE" ;;
    esac

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
# Top talkers: per-LAN-host bandwidth from pf state byte counters
###############################################################################
#
# Each cycle, every state entering the LAN interface is read from
# `pfctl -vvs state` and compared, by state id, against the previous cycle.
# The byte deltas are summed per LAN host, converted to Mbps, and fed through
# the same 1m/5m/15m EWMA used for the WANs. Only states still open at sample
# time are seen, so connections that open and close between samples are
# missed: this is an approximation of "who is using the bandwidth right now",
# not accounting.
#
# State files (in STATE_DIR):
#   wan_watcher_hosts.states  one line per pf state: id ip up_bytes down_bytes
#   wan_watcher_hosts.ewma    line 1: sample timestamp; then one line per host:
#                             ip d1 d5 d15 u1 u5 u15 a1 a5 a15 b1 b5 b15
#                             (d/u = down/up Mbps EWMA; a/b = Mbps EWMA of the
#                             host's traffic carried by WAN1/WAN2)
#   wan_watcher_hosts.names   "ip name" lookup rebuilt every cycle

HOSTS_STATES="${STATE_DIR}/wan_watcher_hosts.states"
HOSTS_EWMA="${STATE_DIR}/wan_watcher_hosts.ewma"
HOSTS_NAMES="${STATE_DIR}/wan_watcher_hosts.names"

# Build an "ip name" list for LAN hosts from DHCP static mappings, then Kea
# leases, then ISC leases. The first entry for an IP wins.
build_host_names() {
    {
        awk '
            /<staticmap>/  { inmap=1; ip=""; hn=""; ds="" }
            inmap && /<ipaddr>/   { ip=$0; sub(/.*<ipaddr>/,"",ip); sub(/<\/ipaddr>.*/,"",ip) }
            inmap && /<hostname>/ { hn=$0; sub(/.*<hostname>/,"",hn); sub(/<\/hostname>.*/,"",hn) }
            inmap && /<descr>/    { ds=$0; sub(/.*<descr>/,"",ds); sub(/<\/descr>.*/,"",ds)
                                    sub(/^<!\[CDATA\[/,"",ds); sub(/\]\]>$/,"",ds) }
            /<\/staticmap>/ { if (ip != "") print ip, (hn != "" ? hn : ds); inmap=0 }
        ' /conf/config.xml 2>/dev/null
        for LEASES in /var/lib/kea/dhcp4.leases /var/db/kea/dhcp4.leases; do
            [ -f "$LEASES" ] && awk -F, '
                NR > 1 && $9 != "" { h=$9; sub(/\.$/,"",h); n[$1]=h }
                END { for (ip in n) print ip, n[ip] }
            ' "$LEASES"
        done
        [ -f /var/dhcpd/var/db/dhcpd.leases ] && awk '
            /^lease / { ip=$2 }
            /client-hostname/ { h=$2; gsub(/[";]/,"",h); if (ip != "") n[ip]=h }
            END { for (ip in n) print ip, n[ip] }
        ' /var/dhcpd/var/db/dhcpd.leases
    } > "$HOSTS_NAMES" 2>/dev/null
}

poll_top_talkers() {
    TOP_TALKERS_JSON=""
    is_numeric "$TOP_TALKERS_N" && [ "$TOP_TALKERS_N" -gt 0 ] || return
    [ -n "$LAN_IFACE" ] || return

    build_host_names
    [ -f "$HOSTS_STATES" ] || : > "$HOSTS_STATES"
    [ -f "$HOSTS_EWMA" ] || : > "$HOSTS_EWMA"
    NOW_SEC=$(date +%s)

    TOP_TALKERS_JSON=$(pfctl -vvs state 2>/dev/null | awk \
        -v lan="$LAN_IFACE" -v router="$ROUTER_IP" \
        -v wan1="$WAN1_IFACE" -v wan2="$WAN2_IFACE" \
        -v now="$NOW_SEC" -v topn="$TOP_TALKERS_N" \
        -v names_f="$HOSTS_NAMES" -v states_f="$HOSTS_STATES" -v ewma_f="$HOSTS_EWMA" \
        -v states_out="${HOSTS_STATES}.new" -v ewma_out="${HOSTS_EWMA}.new" '
        # "addr:port" -> addr for IPv4; IPv6 ("addr[port]") is skipped
        function ipof(s,   a) { if (index(s, "[")) return ""; split(s, a, ":"); return a[1] }
        # "gateway@iface" -> wan1 / wan2 / ""
        function wanof(gw,   i) {
            i = index(gw, "@"); if (!i) return ""
            gw = substr(gw, i + 1)
            if (gw == wan1) return "wan1"; if (gw == wan2) return "wan2"; return ""
        }
        function clean(n) { gsub(/[^A-Za-z0-9._ -]/, "", n); return substr(n, 1, 32) }
        function ewma(a, cur, prev, seed) { return seed ? cur : a * cur + (1 - a) * prev }
        # JSON array of the top N hosts by down+up from the given arrays
        function topjson(Dn, Up, Wa, Wb,   out, sep, k, ip, best, bv, v, wan) {
            out = "["; sep = ""
            for (k = 0; k < topn; k++) {
                best = ""; bv = 0
                for (ip in Dn) if (!(ip in used)) { v = Dn[ip] + Up[ip]; if (v > bv) { bv = v; best = ip } }
                if (best == "") break
                used[best] = 1
                wan = (Wa[best] > Wb[best]) ? "wan1" : (Wb[best] > 0 ? "wan2" : "")
                out = out sep sprintf("{\"ip\":\"%s\",\"name\":\"%s\",\"down\":%.2f,\"up\":%.2f,\"wan\":\"%s\"}", \
                                      best, (best in nm) ? nm[best] : "", Dn[best], Up[best], wan)
                sep = ","
            }
            for (ip in used) delete used[ip]
            return out "]"
        }

        # --- input 1: host names
        FILENAME == names_f { ip = $1; n = $0; sub(/^[^ ]+ */, "", n); if (!(ip in nm)) nm[ip] = clean(n); next }
        # --- input 2: previous state snapshot
        FILENAME == states_f { pup[$1] = $3; pdn[$1] = $4; next }
        # --- input 3: previous EWMA values
        FILENAME == ewma_f {
            if (FNR == 1) { prev_ts = $1; next }
            ip = $1; known[ip] = 1; nknown++
            D1[ip]=$2;  D5[ip]=$3;  D15[ip]=$4;  U1[ip]=$5;  U5[ip]=$6;  U15[ip]=$7
            A1[ip]=$8;  A5[ip]=$9;  A15[ip]=$10; B1[ip]=$11; B5[ip]=$12; B15[ip]=$13
            next
        }
        # --- input 4: pfctl -vvs state (several lines per state)
        /^[A-Za-z]/ {
            cur = ""
            # "<-" = state entering the LAN interface; $5 is the LAN host, $3 the destination.
            # Traffic to the router itself (DNS, web UI) is not internet usage.
            if ($1 == lan && $4 == "<-" && index($3, router ":") != 1) cur = ipof($5)
            next
        }
        cur != "" && / bytes,/ {
            for (i = 2; i <= NF; i++) if ($i == "bytes,") { split($(i-1), bp, ":"); up = bp[1]; dn = bp[2] }
            next
        }
        cur != "" && $1 == "id:" {
            id = $2; gw = ""
            for (i = 3; i <= NF; i++) if ($i == "route-to:") gw = $(i+1)
            if (id in pup) { du = up - pup[id]; dd = dn - pdn[id]; if (du < 0) du = 0; if (dd < 0) dd = 0 }
            else           { du = up; dd = dn }      # new since last sample: all its bytes count
            seen[cur] = 1; U[cur] += du; D[cur] += dd
            w = wanof(gw)
            if (w == "wan1") WA[cur] += du + dd; else if (w == "wan2") WB[cur] += du + dd
            print id, cur, up, dn > states_out
            cur = ""
            next
        }
        END {
            dt = (prev_ts == "") ? 0 : now - prev_ts
            first = (dt <= 0 || dt > 600)       # no usable previous sample: snapshot only
            a1 = 1 - exp(-dt/60); a5 = 1 - exp(-dt/300); a15 = 1 - exp(-dt/900)
            print now > ewma_out
            for (ip in seen)  all[ip] = 1
            for (ip in known) all[ip] = 1
            for (ip in all) {
                d = first ? 0 : D[ip]  * 8 / dt / 1e6
                u = first ? 0 : U[ip]  * 8 / dt / 1e6
                ra = first ? 0 : WA[ip] * 8 / dt / 1e6
                rb = first ? 0 : WB[ip] * 8 / dt / 1e6
                # On the first real sample every host is new: seed the averages with
                # the current rate, as poll_wan does. Later, a host not in the file was
                # idle (or just forgotten), so its averages ramp up from zero instead.
                s = (nknown == 0) && !(ip in known)
                D1[ip]=ewma(a1,d,D1[ip],s); D5[ip]=ewma(a5,d,D5[ip],s); D15[ip]=ewma(a15,d,D15[ip],s)
                U1[ip]=ewma(a1,u,U1[ip],s); U5[ip]=ewma(a5,u,U5[ip],s); U15[ip]=ewma(a15,u,U15[ip],s)
                A1[ip]=ewma(a1,ra,A1[ip],s); A5[ip]=ewma(a5,ra,A5[ip],s); A15[ip]=ewma(a15,ra,A15[ip],s)
                B1[ip]=ewma(a1,rb,B1[ip],s); B5[ip]=ewma(a5,rb,B5[ip],s); B15[ip]=ewma(a15,rb,B15[ip],s)
                # forget hosts that are idle now and whose 15m average has faded out (< 10 kbps)
                if (d + u < 0.01 && D15[ip] + U15[ip] < 0.01) {
                    delete D1[ip]; delete D5[ip]; delete D15[ip]; delete U1[ip]; delete U5[ip]; delete U15[ip]
                    delete A1[ip]; delete A5[ip]; delete A15[ip]; delete B1[ip]; delete B5[ip]; delete B15[ip]
                    continue
                }
                printf "%s %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", ip, \
                    D1[ip], D5[ip], D15[ip], U1[ip], U5[ip], U15[ip], \
                    A1[ip], A5[ip], A15[ip], B1[ip], B5[ip], B15[ip] > ewma_out
                RD[ip] = d; RU[ip] = u; RA[ip] = ra; RB[ip] = rb
            }
            if (first) exit
            printf "{\"15s\":%s,\"1m\":%s,\"5m\":%s,\"15m\":%s}", \
                topjson(RD, RU, RA, RB), topjson(D1, U1, A1, B1), \
                topjson(D5, U5, A5, B5), topjson(D15, U15, A15, B15)
        }
    ' "$HOSTS_NAMES" "$HOSTS_STATES" "$HOSTS_EWMA" -)

    mv -f "${HOSTS_STATES}.new" "$HOSTS_STATES" 2>/dev/null
    mv -f "${HOSTS_EWMA}.new" "$HOSTS_EWMA" 2>/dev/null
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

    # Log the WAN portion only; top_talkers is appended to the POST but is too
    # large to log every cycle.
    LOG_JSON="${BATCH_JSON}}"
    TT_NOTE=""
    if [ -n "$TOP_TALKERS_JSON" ]; then
        BATCH_JSON="${BATCH_JSON},\"top_talkers\":${TOP_TALKERS_JSON}"
        TT_NOTE=" +top_talkers($(( $(wc -l < "$HOSTS_EWMA") - 1 )) hosts)"
    fi

    BATCH_JSON="${BATCH_JSON}}"

    # Skip if no WANs collected
    if [ -z "$WAN1_JSON" ] && [ -z "$WAN2_JSON" ]; then
        echo "  No WAN data collected, skipping POST"
        return
    fi

    # WW_DEBUG=1 logs the complete POST body
    [ -n "$WW_DEBUG" ] && echo "POST body: ${BATCH_JSON}"

    # POST to ESP32
    HTTP_CODE=$(curl -s -o /dev/null -m 5 -w "%{http_code}" \
        -X POST -H "Content-Type: application/json" \
        -d "$BATCH_JSON" \
        "http://${ESP32_HOST}/api/wans" 2>&1)
    CURL_RC=$?

    if [ "$CURL_RC" -ne 0 ]; then
        echo "${LOG_JSON}${TT_NOTE} -> POST failed rc=${CURL_RC}"
    else
        echo "${LOG_JSON}${TT_NOTE} -> POST OK (${HTTP_CODE})"
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

        # Per-host usage (needs WAN interfaces discovered by poll_wan above)
        poll_top_talkers

        # Post all collected WANs in a single batch request
        post_all_wans
    fi

    sleep "$INTERVAL"
done
