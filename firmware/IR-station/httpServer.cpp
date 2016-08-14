#include "httpServer.h"

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <FS.h>
#include "config.h"
#include "file.h"
#include "station.h"
#include "wifi.h"
#include "led.h"

// TCP server at port 80 will respond to HTTP requests
ESP8266WebServer server(80);

#if USE_CAPITAL_PORTAL == true
// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif

void serverTask() {
  server.handleClient();
#if USE_CAPITAL_PORTAL == true
  if (station.mode == IR_STATION_MODE_NULL) dnsServer.processNextRequest();
#endif
}

void dispRequest() {
  println_dbg("");
  println_dbg("New Request");
  println_dbg("URI: " + server.uri());
  println_dbg(String("Method: ") + ((server.method() == HTTP_GET) ? "GET" : "POST"));
  println_dbg("Arguments: " + String(server.args()));
  for (uint8_t i = 0; i < server.args(); i++) {
    println_dbg("  " + server.argName(i) + " = " + server.arg(i));
  }
}

void setupFormServer(void) {
#if USE_CAPITAL_PORTAL == true
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
#endif

  server.on("/wifi-list", []() {
    dispRequest();
    int n = WiFi.scanNetworks();
    String res = "[";
    for (int i = 0; i < n; ++i) {
      res += "\"" + WiFi.SSID(i) + "\"";
      if (i != n - 1) res += ",";
    }
    res += "]";
    server.send(200, "text/json", res);
    println_dbg("End");
  });
  server.on("/confirm", []() {
    dispRequest();
    station.ssid = server.arg("ssid");
    station.password = server.arg("password");
    station.stealth = server.arg("stealth") == "true";
    station.hostname = server.arg("url");
    if (station.hostname == "") {
      station.hostname = HOSTNAME_DEFAULT;
    }
    println_dbg("Target SSID: " + station.ssid);
    println_dbg("Target Password: " + station.password);
    println_dbg("mDNS Address: " + station.hostname);
    indicator.set(0, 1023, 0);
    server.send(200);
    WiFi.disconnect(true);
    delay(1000);
    WiFi.begin(station.ssid.c_str(), station.password.c_str());
  });
  server.on("/isConnected", []() {
    dispRequest();
    if (WiFi.status() == WL_CONNECTED) {
      String res = (String)WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
      server.send(200, "text/palin", res);
      station.setMode(IR_STATION_MODE_STA);
      indicator.set(0, 0, 1023);
    } else {
      println_dbg("Not connected yet.");
      server.send(200, "text/plain", "false");
      println_dbg("End");
    }
  });
  server.on("/reboot", []() {
    server.send(200);
    delay(100);
    ESP.reset();
  });
  server.on("/set-ap-mode", []() {
    dispRequest();
    station.hostname = server.arg("url");
    if (station.hostname == "") {
      station.hostname = HOSTNAME_DEFAULT;
    }
    server.send(200, "text/plain", "Setting up Access Point Successful");
    station.setMode(IR_STATION_MODE_AP);
    ESP.reset();
  });
  server.onNotFound([]() {
    // Request detail
    dispRequest();
    println_dbg("Redirect");
    String res = "<script>location.href = \"http://" + (String)WiFi.softAPIP()[0] + "." + WiFi.softAPIP()[1] + "." + WiFi.softAPIP()[2] + "." + WiFi.softAPIP()[3] + "/\";</script>";
    server.send(200, "text/html", res);
    println_dbg("End");
  });

  server.serveStatic("/", SPIFFS, "/form/");

  // Start TCP (HTTP) server
  server.begin();
  println_dbg("Form Server Listening");
}

