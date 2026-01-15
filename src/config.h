// Greenhouse configuration
#ifndef CONFIG_H
#define CONFIG_H

// WiFi (replaced with provided network credentials)
#define WIFI_SSID "Redmi 9T"
#define WIFI_PASS "murcia09"

// Static IP configuration (adjust as needed)
// Format: IPAddress(a,b,c,d)
#define STATIC_IP IPAddress(192,168,1,50)
#define STATIC_GATEWAY IPAddress(192,168,1,1)
#define STATIC_SUBNET IPAddress(255,255,255,0)
#define STATIC_DNS IPAddress(8,8,8,8)

// Relay logic: set to true if relay is active LOW (typical relay boards)
#define RELAY_ACTIVE_LOW true

#endif // CONFIG_H
