# FELDTEST — Wurferkennung + Power-Climb

**Ziel:** Zwei Testcases am Katapult fliegen. In beiden steigt die Drohne nach der
Wurferkennung erst **einige Sekunden mit festem Schub** weiter (Power-Climb), damit
sich die Sensoren vom Abschuss erholen, und geht erst danach in den normalen
Schwebeflug über.

- **Testcase 1:** Standard-Erkennung + Power-Climb
- **Testcase 2:** Früh-Erkennung (Motorstart früher) + Power-Climb
- *(Optional: Baseline-Referenz = Standard-Erkennung ohne Power-Climb, wie Juli-Tests)*

**Basis dieser Firmware:** exakt **ArduCopter 4.6.3 (`92b0cd78`)** — dieselbe
Version wie die Juli-Baseline. Branch: `fabian/throw-detect-variants-4.6`
(https://github.com/jonenfabian/ardupilot/tree/fabian/throw-detect-variants-4.6)

**Ihr braucht:** Laptop mit Mission Planner, USB-Kabel, freigegebenes Testgelände
mit **viel Luftraum nach oben** (siehe Kapitel 5), Not-Disarm griffbereit.

---

## 1. Firmware bauen und flashen

### 1.1 Bauen (`.apj`)

```text
git fetch && git checkout fabian/throw-detect-variants-4.6
git submodule update --init --recursive
./waf configure --board CubeOrangePlus
./waf copter
```

Ergebnis: `build/CubeOrangePlus/bin/arducopter.apj`.

### 1.2 Flashen im Mission Planner

1. Vorher Parameter sichern: verbinden → **CONFIG → Full Parameter List → Save to file**.
2. Trennen (Disconnect). Autopilot per USB direkt (kein Hub) anschließen.
3. **SETUP → Install Firmware** → unten **Load custom firmware** klicken.
   (Link nicht sichtbar? **CONFIG → Planner → Layout: Advanced** einstellen.)
4. Die gebaute `arducopter.apj` auswählen, Meldungen bis `Upload Done` abwarten.
5. Einige Sekunden warten, dann **Connect**.

### 1.3 Prüfen, ob die richtige Firmware läuft

**CONFIG → Full Parameter List → Refresh Params** → oben rechts ins Suchfeld
`THROW_CLIMB` eintippen. Erscheinen `THROW_CLIMB_S` und `THROW_CLIMB_THR`,
ist die richtige Firmware drauf. Fehlen sie → falsche Firmware, zurück zu 1.2.

---

## 2. Parameter einstellen im Mission Planner — Schritt für Schritt

So ändert ihr **jeden** Parameter in dieser Anleitung:

1. Drohne verbinden: oben rechts COM-Port wählen (oder AUTO), Baud **115200**, **Connect**.
2. Oben in der Menüleiste auf **CONFIG** klicken.
3. Links im Menü **Full Parameter List** wählen.
4. Rechts oben ins **Suchfeld** den Parameternamen tippen (z. B. `THROW`) —
   die Liste filtert sich automatisch.
5. In der Zeile des Parameters in die Spalte **Value** klicken und den neuen
   Wert eintragen.
6. Rechts auf **Write Params** klicken — erst damit landet der Wert auf der Drohne.
7. Zur Kontrolle **Refresh Params** klicken und den Wert nochmal ablesen.

Die Statusmeldungen der Drohne (z. B. `throw detected`, `power climb`) seht ihr
unter **DATA** (oben links) → Reiter **Messages** (unten links).

---

## 3. Feste Werte (einmalig vor allen Versuchen setzen)

Alle nach Schema aus Kapitel 2 setzen, am Ende einmal **Write Params**:

| Parameter | Wert | Bedeutung |
|-----------|------|-----------|
| `THROW_TYPE` | `0` | Wurf nach oben |
| `THROW_ALT_MIN` | `0` | keine Min-Höhe für Erkennung |
| `THROW_ALT_MAX` | `0` | keine Max-Höhe für Erkennung |
| `THROW_NEXTMODE` | `5` | nach dem Fangen: Loiter |
| `THROW_MOT_START` | `0` | Props vor Erkennung aus |
| `THROW_ACCEL_MAX` | `0.7` | Schwelle „Abschuss-Impuls vorbei" |
| `THROW_DET_MS` | `80` | Haltezeit Früh-Erkennung (nur Testcase 2 relevant) |
| `THROW_SPD_MIN` | `1.5` | Mindestgeschwindigkeit (aus Juli-Logs abgeleitet — nicht erhöhen!) |
| `THROW_VELZ_MIN` | `1` | Mindest-Steigrate |
| `THROW_IMPULSE_G` | `8` | Abschuss-Nachweis (nur Testcase 2) |
| `THROW_UPR_THR` | `0.5` | Gas beim Aufrichten |
| `MOT_SPOOL_TIME` | `0.3` | Motor-Hochlaufzeit (s) |

Hinweis 4.6.3: Nach dem Power-Climb steigt die Drohne beim Einfangen nochmal fest
**+3 m** über die aktuelle Höhe (in dieser Version hartkodiert) — das ist normal.

---

## 4. Die Testcases einstellen

Pro Testcase nur die folgenden Parameter ändern (Kapitel-2-Schema) → **Write Params** → werfen.

### Testcase 1 — Standard-Erkennung + Power-Climb

| Parameter | Wert |
|-----------|------|
| `THROW_DETECT` | `0` |
| `THROW_CLIMB_S` | `5` |
| `THROW_CLIMB_THR` | `0.6` |

### Testcase 2 — Früh-Erkennung + Power-Climb

| Parameter | Wert |
|-----------|------|
| `THROW_DETECT` | `3` |
| `THROW_CLIMB_S` | `5` |
| `THROW_CLIMB_THR` | `0.6` |

Motorstart deutlich früher als bei Testcase 1 (nach dem Abschuss-Impuls statt erst
bei der Standard-Bestätigung). Löst die Früh-Erkennung nicht aus, übernimmt
automatisch die Standard-Erkennung (Meldung `fallback peak` statt `detect 3`) —
bitte notieren, das ist ein Versuchsergebnis. Der Startzeitpunkt lässt sich über
`THROW_DET_MS` verschieben (80 = früh; größer = später; über ~300 übernimmt der
Fallback).

### Baseline-Referenz (nur bei Bedarf, entspricht den Juli-Flügen)

| Parameter | Wert |
|-----------|------|
| `THROW_DETECT` | `0` |
| `THROW_CLIMB_S` | `0` |

---

## 5. Power-Climb: Was passiert, und was ihr einstellen könnt

**Ablauf:** Erkennung → Motoren laufen hoch → Drohne richtet sich auf → **steigt
`THROW_CLIMB_S` Sekunden mit festem Gas `THROW_CLIMB_THR` senkrecht** (ohne auf die
gestörten Höhen-/Geschwindigkeitsschätzungen zu hören) → normale Höhenregelung
(+3 m) → Position halten → Loiter.

**Die zwei Feintuning-Regler:**

| Parameter | Bedeutung | Werte zum Testen |
|-----------|-----------|------------------|
| `THROW_CLIMB_S` | Dauer des Steigflugs in Sekunden. `0` = Funktion aus. | `3` → `5` → `10` (steigern, nur wenn nötig) |
| `THROW_CLIMB_THR` | Schub während des Steigflugs. `0.5` = 50 %, `0.6` = 60 %, `0.7` = 70 % Gas. | Start `0.6`; **muss über dem Schwebe-Gas (~0.33) liegen**, sonst sinkt sie! |

**Wie hoch steigt sie dabei? (Schätzung — nach den ersten Logs prüfen!)**
Bei Schub 0.6 beschleunigt die Drohne anfangs mit grob 8 m/s² nach oben (real
bremst der Luftwiderstand). Grobe Richtwerte inklusive Abschussschwung:

| `THROW_CLIMB_S` | erwarteter Höhengewinn |
|-----------------|------------------------|
| 3 s | ~40–70 m |
| 5 s | ~100–150 m |
| 10 s | 200 m+ — **nur mit großzügigem, freigegebenem Luftraum!** |

Dazu kommen nach Ende des Steigflugs noch ~10–25 m Überschwinger, bis die
Höhenregelung die Fahrt abgebaut hat. `THROW_ALT_MAX` begrenzt das **nicht**
(wirkt nur auf die Erkennung, nicht auf den Climb).

**Abbrechen im Flug:** Moduswechsel am Sender (z. B. auf Loiter/AltHold) beendet
alles sofort — Daumen am Schalter. Zweiter Weg: `THROW_CLIMB_S` am Laptop auf `0`
setzen + Write Params — der Steigflug endet augenblicklich.

---

## 6. Ablauf pro Wurf

1. Testcase-Parameter gesetzt (Kapitel 4), **Write Params**.
2. Flugmodus **Throw** wählen (Modus 18), dann armieren. **Armierte Drohne nicht
   tragen** — erst im Katapult armieren.
3. Abschuss.
4. Meldungen unter **DATA → Messages** prüfen. Erwartete Reihenfolge:
   - `waiting for throw`
   - `throw detected - spooling motors (detect 0)` bzw. `(detect 3)`
     — bei Testcase 2 ggf. `(fallback peak)` = Früh-Erkennung hat verpasst, notieren!
   - `uprighted - power climb 5.0s`
   - `power climb done - controlling height`
   - `height achieved - controlling position`
5. Notieren: Testcase, Einstellwerte, Verhalten (Trudeln? Höhe? ruhiges Fangen?), Log sichern.

---

## 7. Sicherheit

- Props, Personen, Freifeld; Not-Disarm griffbereit. Bei Testcase 2 drehen die
  Props schon wenige Meter über dem Katapult an — niemand über der Austrittsbahn.
- **Luftraum:** Höhengewinn-Tabelle in Kapitel 5 beachten; mit 3 s beginnen.
  Sichtlinie halten.
- `THROW_CLIMB_THR` nie unter das Schwebe-Gas (~0.33) stellen.
- Nach Fehlstart (Abschuss ohne Flug): **disarmen, neu armieren.**
- Bricht ein Versuch mit der Meldung „EKF Failsafe" ab (Drohne geht in LAND
  mitten im Steigflug): Für die Testreihe darf `FS_EKF_THRESH` von `0.8` auf
  `1.0` gesetzt werden (Kapitel-2-Schema). **Nach den Tests zurückstellen.**
- Die Früh-Erkennungs-Methoden 1–5 und das Fallback-Sicherheitsnetz sind weiter
  in der Firmware; Standard-Testplan sind aber nur die zwei Cases oben.

---

## 8. Technik (für die Auswertung)

- Branch `fabian/throw-detect-variants-4.6`, Basis Tag `Copter-4.6.3` (`92b0cd78`).
- Code: `ArduCopter/mode_throw.cpp`; Parameter in `Parameters.cpp` (Indizes
  identisch zum Master-Branch — Werte überleben Firmware-Wechsel).
- Log-Auswertung: THRO-Message, **Stage-Nummern in diesem Build:**
  0 Disarmed, 1 Detecting, 2 Spool, 3 Uprighting, **4 PowerClimb**,
  5 HgtStabilise, 6 PosHold.
- SITL-Autotests: `ThrowMode`, `ThrowModeEarlyDetect`, `ThrowModeDetectFallback`,
  `ThrowModePowerClimb`.

Quellen:

- https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html
- https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html
