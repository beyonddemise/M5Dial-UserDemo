/**
 * @file secrets.example.h
 * @brief Template for the UserDemo sketch's Wi-Fi credentials (used by the factory test).
 *
 * Optional: copy to secrets.h (git-ignored, see sketch/.gitignore) and fill in your values:
 *   cp UserDemo/secrets.example.h UserDemo/secrets.h
 * M5DialDemo's config.h includes secrets.h if present and otherwise uses placeholders.
 * Never commit secrets.h.
 */
#pragma once

/** SSID of the Wi-Fi network the factory test joins. */
#define FACTORY_TEST_WIFI_SSID "your-ssid"
/** Password of that network. */
#define FACTORY_TEST_WIFI_PASSWORD "your-password"