void setupServer(void) {
  // Set up mDNS responder:
  if (station.hostname == "") station.hostname = HOSTNAME_DEFAULT;
  print_dbg("mDNS address: ");
  println_dbg("http://" + station.hostname + ".local");
  if (!MDNS.begin(station.hostname.c_str())) {
    println_dbg("Error setting up MDNS responder!");
  } else {
    println_dbg("mDNS responder started");
  }

  server.on("/name-list", []() {
    dispRequest();
    String res = "";
    res += "[";
    for (uint8_t ch = 0; ch < IR_CH_SIZE; ch++) {
      res += "\"" + station.chName[ch] + "\"";
      if (ch != IR_CH_SIZE - 1) res += ",";
    }
    res += "]";
    server.send(200, "text/json", res);
    println_dbg("End");
  });
  server.on("/send", []() {
    dispRequest();
    String res;
    uint8_t ch = server.arg("ch").toInt();
    ch -= 1; // display: 1 ch ~ IR_CH_SIZE ch but data: 0 ch ~ (IR_CH_NAME - 1) ch so 1 decriment
    if (0 <= ch  && ch < IR_CH_SIZE) {
      if (station.irSendSignal(ch)) {
        res = "Sent ch " + String(ch + 1, DEC) + " (" + station.chName[ch] + ")";
      } else {
        res = "No signal was sent";
      }
    } else {
      res = "Invalid channel selected. Sending failed";
    }
    server.send(200, "text/plain", res);
    println_dbg("End");
  });
  server.on("/recode", []() {
    dispRequest();
    String status;
    uint8_t ch = server.arg("ch").toInt();
    ch -= 1; // display: 1 ch ~ IR_CH_SIZE ch but data: 0 ch ~ (IR_CH_NAME - 1) ch so 1 decriment
    if (0 <= ch  && ch < IR_CH_SIZE) {
      String name = server.arg("name");
      if (station.irRecodeSignal(ch, name)) {
        status = "Recoding Successful: ch " + String(ch + 1);
      } else {
        status = "No Signal Recieved";
      }
    } else {
      status = "Invalid Request";
    }
    server.send(200, "text/plain", status);
    println_dbg("End");
  });
  server.on("/rename", []() {
    dispRequest();
    String status;
    uint8_t ch = server.arg("ch").toInt();
    ch -= 1; // display: 1 ch ~ IR_CH_SIZE ch but data: 0 ch ~ (IR_CH_NAME - 1) ch so 1 decriment
    if (0 <= ch  && ch < IR_CH_SIZE) {
      if (station.renameSignal(ch, server.arg("name"))) {
        status = "Rename successful";
      } else {
        status = "Rename Failed";
      }
    } else {
      status = "Invalid Request";
    }
    server.send(200, "text/plain", status);
    println_dbg("End");
  });
  server.on("/upload", []() {
    // Request detail
    dispRequest();
    String status;
    // Match the request
    uint8_t ch = server.arg("ch").toInt();
    ch -= 1; // display: 1 ch ~ IR_CH_SIZE ch but data: 0 ch ~ (IR_CH_NAME - 1) ch so 1 decriment
    if (0 <= ch  && ch < IR_CH_SIZE) {
      if (station.uploadSignal(ch, server.arg("name"), server.arg("irJson"))) {
        status = "Upload successful";
      } else {
        status = "Upload Failed";
      }
    } else {
      status = "Invalid Request";
    }
    server.send(200, "text/plain", status);
    println_dbg("End");
  });
  server.on("/clear", []() {
    dispRequest();
    String status;
    uint8_t ch = server.arg("ch").toInt();
    ch -= 1; // display: 1 ch ~ IR_CH_SIZE ch but data: 0 ch ~ (IR_CH_NAME - 1) ch so 1 decriment
    if (0 <= ch  && ch < IR_CH_SIZE) {
      if (station.clearSignal(ch)) {
        status = "Cleared Signal";
      } else {
        status = "Clean Failed";
      }
    } else {
      status = "Invalid Request";
    }
    server.send(200, "text/plain", status);
    println_dbg("End");
  });
  server.on("/clear-all", []() {
    dispRequest();
    station.clearSignals();
    server.send(200, "text/plain", "Cleared All Signals");
    println_dbg("End");
  });
  server.on("/disconnect-wifi", []() {
    dispRequest();
    server.send(200, "text/plain", "Disconnected this WiFi, Please connect again");
    println_dbg("Change WiFi");
    delay(1000);
    station.reset();  // automatically rebooted
  });
  server.on("/info", []() {
    dispRequest();
    String res = "";
    res += "[\"";
    res += "Listening...";
    res += "\",\"";
    if (station.mode == IR_STATION_MODE_STA)res += WiFi.SSID();
    else res += SOFTAP_SSID;
    res += "\",\"";
    if (station.mode == IR_STATION_MODE_STA) res += (String)WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
    else res += (String)WiFi.softAPIP()[0] + "." + WiFi.softAPIP()[1] + "." + WiFi.softAPIP()[2] + "." + WiFi.softAPIP()[3];
    res += "\",\"";
    res += "http://" + station.hostname + ".local";
    res += "\"]";
    server.send(200, "text/json", res);
    println_dbg("End");
  });
  server.onNotFound([]() {
    dispRequest();
    println_dbg("Redirect");
    String res = "<script>location.href = \"http://" + (String)WiFi.softAPIP()[0] + "." + WiFi.softAPIP()[1] + "." + WiFi.softAPIP()[2] + "." + WiFi.softAPIP()[3] + "/\";</script>";
    server.send(200, "text/html", res);
    println_dbg("End");
  });

  server.serveStatic("/", SPIFFS, "/general/", "public");

  // Start TCP (HTTP) server
  server.begin();
  println_dbg("IR Station Server Listening");
}
