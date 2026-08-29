#!/usr/bin/env python3
"""Deterministic AP/netif/listener/service-loop contract for X4 Pro hotspot."""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
activity = (root / "src/activities/network/CrossPointWebServerActivity.cpp").read_text()
server_h = (root / "src/network/CrossPointWebServer.h").read_text()
server_cpp = (root / "src/network/CrossPointWebServer.cpp").read_text()

# DHCP and the AP interface have one explicit, stable IPv4 contract. The old
# fixed delay could advance with a zero/wrong AP address while association was
# already available.
for declaration in (
    "AP_IP(192, 168, 4, 1)",
    "AP_GATEWAY(192, 168, 4, 1)",
    "AP_SUBNET(255, 255, 255, 0)",
    "AP_DHCP_START(192, 168, 4, 2)",
):
    assert declaration in activity, declaration
assert activity.index("WiFi.softAPConfig") < activity.index("WiFi.softAP(AP_SSID")
assert "WiFi.softAPIP() != AP_IP" in activity
assert "apIP != AP_IP" in activity

# The primary port must bind to the verified interface and begin before mDNS
# and captive-DNS helpers. A void WebServer::begin() may otherwise be reported
# as success even when NetworkServer failed to bind/listen.
assert "class CheckedWebServer final : public WebServer" in server_h
assert "static_cast<bool>(_server)" in server_h
assert "new CheckedWebServer(listenAddress, port)" in server_cpp
assert "if (!server->isListening())" in server_cpp
assert server_cpp.index("if (!server->isListening())") < server_cpp.index("running = true")
ap_start = activity.index("void CrossPointWebServerActivity::startAccessPoint()")
ap_end = activity.index("void CrossPointWebServerActivity::startWebServer()")
ap_body = activity[ap_start:ap_end]
assert ap_body.index("startWebServer();") < ap_body.index("restartMdns")
assert ap_body.index("startWebServer();") < ap_body.index("dnsServer->start")

# Foreground request handling is bounded and yields every slice; DHCP/lwIP and
# DNS are not left behind a 500-accept spin.
batch = re.search(r"constexpr int MAX_ITERATIONS = (\d+);", activity)
assert batch and int(batch.group(1)) <= 32
loop_slice = activity[activity.index("constexpr int MAX_ITERATIONS"):]
assert loop_slice.index("yield();") < loop_slice.index("lastHandleClientTime = millis()")

# AP-mode QR and primary text use the direct address. mDNS remains a secondary
# display-only fallback and is not encoded in the QR.
assert 'const std::string directUrl = std::string("http://") + connectedIP + "/";' in activity
assert "QrUtils::drawQrCode(renderer, qrBoundsUrl, directUrl);" in activity
assert "hostnameFallback.c_str()" in activity

print("PASS hotspot AP/DHCP readiness, port-80 listener, scheduling, direct-IP QR contract")
