#!/bin/sh
# ttkbd-request — ask the remote system management host to start the keyboard + mouse session (tt030-remote-keyboard FR-13).
# Run by the TTKBD.PRG desktop icon (/opt/ttkbd/), or from a shell. Signs {"op":"ttkbd-start","request_id","ts"} with
# the card's request key (ttsign, the key yum's build requests use; "op" keeps the two request kinds apart) and
# publishes it to sam/tt030/kbd/request. The host's listener (scripts/ttkbd_request_listen.py) checks the signature,
# age and request id, then starts the same serial session as its launcher icon. Nothing answers on MQTT: the
# session starting is the answer. Exit 0 published · 1 signing or publish failed.
auth="-h ${MQTT_HOST:-mqtt.sam.int}"
[ -r /etc/yum.tt/mqtt.cred ] && . /etc/yum.tt/mqtt.cred && auth="$auth -u $MQTT_USER -P $MQTT_PASS"
now=$(date +%s)
body="{\"op\":\"ttkbd-start\",\"request_id\":\"kbd-$now-$$\",\"ts\":$now}"
sig=$(printf '%s' "$body" | ttsign /etc/yum.tt/request.key) || { echo "ttkbd-request: signing failed" >&2; exit 1; }
# shellcheck disable=SC2086 # $auth is two or six flags
ttmqtt pub $auth sam/tt030/kbd/request "$body|$sig" || { echo "ttkbd-request: publish failed" >&2; exit 1; }
logger -t ttkbd-request "start requested ($body)" 2>/dev/null
exit 0
