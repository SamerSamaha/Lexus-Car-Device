#!/bin/sh
# Makes the certificates of the phone link (LHU-053, DN-053): a private certificate authority
# that the iPhone trusts once, and the Pi's server certificate signed by it. Safari gives a page
# the phone's position only over HTTPS (assumption A27).
#
#   deploy/car/phone_link_certificates.sh <folder>
#
# Run by the install script with ~/.config/lexus-head-unit/phone-link. Safe to run again: the
# authority is kept (the phone trusts it), and the server certificate is made again only when
# it is missing or ends within 30 days. Apple accepts a server certificate of at most 398 days
# with the serverAuth usage and the names in subjectAltName, so it is made for 397 days. Names:
# lexus-head-unit.local, localhost, 127.0.0.1, and every address of the iPhone hotspot subnet
# 172.20.10.0/28 (assumption A28). The keys are readable by the user only.
set -eu
folder="${1:?usage: phone_link_certificates.sh <folder>}"
command -v openssl > /dev/null 2>&1 || { echo "phone_link_certificates: openssl missing" >&2; exit 2; }
mkdir -p "$folder"
chmod 700 "$folder"
umask 077

if [ ! -s "$folder/ca.crt" ] || [ ! -s "$folder/ca.key" ]; then
    openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 3650 \
        -keyout "$folder/ca.key" -out "$folder/ca.crt" \
        -subj "/CN=Lexus Head Unit local CA" \
        -addext "basicConstraints=critical,CA:TRUE" \
        -addext "keyUsage=critical,keyCertSign,cRLSign" 2> /dev/null
    rm -f "$folder/server.crt"
    echo "phone_link_certificates: new authority $folder/ca.crt (install it on the phone)"
fi

renew_seconds=2592000
if [ -s "$folder/server.crt" ] && [ -s "$folder/server.key" ] \
    && openssl x509 -in "$folder/server.crt" -noout -checkend "$renew_seconds" > /dev/null 2>&1; then
    echo "phone_link_certificates: server certificate kept"
    exit 0
fi

names="DNS:lexus-head-unit.local,DNS:localhost,IP:127.0.0.1"
host=1
while [ "$host" -le 14 ]; do
    names="$names,IP:172.20.10.$host"
    host=$((host + 1))
done
extensions="$folder/server.ext"
printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=%s\n' \
    "$names" > "$extensions"
openssl req -newkey rsa:2048 -nodes -sha256 -keyout "$folder/server.key" \
    -out "$folder/server.csr" -subj "/CN=lexus-head-unit.local" 2> /dev/null
openssl x509 -req -sha256 -days 397 -in "$folder/server.csr" \
    -CA "$folder/ca.crt" -CAkey "$folder/ca.key" -CAcreateserial \
    -out "$folder/server.crt" -extfile "$extensions" 2> /dev/null
rm -f "$folder/server.csr" "$extensions"
chmod 600 "$folder/ca.key" "$folder/server.key"
chmod 644 "$folder/ca.crt" "$folder/server.crt"
echo "phone_link_certificates: server certificate $folder/server.crt (397 days)"
