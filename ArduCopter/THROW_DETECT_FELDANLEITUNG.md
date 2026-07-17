# Throw-Erkennung im Feld testen — Anleitung (ohne Fachchinesisch)

**Ziel:** Sechs verschiedene „Wann gehen die Motoren an?“-Varianten am Katapult
vergleichen, **ohne** bei jedem Wurf nachdenken zu müssen.

**Du brauchst:**

- Laptop mit **Mission Planner** (Windows ist üblich)
- USB-Kabel zur Flugsteuerung (Autopilot)
- Die Firmware von Branch  
  `fabian/throw-detect-variants`  
  (Fork: https://github.com/jonenfabian/ardupilot/tree/fabian/throw-detect-variants)
- Freies Testgelände, Not-Disarm, übliche Sicherheit

---

## 0. Kurz: Was muss auf die Drohne?

Die neuen Einstellungen (`THROW_DETECT` usw.) gibt es **nur** in **dieser**
Firmware. Die Standard-Firmware aus dem Internet kennt sie **nicht**.

Ablauf in einem Satz:

1. Firmware-Datei bauen (`.apj`)  
2. Mit Mission Planner **auf die Flugsteuerung laden** („flashen“)  
3. Verbinden → Parameter setzen → fliegen  

---

## 1. Firmware auf die Drohne bringen (flashen)

„Flashen“ = die Software der Flugsteuerung **neu schreiben**, wie ein Update.

### 1.1 Wichtig vorher

- Props **ab** oder sicher blockiert (je nach Team-Regel).  
- **Parameter sichern** (falls die Drohne schon eingerichtet ist):  
  Mission Planner → verbinden → **CONFIG** → **Full Parameter List** (oder
  **Full Parameter Tree**) → **Save to file** / speichern.  
  Quelle: [Loading Firmware](https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html)  
  (Parameter bleiben bei gleichem Fahrzeugtyp meist erhalten — sichern ist trotzdem klug.)

### 1.2 Firmware-Datei besorgen (`.apj`)

Jemand im Team (mit dem gebauten Branch) muss **für euer Board** bauen, z. B.:

```text
./waf configure --board <EURE_BOARD_ID>
./waf copter
```

Die fertige Datei liegt typischerweise unter:

```text
build/<board>/bin/arducopter.apj
```

(`<board>` = z. B. der Name eurer Pixhawk/Cube/… — **exakt den Board-Namen
verwenden, den ihr sonst auch flasht.**)

Ohne passendes Board-Build: die Datei von dem Teammitglied holen, das die
Hardware kennt.

### 1.3 Mit Mission Planner flashen (Custom Firmware)

Offizielle Schritte für **eigene** Firmware (nicht der normale „Quad“-Button
von firmware.ardupilot.org), siehe  
[Loading Firmware](https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html)
und Abschnitt *Custom Firmware*:

1. Flugsteuerung per **USB** an den PC (möglichst **direkter** USB-Port, kein Hub).  
2. Mission Planner starten.  
3. Oben rechts: **COM-Port** wählen (oder **AUTO**), Baud oft **115200**.  
4. **Noch nicht** auf **Connect** drücken.  
5. Oben **SETUP** → links **Install Firmware**  
   (deutsch manchmal: Setup → Firmware installieren).  
6. Link **Load custom firmware** / **Eigene Firmware laden** klicken.  
   - Falls der Link **nicht** sichtbar ist:  
     **CONFIG** → **Planner** → Layout auf **Advanced** stellen  
     (steht so in der ArduPilot-Doku).  
7. Die Datei **`arducopter.apj`** wählen (**nicht** `.hex`, außer ihr wisst genau,
   dass ihr DFU nutzt).  
8. Anweisungen befolgen (ggf. USB ab/an, „Are you sure?“ → Ja).  
9. Unten rechts sollten Statusmeldungen wie **erase…**, **program…**,
   **verify…**, **Upload Done** kommen.  
10. Einige Sekunden warten, dann **Connect**.

### 1.4 Prüfen, ob die richtige Firmware drauf ist

1. **Connect**.  
2. **CONFIG** → **Full Parameter List** → **Refresh Params**.  
3. Suche: **`THROW_DETECT`**.  

| Ergebnis | Bedeutung |
|----------|-----------|
| Parameter **sichtbar** | Richtige (neue) Firmware — weiter mit Kapitel 2. |
| Parameter **fehlt** | Noch alte Firmware oder falsches Board-Build — nochmal flashen / richtiges Board. |

---

## 2. Parameter in Mission Planner einstellen

### Menüweg

Quelle: [Mission Planner Configuration and Tuning](https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html)

1. Drohne **Connect**.  
2. Oben **`CONFIG`**.  
3. Links **`Full Parameter List`**  
   (oder **`Full Parameter Tree`**).  
4. Suche: **`THROW`**.  
5. Werte eintragen (Kapitel 3).  
6. **`Write Params`** drücken (sonst speichert die Drohne nichts).  
7. Optional: nochmal **Refresh Params** und Zahl kontrollieren.

| Knopf | Bedeutung |
|-------|-----------|
| Wert ändern | nur auf dem Bildschirm |
| **Write Params** | in die Flugsteuerung schreiben |
| **Refresh Params** | von der Drohne neu laden |

---

## 3. Einmalig: feste Werte (vor allen 6 Versuchen)

Diese Werte **einmal** setzen, **Write Params**, dann nur noch
`THROW_DETECT` pro Versuch ändern.

| Parameter | Wert | Kurz |
|-----------|------|------|
| `THROW_TYPE` | `0` | Wurf nach oben |
| `THROW_ALT_ACSND` | `0` | nach dem Fangen nicht extra hochklettern |
| `THROW_ALT_DCSND` | `1` | (bei TYPE 0 unwichtig) |
| `THROW_ALT_MIN` | `0` | keine Min-Höhe |
| `THROW_ALT_MAX` | `0` | keine Max-Höhe |
| `THROW_NEXTMODE` | `5` | danach Loiter (Position halten) |
| `THROW_MOT_START` | `0` | Props vor Erkennung aus |
| `THROW_ACCEL_MAX` | `0.7` | „Abschuss vorbei“-Grenze |
| `THROW_DET_MS` | `80` | Wartezeit (ms), bis Bedingung gilt |
| `THROW_SPD_MIN` | `5` | Mindest-Geschwindigkeit |
| `THROW_VELZ_MIN` | `1` | Mindest-Steigen |
| `THROW_IMPULSE_G` | `8` | Stärke des Abschuss-Schlags (Variante 3/4) |
| `THROW_UPR_THR` | `0.5` | Gas beim Aufrichten (50 %) |
| `MOT_SPOOL_TIME` | `0.3` | Motor-Hochlauf (Sekunden), Suche `MOT_SPOOL` |

---

## 4. Die 6 Versuche — nur diese eine Zahl ändern

Nach dem Setzen der festen Tabelle (Kapitel 3):

**Pro Versuch:** nur `THROW_DETECT` setzen → **Write Params** → Wurf.

### Versuch 1 — altes Verhalten (Vergleich)

| Parameter | Wert |
|-----------|------|
| **`THROW_DETECT`** | **`0`** |

Motoren relativ spät (nahe „Gipfel“ der Bahn). Baseline.

---

### Versuch 2 — EarlyCoast

| Parameter | Wert |
|-----------|------|
| **`THROW_DETECT`** | **`1`** |

Früh, sobald der starke Abschuss vorbei ist (ca. unter 0,7 g), Drohne steigt noch.

---

### Versuch 3 — EarlyCoast mit fester 1-g-Grenze

| Parameter | Wert |
|-----------|------|
| **`THROW_DETECT`** | **`2`** |

Wie Versuch 2, aber „Abschuss vorbei“ fest bei 1,0 g.

---

### Versuch 4 — PostImpulse (für Katapult empfohlen)

| Parameter | Wert |
|-----------|------|
| **`THROW_DETECT`** | **`3`** |

Zuerst muss ein starker Schlag (≥ 8 g) gesehen werden, dann früh Motoren.  
Weniger Fehlstart beim Tragen.

---

### Versuch 5 — PostImpulse, schneller

| Parameter | Wert |
|-----------|------|
| **`THROW_DETECT`** | **`4`** |

Wie Versuch 4, kürzeres Warten (früher / aggressiver).

---

### Versuch 6 — ClimbingFast

| Parameter | Wert |
|-----------|------|
| **`THROW_DETECT`** | **`5`** |

Früh wie Versuch 2, **ohne** den Impuls-Zwang. Etwas höheres Fehltrigger-Risiko.

---

### Empfohlene Reihenfolge im Feld

Wenn ihr die Nummern 0–5 nacheinander wollt: **Versuch 1 → 6** wie oben
(`THROW_DETECT` = 0,1,2,3,4,5).

Wenn ihr **schnell** das Wichtigste wollt:

| Reihenfolge | `THROW_DETECT` |
|-------------|----------------|
| 1. | `0` |
| 2. | `3` |
| 3. | `4` |
| 4. | `1` |
| 5. | `2` |
| 6. | `5` |

---

## 5. Jeder einzelne Wurf (Checkliste)

1. Feste Parameter stimmen (Kapitel 3).  
2. `THROW_DETECT` = gewünschte Zahl → **Write Params**.  
3. Flugmodus **Throw** (Modus-Nummer **18**).  
4. **Armieren** (Throw zuerst wählen, dann armieren).  
5. Abschuss.  
6. Meldungen am Boden, z. B.:  
   - `waiting for throw`  
   - `throw detected - spooling motors (detect 3)` ← die Zahl = eure Variante  
7. Notieren: Variante-Nummer, ob sie sich fängt, ob sie trudelt, Log behalten.  
8. Nächster Versuch: nur `THROW_DETECT` ändern.

---

## 6. Was die Varianten bedeuten (ein Satz je)

| `THROW_DETECT` | Name | Ein Satz |
|----------------|------|----------|
| 0 | LegacyPeak | Alt: wartet eher auf den Gipfel der Bahn. |
| 1 | EarlyCoast | Früh nach Ende des Abschuss-Drucks (0,7 g). |
| 2 | EarlyCoast1g | Früh mit fester 1-g-Grenze. |
| 3 | PostImpulse | Starken Schlag gesehen, dann früh — gut für Katapult. |
| 4 | PostImpulseFast | Wie 3, noch früher. |
| 5 | ClimbingFast | Früh ohne Impuls-Pflicht. |

---

## 7. Sicherheit

- Props / Personen / Freifeld beachten.  
- Frühe Varianten (1–5) schalten Motoren **früher** ein.  
- `THROW_MOT_START=1` (nicht in den ersten 6 Versuchen): Props können **vor** dem
  Schuss schon drehen — nur mit bewusstem Sicherheitskonzept.  
- Not-Disarm greifbar.

---

## 8. Später (nicht bei den ersten 6)

Nur wenn eine Variante gut war, **einzeln** testen:

| Parameter | Test A | Zurück |
|-----------|--------|--------|
| `THROW_MOT_START` | `1` | `0` |
| `MOT_SPOOL_TIME` | `0.1` | `0.3` |

Nicht mit den ersten 6 Versuchen vermischen.

---

## 9. Sind wir bereit für den Feldtest?

| Punkt | Status |
|-------|--------|
| Code + 6 Varianten im Branch | ja |
| Parameter in Mission Planner umschaltbar | ja (nach Flash) |
| Laie-Anleitung inkl. Flash + 6 Versuchswerte | diese Datei |
| SITL-Build der Änderungen | ja (bereits geprüft) |
| **Firmware `.apj` für euer reales Board** | **Team muss bauen** (`--board <euer Board>`) |
| Hardware-Flash auf Mehrans Drohne | **vor Ort / im Team** |
| GPS/Arming/Kalibrierung wie gewohnt | ja, nicht neu erfinden |
| Feld: Freiraum, Sicherheit, Logs | ja, Piloten-Verantwortung |

**Kurz:** Software-seitig für den Test **bereit**, sobald die **richtige `.apj`
für euer Board** gebaut und geflasht ist und `THROW_DETECT` in der Parameterliste
erscheint. Ohne diesen Flash bringen die 6 Versuche nichts (alte Firmware).

---

## 10. Technik (nur falls jemand im Code nachschaut)

- Branch: `fabian/throw-detect-variants`  
- Code: `ArduCopter/mode_throw.cpp`, Parameter in `Parameters.cpp`  
- AI-unterstützte Änderung; Verantwortung und Tests liegen beim Team  

Menü-Quellen:

- https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html  
- https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html  
