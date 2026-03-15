# Receiver_ESP826 – Empfänger für Laser-Entfernungsmesser (ESP8266 + OLED)

Dieses Projekt realisiert einen **Empfänger** für einen laserbasierten Entfernungsmesser.  
Die Hardware basiert auf einem **ESP8266 NodeMCU** und einem **1,3" I2C-OLED-Display** (SSD1306).  
Der Empfänger verbindet sich mit dem Access Point des Laser‑Senders (ebenfalls ein ESP8266), fragt periodisch die aktuelle Entfernung per HTTP‑JSON ab und zeigt diese in Metern auf dem Display an.

Entwickelt von **Amir Mobasheraghdam**  

🐙 [https://github.com/Amirmobash](https://github.com/Amirmobash)

---

## 📦 Benötigte Komponenten

- 1× ESP8266 NodeMCU (ESP-12E)
- 1× 1,3" I2C‑OLED‑Display (SSD1306, 128×64 Pixel)
- 1× optional: Taster für spätere Erweiterungen (z. B. „Mark“-Funktion)
- Breadboard und Jumper-Kabel

---

## 🔌 Verdrahtung (I2C)

| OLED Pin | ESP8266 NodeMCU | GPIO  |
|----------|------------------|-------|
| VCC      | 3,3 V            | –     |
| GND      | GND              | –     |
| SDA      | D2               | GPIO4 |
| SCL      | D1               | GPIO5 |

**Optionaler Taster:**  
Zwischen D3 (GPIO0) und GND – intern mit `INPUT_PULLUP` beschaltet.

---

## 🛠️ Einrichtung der Arduino-IDE

1. **Board‑Paket für ESP8266** installieren  
   *Datei → Voreinstellungen → Zusätzliche Boardverwalter-URLs:*  
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`

2. **Benötigte Bibliotheken** (über Bibliotheksverwalter installieren):
   - `Adafruit GFX Library` von Adafruit
   - `Adafruit SSD1306` von Adafruit
   - (Wire ist bereits in der ESP8266‑Core enthalten)

3. **Board auswählen:**  
   `NodeMCU 1.0 (ESP-12E)` (oder ein passendes Board)

---

## ⚙️ Konfiguration

Im Sketch müssen die Zugangsdaten des Laser‑Senders eingetragen werden:

```cpp
const char* WIFI_SSID   = "LaserESP8266";   // SSID des Access Points vom Sender
const char* WIFI_PASS   = "";                // Passwort (leer, falls keins)
const char* LASER_HOST  = "192.168.4.1";     // IP des Senders (Standard)
const uint16_t LASER_PORT = 80;
```

Der Laser‑Sender muss unter dieser IP einen HTTP‑Endpunkt `/json` bereitstellen, der ein JSON‑Objekt mit dem Feld `"mm"` liefert, z. B.:

```json
{"mm":12345, "status":"OK"}
```

---

## 🚀 Funktionsweise

1. **Boot‑Logo**  
   Nach dem Start erscheint für 6 Sekunden ein Begrüßungsbildschirm mit dem Namen des Autors und dem Projekttitel.

2. **Verbindungsaufbau**  
   Danach wechselt die Anzeige zu „CONNECTING…“. Der ESP8266 versucht, sich mit dem WLAN des Laser‑Senders zu verbinden.

3. **Messbetrieb**  
   Sobald die Verbindung steht, wird alle 500 ms die Entfernung vom Sender abgefragt.  
   Der erhaltene Wert (in Millimetern) wird auf dem Display in Metern mit drei Nachkommastellen dargestellt (z. B. `12.345 m`).

4. **Fehlerbehandlung**  
   Falls für mehr als 5 Sekunden keine gültigen Daten empfangen werden, kehrt das Gerät automatisch in den „CONNECTING“‑Zustand zurück und versucht erneut, eine Verbindung herzustellen.

---

## 🔮 Erweiterungsmöglichkeiten

- **Mark‑Taster**  
  Der Sketch enthält bereits vorbereiteten Code für einen Taster an D3. Damit könnte später ein HTTP‑Befehl (`/mark`) an den Sender gesendet werden, um z. B. eine Referenzmessung zu speichern.

- **Anpassung der Polling‑Rate**  
  Über die Konstante `LASER_POLL_INTERVAL_MS` kann die Aktualisierungsfrequenz geändert werden.

---

## 📝 Versionshistorie

- **v1.0** – Grundlegende Funktion: Verbindung, Abfrage, Anzeige

---

## 📄 Lizenz

Dieses Projekt ist unter der MIT‑Lizenz veröffentlicht.  
Weitere Informationen finden Sie in der Datei `LICENSE`.

---

**Viel Erfolg beim Nachbau!**  
Bei Fragen oder Anregungen können Sie mich gerne über [GitHub](https://github.com/Amirmobash) kontaktieren.
