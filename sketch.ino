/*
  Arka Game 1-2 firmware — ESP32-C3 + HX711 load cell.

  Board: ESP32-C3 DevKitM-1
  Libraries:
  - ArduinoJson 7.4.2+
  - WebSockets by Markus Sattler 2.6.1+
  - HX711 Arduino Library by Bogdan Necula

  Wi-Fi and device secret are hardcoded in this file.
  The device secret must match DEVICE_SECRET_BASE64 on the backend.
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include <time.h>

#include <algorithm>
#include <cctype>
#include <memory>

namespace {

constexpr uint8_t kProtocolVersion = 1;
constexpr char kProtocolName[] = "arka-device-v1";
constexpr char kWssHost[] = "api.arrka.my.id";
constexpr uint16_t kWssPort = 443;
constexpr char kWssPath[] = "/ws/device";
constexpr char kFirmwareVersion[] = "0.2.6";
constexpr char kBuildMarker[] = "game12-boot-tare-2026-08-05";

constexpr char kWifiSsid[] = "Wokwi-GUEST";
constexpr char kWifiPassword[] = "";
constexpr char kDeviceSecretBase64[] = "REPLACE_WITH_DEVICE_SECRET_BASE64";

// ==========================================
// KONFIGURASI PIN TERBARU 
// ==========================================
constexpr uint8_t kBatteryPin   = 1; // Baterai di P01
constexpr uint8_t kBuzzerPin    = 2; // Buzzer di P02
constexpr uint8_t kHx711DoutPin = 3; // DT HX711 di P03
constexpr uint8_t kHx711SckPin  = 4; // SCK HX711 di P04

constexpr float kCalibrationFactor = 0.42f;
constexpr float kGameScaleGrams = 120000.0f;
constexpr uint32_t kTelemetryIntervalMs = 100;
constexpr uint32_t kHeartbeatDefaultMs = 5000;
constexpr uint32_t kServerStaleMs = 45000;
constexpr uint32_t kProvisioningTimeoutMs = 300000;
constexpr uint32_t kBootReprovisionWindowMs = 3000;
constexpr size_t kMaxMessageBytes = 16 * 1024;
constexpr size_t kCommandCacheSize = 16;

constexpr char kTlsCaPem[] = R"PEM(-----BEGIN CERTIFICATE-----
MIIE2jCCAsKgAwIBAgIQTr0klH4k05SALYSlL9WzGTANBgkqhkiG9w0BAQsFADAu
MQswCQYDVQQGEwJVUzENMAsGA1UEChMESVNSRzEQMA4GA1UEAxMHUm9vdCBZUjAe
Fw0yNTA5MDMwMDAwMDBaFw0yODA5MDIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYw
FAYDVQQKEw1MZXQncyBFbmNyeXB0MQwwCgYDVQQDEwNZUjIwggEiMA0GCSqGSIb3
DQEBAQUAA4IBDwAwggEKAoIBAQDZ0LxwBppqh84luqMerV/eeL/fXQ7mLQQv1Lnp
WKZbyvGpx6wh6AfnslAnF6ewTkcHA+gSOoBvm3Dfm06AuGiF+KRut4fAcowqnAQQ
CW98+QPP/eOv/wug7Iyk4NkOxf2I6g2f55T6nJoOTLFcukeRq80JGQEYan+dPFr9
OGUgQK2hGKgNkW87pappsOAuUJcroYhRt5uUis4qaZireiseu32gzDJNBAiKtsvd
6HX4v25bpkRNcS/B/Gtc9kVbUpD+2PLPxdei3Tim55k4tfAEXwD2qyiPTxrTNq6l
N+AMr5g2c1dNqkOTwjxeV6L5lpP1rGiYvLnRaPlOqyZRPW+5AgMBAAGjge4wgesw
DgYDVR0PAQH/BAQDAgGGMBMGA1UdJQQMMAoGCCsGAQUFBwMBMBIGA1UdEwEB/wQI
MAYBAf8CAQAwHQYDVR0OBBYEFEAVLSZ57TIgnt+ach3WMh+BDIEMMB8GA1UdIwQY
MBaAFN7nW2DQIm1AKH0/DQH+pLVStFGUMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEF
BQcwAoYWaHR0cDovL3lyLmkubGVuY3Iub3JnLzATBgNVHSAEDDAKMAgGBmeBDAEC
ATAnBgNVHR8EIDAeMBygGqAYhhZodHRwOi8veXIuYy5sZW5jci5vcmcvMA0GCSqG
SIb3DQEBCwUAA4ICAQB0ZUQWZ9/Yn9COEpo+JfecMnB0h0vwDm/M66IqXqw3LoaL
mx9lZvRTeDIS67PUeI3yCA2W6PKRD0/FE/G57lOmS+Xy5AaaL00ICGOqjNcCaMWW
8o8nevHOd4i4lqgtznE/28QwlcdJyF8yBiWHpnyjhEpmNWJURgOCOg2xpwRMBCsj
MScqYPtOhBeuYQvSwAEeTML2Ukh6uGuX4E14q65Ja8cdjF5bAldnP1eE4FBaAwsZ
G2fOqqrKV03Y85Nw2btedP1AtliQuJZs/Jo/gXxXdc7LrH3McgnpnbTiAncX7yES
hP6kzQejllqMCIt52HOjxDGWafS7Xw+DKwqmH+Eqy8dcbOuag/1AYlQoKNVK3F5q
Hh6tEDiMqQcLIibGKteE6iHo4A/bIScbzrhXUYuism42ZYzmc48FMVIH3qy4L84E
TdAH2gtxw0PAhvRVXp8HP7wfngpzsN/8xOTpeRSbM4+Qbc56G6+Bifmv6sk1ieQb
NA3wJdl4DDUuQSV8hBgx6zoI1ZSGORprDFux7c6rhc77QZMSRrEgomBeklervEve
86ylWmZ3WWHV6RLMi8xNvjd71r4EPIGgY7BZU/VPBkq+uA7Gb6mbJnFgV43uh3xy
LRFgxIAphIukwTGSMZZR+AI+Qnp0BYTWovHXozOf3H8r6hozEoT02JHn0AeTfA==
-----END CERTIFICATE-----
)PEM";

bool intervalElapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

bool validUuid(const String &value) {
  if (value.length() != 36) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    const bool dash = index == 8 || index == 13 || index == 18 || index == 23;
    if (dash ? value[index] != '-' : !isxdigit(static_cast<unsigned char>(value[index]))) return false;
  }
  return true;
}

bool validBase64Url(const String &value) {
  if (value.length() != 43) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    if (!(isalnum(static_cast<unsigned char>(character)) || character == '-' || character == '_')) return false;
  }
  return true;
}

bool validStandardBase64(const String &value) {
  if (value.isEmpty() || value.length() % 4 != 0) return false;
  bool padding = false;
  uint8_t paddingCount = 0;
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    if (character == '=') {
      padding = true;
      if (++paddingCount > 2 || index < value.length() - 2) return false;
      continue;
    }
    if (padding || !(isalnum(static_cast<unsigned char>(character)) || character == '+' || character == '/')) return false;
  }
  return true;
}

bool hasOnlyKeys(JsonObjectConst object, const char *const *keys, size_t count) {
  if (object.size() != count) return false;
  for (JsonPairConst pair : object) {
    bool allowed = false;
    for (size_t index = 0; index < count; ++index) {
      if (pair.key() == keys[index]) {
        allowed = true;
        break;
      }
    }
    if (!allowed) return false;
  }
  return true;
}

String randomUuid() {
  uint8_t bytes[16];
  esp_fill_random(bytes, sizeof(bytes));
  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0fU) | 0x40U);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3fU) | 0x80U);
  char output[37];
  snprintf(output, sizeof(output),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
           bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
  return String(output);
}

uint64_t epochMilliseconds() {
  timeval now{};
  gettimeofday(&now, nullptr);
  return static_cast<uint64_t>(now.tv_sec) * 1000ULL + static_cast<uint64_t>(now.tv_usec / 1000);
}

struct Provisioning {
  uint8_t deviceSecret[64]{};
  size_t deviceSecretLength = 0;

  bool valid() const {
    return deviceSecretLength >= 32 && deviceSecretLength <= sizeof(deviceSecret);
  }
  void clearSecret() {
    mbedtls_platform_zeroize(deviceSecret, sizeof(deviceSecret));
    deviceSecretLength = 0;
  }
  ~Provisioning() { clearSecret(); }
};

enum class AssociationKind : uint8_t { NONE, SETUP, SESSION };
enum class LedgerState : uint8_t { NONE, ACTIVE, CLEANED };

struct Association {
  AssociationKind kind = AssociationKind::NONE;
  String id;
  String reservationId;
  void clear() {
    kind = AssociationKind::NONE;
    id.clear();
    reservationId.clear();
  }
};

struct Ledger {
  LedgerState state = LedgerState::NONE;
  AssociationKind kind = AssociationKind::NONE;
  String id;
  String reservationId;
  String cleanupCommandId;
};

struct Command {
  String type;
  String commandId;
  AssociationKind kind = AssociationKind::NONE;
  String associationId;
  String reservationId;
};

struct CachedCommand {
  Command command;
  bool used = false;
  bool ack = false;
  String reason;
};

enum class HandshakePhase : uint8_t { IDLE, WAIT_CHALLENGE, WAIT_ACCEPT };

Preferences preferences;
WebSocketsClient webSocket;
HX711 scale;
Provisioning provisioning;
Association association;
Ledger ledger;
CachedCommand commandCache[kCommandCacheSize];
size_t commandCacheCursor = 0;
String bootId;
uint64_t outgoingSequence = 0;
uint64_t lastServerSequence = 0;
bool serverSequenceInitialized = false;
bool socketConnected = false;
bool authenticated = false;
bool sensorFault = false;
HandshakePhase handshakePhase = HandshakePhase::IDLE;
uint32_t heartbeatIntervalMs = kHeartbeatDefaultMs;
uint32_t lastHeartbeatMs = 0;
uint32_t lastServerContactMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t lastSensorLogMs = 0;
uint32_t socketConnectedAtMs = 0;
uint32_t authenticatedAtMs = 0;
uint32_t reconnectDelayMs = 3000;

// ==========================================
// VARIABEL & STATE BATERAI
// ==========================================
int batteryPercent = 100;
bool batteryCritical = false; 
unsigned long lastBatteryCheckMs = 0;
unsigned long lastBuzzerToggleMs = 0;
bool buzzerState = false;

// Nilai ADC untuk pemetaan baterai (bisa disesuaikan nanti)
const int adcBatPenuh = 2605; 
const int adcBatHabis = 1985; 

void manageBatteryAndBuzzer(uint32_t now) {
  // 1. Baca baterai setiap 5 detik
  if (now - lastBatteryCheckMs > 5000) {
    lastBatteryCheckMs = now;
    int adcValue = analogRead(kBatteryPin);
    
    // Mapping nilai ADC ke persentase
    batteryPercent = map(adcValue, adcBatHabis, adcBatPenuh, 0, 100);
    batteryPercent = constrain(batteryPercent, 0, 100);
    
    // Tandai kritis jika <= 10%
    batteryCritical = (batteryPercent <= 10);
  }

  // 2. Logika Indikator Buzzer (Non-Blocking)
  if (batteryPercent <= 10) {
    // Kritis (<= 10%): Beep cepat & alarm keras (Interval 250ms)
    if (now - lastBuzzerToggleMs > 250) {
      lastBuzzerToggleMs = now;
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(kBuzzerPin, 1500, 150); // Frekuensi tinggi
      }
    }
  } else if (batteryPercent <= 20) {
    // Peringatan (<= 20%): Beep lambat (Interval 1000ms)
    if (now - lastBuzzerToggleMs > 1000) {
      lastBuzzerToggleMs = now;
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(kBuzzerPin, 800, 200); // Frekuensi sedang
      }
    }
  }
}

bool decodeDeviceSecret(const String &encoded, Provisioning &target) {
  target.clearSecret();
  if (!validStandardBase64(encoded)) return false;
  size_t length = 0;
  const int result = mbedtls_base64_decode(
      target.deviceSecret, sizeof(target.deviceSecret), &length,
      reinterpret_cast<const unsigned char *>(encoded.c_str()), encoded.length());
  if (result != 0 || length < 32 || length > sizeof(target.deviceSecret)) {
    target.clearSecret();
    return false;
  }
  target.deviceSecretLength = length;
  return true;
}

bool initializeProvisioning() {
  return decodeDeviceSecret(String(kDeviceSecretBase64), provisioning) && provisioning.valid();
}

bool persistLedger(const Ledger &value) {
  JsonDocument document;
  document["version"] = 1;
  document["state"] = value.state == LedgerState::ACTIVE ? "ACTIVE" : "CLEANED";
  document["kind"] = value.kind == AssociationKind::SETUP ? "SETUP" : "SESSION";
  document["associationId"] = value.id;
  document["reservationId"] = value.reservationId;
  document["cleanupCommandId"] = value.cleanupCommandId;
  String encoded;
  serializeJson(document, encoded);
  if (!preferences.begin("arka-g12-state", false)) {
    Serial.println("ARKA_GAME12_LEDGER_RAM_ONLY");
    return true;
  }
  const bool saved = preferences.putString("ledger", encoded) == encoded.length();
  preferences.end();
  if (!saved) Serial.println("ARKA_GAME12_LEDGER_RAM_ONLY");
  return true;
}

bool loadLedger() {
  ledger = {};
  if (!preferences.begin("arka-g12-state", false)) {
    Serial.println("ARKA_GAME12_LEDGER_RAM_ONLY");
    return true;
  }
  const String encoded = preferences.getString("ledger", "");
  preferences.end();
  if (encoded.isEmpty()) return true;

  JsonDocument document;
  const bool decoded = !deserializeJson(document, encoded) && document.is<JsonObjectConst>();
  if (decoded) {
    JsonObjectConst input = document.as<JsonObjectConst>();
    const String state(input["state"] | "");
    const String kind(input["kind"] | "");
    ledger.state = state == "ACTIVE" ? LedgerState::ACTIVE : state == "CLEANED" ? LedgerState::CLEANED : LedgerState::NONE;
    ledger.kind = kind == "SETUP" ? AssociationKind::SETUP : kind == "SESSION" ? AssociationKind::SESSION : AssociationKind::NONE;
    ledger.id = String(input["associationId"] | "");
    ledger.reservationId = String(input["reservationId"] | "");
    ledger.cleanupCommandId = String(input["cleanupCommandId"] | "");
    if (ledger.state != LedgerState::NONE && ledger.kind != AssociationKind::NONE &&
        validUuid(ledger.id) && validUuid(ledger.reservationId)) return true;
  }

  ledger = {};
  if (preferences.begin("arka-g12-state", false)) {
    preferences.remove("ledger");
    preferences.end();
    Serial.println("ARKA_GAME12_LEDGER_RESET");
  } else {
    Serial.println("ARKA_GAME12_LEDGER_RAM_ONLY");
  }
  return true;
}

String base64Url(const uint8_t *bytes, size_t length) {
  const size_t capacity = 4 * ((length + 2) / 3) + 1;
  std::unique_ptr<unsigned char[]> encoded(new unsigned char[capacity]);
  size_t encodedLength = 0;
  if (!encoded || mbedtls_base64_encode(encoded.get(), capacity, &encodedLength, bytes, length) != 0) return String();
  String result;
  for (size_t index = 0; index < encodedLength; ++index) {
    const char character = static_cast<char>(encoded[index]);
    if (character == '+') result += '-';
    else if (character == '/') result += '_';
    else if (character != '=') result += character;
  }
  return result;
}

String challengeProof(const String &challengeId, const String &nonce) {
  const String material = String(kProtocolName) + "\n" + challengeId + "\n" + nonce + "\n" + bootId;
  uint8_t digest[32];
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info || mbedtls_md_hmac(info, provisioning.deviceSecret, provisioning.deviceSecretLength,
                               reinterpret_cast<const unsigned char *>(material.c_str()), material.length(), digest) != 0) {
    return String();
  }
  const String proof = base64Url(digest, sizeof(digest));
  mbedtls_platform_zeroize(digest, sizeof(digest));
  return proof;
}

void addEnvelope(JsonDocument &document, const char *type, uint64_t sequence) {
  document["protocolVersion"] = kProtocolVersion;
  document["type"] = type;
  document["messageId"] = randomUuid();
  document["sentAtMs"] = epochMilliseconds();
  document["sequence"] = sequence;
}

bool sendDocument(JsonDocument &document) {
  if (!socketConnected) return false;
  String encoded;
  serializeJson(document, encoded);
  return encoded.length() <= kMaxMessageBytes && webSocket.sendTXT(encoded);
}

void sendHello() {
  JsonDocument document;
  addEnvelope(document, "device.hello", 0);
  document["bootId"] = bootId;
  document["payload"]["firmwareVersion"] = kFirmwareVersion;
  JsonArray capabilities = document["payload"]["capabilities"].to<JsonArray>();
  capabilities.add("FSR_10HZ");
  capabilities.add("FSR_TARED_ON_SETUP_BIND");
  handshakePhase = sendDocument(document) ? HandshakePhase::WAIT_CHALLENGE : HandshakePhase::IDLE;
}

void sendProof(const String &challengeId, const String &nonce) {
  const String proof = challengeProof(challengeId, nonce);
  if (proof.isEmpty()) {
    webSocket.disconnect();
    return;
  }
  JsonDocument document;
  addEnvelope(document, "device.prove", 0);
  document["payload"]["challengeId"] = challengeId;
  document["payload"]["proof"] = proof;
  handshakePhase = sendDocument(document) ? HandshakePhase::WAIT_ACCEPT : HandshakePhase::IDLE;
}

void addAssociation(JsonDocument &document, AssociationKind kind, const String &id) {
  if (kind == AssociationKind::SETUP) document["setupId"] = id;
  if (kind == AssociationKind::SESSION) document["sessionId"] = id;
}

bool sendHealth(const char *type) {
  if (!authenticated) return false;
  const uint64_t sequence = outgoingSequence + 1;
  JsonDocument document;
  addEnvelope(document, type, sequence);
  document["payload"]["battery"]["valid"] = true;
  document["payload"]["battery"]["percent"] = batteryPercent; // Dikirim persentase real
  JsonArray faults = document["payload"]["faults"].to<JsonArray>();
  if (sensorFault) faults.add("FSR");
  if (batteryCritical) faults.add("BATTERY_CRITICAL");
  if (!sendDocument(document)) return false;
  outgoingSequence = sequence;
  lastHeartbeatMs = millis();
  return true;
}

bool sendAck(const Command &command, bool ack, const String &reason = String()) {
  if (!authenticated) return false;
  const uint64_t sequence = outgoingSequence + 1;
  JsonDocument document;
  addEnvelope(document, "device.commandAck", sequence);
  addAssociation(document, command.kind, command.associationId);
  document["payload"]["commandId"] = command.commandId;
  document["payload"]["outcome"] = ack ? "ACK" : "NACK";
  if (!ack) document["payload"]["reason"] = reason;
  if (!sendDocument(document)) return false;
  outgoingSequence = sequence;
  return true;
}

int scaledFsrRaw(float grams) {
  if (!isfinite(grams)) return 0;
  return constrain(static_cast<int>(lroundf(std::max(0.0f, grams) * 4095.0f / kGameScaleGrams)), 0, 4095);
}

void sendTelemetry(uint32_t now) {
  // Jika baterai <= 10%, blokir pengiriman telemetry (tidak bisa main)
  if (batteryCritical) return;

  if (!authenticated || association.kind == AssociationKind::NONE || sensorFault ||
      !intervalElapsed(now, lastTelemetryMs, kTelemetryIntervalMs) || !scale.is_ready()) return;
  lastTelemetryMs = now;
  const float grams = std::max(0.0f, scale.get_units(1));
  const int fsrRaw = scaledFsrRaw(grams);
  if (intervalElapsed(now, lastSensorLogMs, 1000)) {
    lastSensorLogMs = now;
    Serial.print("ARKA_GAME12_SENSOR grams=");
    Serial.print(grams, 1);
    Serial.print(" fsr_raw=");
    Serial.println(fsrRaw);
  }
  const uint64_t sequence = outgoingSequence + 1;
  JsonDocument document;
  addEnvelope(document, "telemetry.fsr", sequence);
  addAssociation(document, association.kind, association.id);
  document["payload"]["fsrRaw"] = fsrRaw;
  if (sendDocument(document)) outgoingSequence = sequence;
}

CachedCommand *findCached(const String &commandId) {
  for (auto &cached : commandCache) if (cached.used && cached.command.commandId == commandId) return &cached;
  return nullptr;
}

bool sameCommand(const Command &left, const Command &right) {
  return left.type == right.type && left.commandId == right.commandId && left.kind == right.kind &&
         left.associationId == right.associationId && left.reservationId == right.reservationId;
}

void rememberCommand(const Command &command, bool ack, const String &reason) {
  commandCache[commandCacheCursor] = {command, true, ack, reason};
  commandCacheCursor = (commandCacheCursor + 1) % kCommandCacheSize;
}

void finishCommand(const Command &command, bool ack, const String &reason = String()) {
  rememberCommand(command, ack, reason);
  sendAck(command, ack, reason);
  Serial.print("ARKA_GAME12_COMMAND type=");
  Serial.print(command.type);
  Serial.print(" outcome=");
  Serial.println(ack ? "ACK" : "NACK");
}

bool commandIdentity(const String &type, JsonObjectConst payload, Command &command) {
  const bool setup = type.startsWith("setup.");
  const bool session = type.startsWith("session.") || type == "device.feedback";
  if (setup == session) return false;
  command.type = type;
  command.commandId = String(payload["commandId"] | "");
  command.kind = setup ? AssociationKind::SETUP : AssociationKind::SESSION;
  command.associationId = String(payload[setup ? "setupId" : "sessionId"] | "");
  command.reservationId = String(payload["reservationId"] | "");
  return validUuid(command.commandId) && validUuid(command.associationId) &&
         (type == "device.feedback" || validUuid(command.reservationId));
}

bool activeLedgerMatches(const Command &command) {
  return ledger.state == LedgerState::ACTIVE && ledger.kind == command.kind && ledger.id == command.associationId &&
         ledger.reservationId == command.reservationId;
}

void restoreAssociation(const Command &command) {
  association.kind = command.kind;
  association.id = command.associationId;
  association.reservationId = command.reservationId;
}

void handleCommand(JsonObjectConst envelope) {
  const String type(envelope["type"].as<const char *>());
  const uint64_t serverSequence = envelope["sequence"].as<uint64_t>();
  JsonObjectConst payload = envelope["payload"].as<JsonObjectConst>();
  Command command;
  if (!commandIdentity(type, payload, command)) return;

  if (CachedCommand *cached = findCached(command.commandId)) {
    if (!sameCommand(cached->command, command)) {
      sendAck(command, false, "INVALID_COMMAND");
      return;
    }
    if (cached->ack && (type == "setup.bind" || type == "session.bind") &&
        activeLedgerMatches(command) && association.kind == AssociationKind::NONE) restoreAssociation(command);
    sendAck(cached->command, cached->ack, cached->reason);
    return;
  }

  if (serverSequenceInitialized &&
      (serverSequence <= lastServerSequence || serverSequence - lastServerSequence > 32)) {
    finishCommand(command, false, "INVALID_COMMAND");
    return;
  }
  lastServerSequence = serverSequence;
  serverSequenceInitialized = true;

  if (type == "setup.bind" || type == "session.bind") {
    // Blokir pengikatan sesi baru jika baterai <= 10%
    if (batteryCritical) {
      finishCommand(command, false, "BATTERY_CRITICAL");
      return;
    }

    if (activeLedgerMatches(command)) {
      restoreAssociation(command);
      finishCommand(command, true);
      return;
    }
    if (association.kind != AssociationKind::NONE) {
      finishCommand(command, false, "BUSY");
      return;
    }
    if (ledger.state == LedgerState::ACTIVE) {
      Serial.print("ARKA_GAME12_STALE_LEDGER_REPLACED oldAssociationId=");
      Serial.println(ledger.id);
    }
    if (type == "setup.bind" && !scale.is_ready()) {
      finishCommand(command, false, "FAULT");
      return;
    }
    lastSensorLogMs = 0;
    Ledger next{LedgerState::ACTIVE, command.kind, command.associationId, command.reservationId, String()};
    if (!persistLedger(next)) {
      finishCommand(command, false, "FAULT");
      return;
    }
    ledger = next;
    restoreAssociation(command);
    finishCommand(command, true);
    return;
  }

  if (type == "setup.unbind" || type == "session.unbind") {
    bool exact = ledger.kind == command.kind && ledger.id == command.associationId &&
                 ledger.reservationId == command.reservationId;
    const bool replay = exact && ledger.state == LedgerState::CLEANED && ledger.cleanupCommandId == command.commandId;
    if (!exact && association.kind == AssociationKind::NONE && ledger.state == LedgerState::ACTIVE) {
      exact = true;
      Serial.print("ARKA_GAME12_STALE_CLEANUP_ACCEPTED oldAssociationId=");
      Serial.println(ledger.id);
    }
    if (!exact || (ledger.state == LedgerState::CLEANED && !replay)) {
      finishCommand(command, false, "INVALID_ASSOCIATION");
      return;
    }
    if (ledger.state == LedgerState::ACTIVE) {
      association.clear();
      Ledger cleaned{LedgerState::CLEANED, command.kind, command.associationId, command.reservationId, command.commandId};
      if (!persistLedger(cleaned)) {
        finishCommand(command, false, "FAULT");
        return;
      }
      ledger = cleaned;
    }
    finishCommand(command, true);
    return;
  }

  if (type == "device.feedback") {
    if (association.kind != AssociationKind::SESSION || association.id != command.associationId) {
      finishCommand(command, false, "INVALID_ASSOCIATION");
      return;
    }
    const String action(payload["action"] | "");
    if (action != "HARD_STOP") {
      finishCommand(command, false, "UNSUPPORTED");
      return;
    }
    finishCommand(command, true);
    return;
  }

  finishCommand(command, false, "UNSUPPORTED");
}

bool strictServerEnvelope(JsonObjectConst input) {
  const char *const keys[] = {"protocolVersion", "type", "messageId", "sentAtMs", "sequence", "payload"};
  return hasOnlyKeys(input, keys, 6) && (input["protocolVersion"] | 0) == kProtocolVersion &&
         input["type"].is<const char *>() && validUuid(String(input["messageId"] | "")) &&
         input["sentAtMs"].is<uint64_t>() && input["sequence"].is<uint64_t>() && input["payload"].is<JsonObjectConst>();
}

void handleChallenge(JsonObjectConst input) {
  if (handshakePhase != HandshakePhase::WAIT_CHALLENGE || String(input["type"] | "") != "device.challenge") {
    webSocket.disconnect();
    return;
  }
  JsonObjectConst payload = input["payload"].as<JsonObjectConst>();
  const String challengeId(payload["challengeId"] | "");
  const String nonce(payload["nonce"] | "");
  if (!validUuid(challengeId) || !validBase64Url(nonce)) {
    webSocket.disconnect();
    return;
  }
  sendProof(challengeId, nonce);
}

void handleAccept(JsonObjectConst input) {
  if (handshakePhase != HandshakePhase::WAIT_ACCEPT || String(input["type"] | "") != "device.accept") {
    webSocket.disconnect();
    return;
  }
  JsonObjectConst payload = input["payload"].as<JsonObjectConst>();
  if (!validUuid(String(payload["connectionId"] | "")) || (payload["maxSequenceGap"] | 0) != 32) {
    webSocket.disconnect();
    return;
  }
  heartbeatIntervalMs = payload["heartbeatIntervalMs"] | kHeartbeatDefaultMs;
  authenticated = true;
  handshakePhase = HandshakePhase::IDLE;
  reconnectDelayMs = 3000;
  authenticatedAtMs = millis();
  lastServerContactMs = authenticatedAtMs;
  sendHealth("device.status");
  Serial.println("ARKA_GAME12_AUTHENTICATED");
}

void handleText(uint8_t *payload, size_t length) {
  if (length == 0 || length > kMaxMessageBytes) {
    webSocket.disconnect();
    return;
  }
  JsonDocument document;
  if (deserializeJson(document, payload, length) || !document.is<JsonObjectConst>()) {
    webSocket.disconnect();
    return;
  }
  JsonObjectConst input = document.as<JsonObjectConst>();
  if (!strictServerEnvelope(input)) {
    webSocket.disconnect();
    return;
  }
  lastServerContactMs = millis();
  if (!authenticated) {
    const String type(input["type"] | "");
    if (type == "device.challenge") handleChallenge(input);
    else if (type == "device.accept") handleAccept(input);
    else webSocket.disconnect();
    return;
  }
  handleCommand(input);
}

void onWebSocketEvent(WStype_t event, uint8_t *payload, size_t length) {
  if (event == WStype_CONNECTED) {
    socketConnected = true;
    socketConnectedAtMs = millis();
    authenticatedAtMs = 0;
    authenticated = false;
    handshakePhase = HandshakePhase::IDLE;
    lastServerSequence = 0;
    serverSequenceInitialized = false;
    sendHello();
    Serial.println("ARKA_GAME12_WSS_CONNECTED");
  } else if (event == WStype_TEXT) {
    handleText(payload, length);
  } else if (event == WStype_PING || event == WStype_PONG) {
    lastServerContactMs = millis();
  } else if (event == WStype_DISCONNECTED || event == WStype_ERROR) {
    const uint32_t now = millis();
    Serial.print("ARKA_GAME12_WSS_DISCONNECTED event=");
    Serial.print(event == WStype_ERROR ? "ERROR" : "CLOSED");
    Serial.print(" connected_ms=");
    Serial.print(static_cast<uint32_t>(now - socketConnectedAtMs));
    Serial.print(" authenticated_ms=");
    Serial.print(authenticatedAtMs == 0 ? 0 : static_cast<uint32_t>(now - authenticatedAtMs));
    Serial.print(" server_silent_ms=");
    Serial.print(static_cast<uint32_t>(now - lastServerContactMs));
    Serial.print(" wifi_status=");
    Serial.println(static_cast<int>(WiFi.status()));
    socketConnected = false;
    authenticated = false;
    handshakePhase = HandshakePhase::IDLE;
    association.clear();
    reconnectDelayMs = std::min<uint32_t>(reconnectDelayMs * 2, 60000);
    webSocket.setReconnectInterval(reconnectDelayMs);
  }
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("ARKA_GAME12_WIFI_CONNECTING");
    WiFi.begin(kWifiSsid, kWifiPassword, 6);
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && !intervalElapsed(millis(), started, 30000)) delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      delay(1000);
    }
  }
}

void synchronizeClock() {
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  while (time(nullptr) <= 1700000000) delay(100);
}

void initializeScale() {
  scale.begin(kHx711DoutPin, kHx711SckPin);
  const uint32_t started = millis();
  while (!scale.is_ready() && !intervalElapsed(millis(), started, 5000)) delay(10);
  sensorFault = !scale.is_ready();
  if (!sensorFault) {
    scale.set_scale(kCalibrationFactor);
    scale.tare(10);
    Serial.println("ARKA_GAME12_HX711_READY");
  } else {
    Serial.println("ARKA_GAME12_HX711_FAULT");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("ARKA_GAME12_STARTING");
  Serial.print("ARKA_GAME12_BUILD=");
  Serial.println(kBuildMarker);
  
  pinMode(kBatteryPin, INPUT);
  pinMode(kBuzzerPin, OUTPUT);
  
  initializeScale();

  if (!initializeProvisioning()) {
    Serial.println("ARKA_GAME12_CONFIGURATION_INVALID");
    delay(1000);
    ESP.restart();
  }
  loadLedger();

  bootId = randomUuid();
  connectWifi();
  synchronizeClock();
  Serial.println("ARKA_GAME12_NETWORK_READY");
  webSocket.onEvent(onWebSocketEvent);
  webSocket.setReconnectInterval(reconnectDelayMs);
  webSocket.enableHeartbeat(kHeartbeatDefaultMs, 10000, 3);
  webSocket.beginSslWithCA(kWssHost, kWssPort, kWssPath, kTlsCaPem, kProtocolName);
}

void loop() {
  webSocket.loop();
  const uint32_t now = millis();
  
  // Eksekusi fungsi pantau baterai & bunyi buzzer
  manageBatteryAndBuzzer(now);

  if (WiFi.status() != WL_CONNECTED || (authenticated && intervalElapsed(now, lastServerContactMs, kServerStaleMs))) {
    if (socketConnected) webSocket.disconnect();
    socketConnected = false;
    authenticated = false;
    association.clear();
  }

  if (authenticated && intervalElapsed(now, lastHeartbeatMs, heartbeatIntervalMs)) sendHealth("device.heartbeat");
  sendTelemetry(now);
  yield();
}
