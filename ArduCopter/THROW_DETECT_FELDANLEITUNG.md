# Throw-Erkennung — Feldanleitung

**Ziel:** Sechs Detektions-Varianten (`THROW_DETECT` 0–5) am Katapult vergleichen,
ohne bei jedem Wurf umkonfigurieren zu müssen.

**Voraussetzungen:**

- Laptop mit **Mission Planner**
- USB-Kabel zum Autopiloten
- Firmware aus Branch `fabian/throw-detect-variants`
  (https://github.com/jonenfabian/ardupilot/tree/fabian/throw-detect-variants)
- Freigegebenes Testgelände, Not-Disarm, übliche Sicherheitsmaßnahmen

Die Parameter (`THROW_DETECT` usw.) sind nur in dieser Firmware enthalten. Ablauf:
Firmware bauen (`.apj`) → mit Mission Planner flashen → verbinden → Parameter setzen → fliegen.

---

## 1. Firmware flashen

### 1.1 Vorbereitung

- Props ab oder gesichert.
- Bestehende Parameter sichern: Mission Planner → verbinden → **CONFIG** →
  **Full Parameter List** → **Save to file**.
  ([Loading Firmware](https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html))

### 1.2 Firmware bauen (`.apj`)

Für das jeweilige Board bauen:

```text
./waf configure --board <BOARD_ID>
./waf copter
```

Ergebnis unter `build/<board>/bin/arducopter.apj`. `<board>` exakt der Board-Name,
der auch sonst verwendet wird.

### 1.3 Custom Firmware flashen

Siehe [Loading Firmware](https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html),
Abschnitt *Custom Firmware*:

1. Autopilot per USB verbinden (direkter Port, kein Hub).
2. Mission Planner starten, COM-Port wählen (oder AUTO), Baud i. d. R. **115200**.
3. **Noch nicht** auf Connect drücken.
4. **SETUP → Install Firmware**.
5. **Load custom firmware** klicken.
   (Ist der Link ausgeblendet: **CONFIG → Planner → Layout: Advanced**.)
6. `arducopter.apj` wählen (nicht `.hex`, außer bei bewusstem DFU-Einsatz).
7. Anweisungen befolgen; Statusmeldungen `erase… / program… / verify… / Upload Done` abwarten.
8. Einige Sekunden warten, dann **Connect**.

### 1.4 Firmware prüfen

**CONFIG → Full Parameter List → Refresh Params**, nach `THROW_DETECT` suchen.

| Ergebnis | Bedeutung |
|----------|-----------|
| Parameter sichtbar | Korrekte Firmware — weiter mit Kapitel 2 |
| Parameter fehlt | Falsche Firmware oder falsches Board — erneut flashen |

---

## 2. Parameter setzen

1. **Connect**.
2. **CONFIG → Full Parameter List**.
3. Nach `THROW` suchen, Werte eintragen, **Write Params**.
4. Optional **Refresh Params** zur Kontrolle.

`Write Params` schreibt in den Autopiloten, `Refresh Params` lädt neu vom Gerät.
([Mission Planner Config](https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html))

---

## 3. Feste Werte (einmalig, vor allen Versuchen)

Einmal setzen, **Write Params**, danach pro Versuch nur `THROW_DETECT` ändern.

| Parameter | Wert | Bedeutung |
|-----------|------|-----------|
| `THROW_TYPE` | `0` | Wurf nach oben |
| `THROW_ALT_ACSND` | `0` | nach dem Fangen nicht weiter steigen |
| `THROW_ALT_DCSND` | `1` | (bei TYPE 0 irrelevant) |
| `THROW_ALT_MIN` | `0` | keine Min-Höhe |
| `THROW_ALT_MAX` | `0` | keine Max-Höhe |
| `THROW_NEXTMODE` | `5` | danach Loiter |
| `THROW_MOT_START` | `0` | Props vor Erkennung aus |
| `THROW_ACCEL_MAX` | `0.7` | Schwelle „Impuls vorbei" (Variante 1/3/4) |
| `THROW_DET_MS` | `80` | Haltezeit (ms), bis Bedingung gilt |
| `THROW_SPD_MIN` | `5` | Mindestgeschwindigkeit |
| `THROW_VELZ_MIN` | `1` | Mindest-Steigrate |
| `THROW_IMPULSE_G` | `8` | Impuls-Schwelle (Variante 3/4) |
| `THROW_UPR_THR` | `0.5` | Gas beim Aufrichten (50 %) |
| `MOT_SPOOL_TIME` | `0.3` | Motor-Hochlaufzeit (s) |

---

## 4. Die 6 Varianten im Überblick

```text
Abschuss (Impuls)  →  freier Aufstieg  →  Scheitelpunkt  →  Abstieg
      |                     |                   |
 Impuls vorbei         früher Start        später Start (0)
```

| Versuch | `THROW_DETECT` | Motorstart | Unterscheidung | Einsatz |
|---------|----------------|------------|----------------|---------|
| 1 | **0** | spät, am Scheitelpunkt (Steigrate bricht ein) | bisheriges Verhalten, Referenz | Baseline |
| 2 | **1** | früh, sobald Impuls vorbei und noch steigend | ohne Impuls-Nachweis | Früh-Test |
| 3 | **2** | früh, etwas großzügiger als 2 | Schwelle „Impuls vorbei" fest bei 1 g statt 0,7 g | A/B 0,7 g vs 1 g |
| 4 | **3** | früh, nach erkanntem Abschuss-Impuls | wie 2 plus Impuls-Nachweis | Katapult (empfohlen) |
| 5 | **4** | wie 4, kürzere Haltezeit → früher | gleicher Riegel wie 4, aggressiver | Feintuning |
| 6 | **5** | früh, rein kinematisch | nur Geschwindigkeit + Steigen, **keine** Accel-Bedingung | Sensor-unabhängiger Vergleich |

**Zusammengefasst:**

- **0** = spät (Scheitelpunkt)
- **1 / 2** = früh über Accel-Schwelle, ohne Impuls-Nachweis (0,7 g vs. 1,0 g)
- **3 / 4** = früh, mit Impuls-Nachweis (4 schneller als 3)
- **5** = früh, rein kinematisch — ignoriert den Beschleunigungssensor komplett

---

## 5. Die 6 Varianten im Detail

Pro Versuch nur `THROW_DETECT` setzen → **Write Params** → Wurf.

### Versuch 1 — Baseline (`THROW_DETECT = 0`)

Motorstart, sobald der Scheitelpunkt der Bahn erkannt wird (Steigrate lässt nach).
Kein Früh-Start; dient als Vergleichsbasis für die neuen Varianten.

### Versuch 2 — EarlyCoast, 0,7 g (`THROW_DETECT = 1`)

Motorstart, sobald der Abschuss-Impuls vorbei ist (< ca. 0,7 g), die Drohne noch
steigt und schnell genug ist — vor dem Scheitelpunkt. Deutlich früher als 0, ohne
Impuls-Nachweis (durch Speed-Schwellen abgesichert).

### Versuch 3 — EarlyCoast, 1 g (`THROW_DETECT = 2`)

Wie Versuch 2, aber „Impuls vorbei" gilt bereits unter 1,0 g (fester Wert). Oft eine
Spur früher als 2. Direkter Vergleich der beiden Schwellen.

### Versuch 4 — PostImpulse (`THROW_DETECT = 3`)

Erst muss ein starker Abschuss-Impuls erkannt werden (z. B. ≥ 8 g), danach — bei
nachlassendem Druck und weiterem Steigen — früher Motorstart. Zusätzliche
Absicherung gegen Fehlauslösung. Primärer Kandidat für Katapult/Druckluft.
Der Impuls-Nachweis verfällt automatisch nach 5 s ohne vollständige Erkennung
(z. B. Fehlstart).

### Versuch 5 — PostImpulse, schneller (`THROW_DETECT = 4`)

Wie Versuch 4, aber kürzere Haltezeit → etwas früherer Start. Nächste Stufe, falls
3 zu spät auslöst; zurück zu 3, falls 4 zu nervös reagiert.

### Versuch 6 — ClimbingFast, rein kinematisch (`THROW_DETECT = 5`)

Früher Start allein über die Bewegung: schnell genug (`THROW_SPD_MIN`) und
steigend (`THROW_VELZ_MIN`) über die Haltezeit — der Beschleunigungssensor wird
**nicht** ausgewertet. Vergleichstest, ob die Accel-Bedingung (Varianten 1–4)
überhaupt nötig ist oder die EKF-Geschwindigkeit allein reicht. Löst dadurch ggf.
noch während des Abschusses aus — höchstes Risiko-Profil der Früh-Varianten.

### Empfohlene Reihenfolge

Vollständig: `THROW_DETECT` = 0, 1, 2, 3, 4, 5.

Verkürzt:

| Reihenfolge | `THROW_DETECT` | Grund |
|-------------|----------------|-------|
| 1 | `0` | Baseline |
| 2 | `3` | Katapult + früh |
| 3 | `4` | noch früher |
| 4 | `1` | früh ohne Impuls |
| 5 | `2` | 0,7 g vs 1 g |
| 6 | `5` | rein kinematisch nötig? |

---

## 6. Ablauf pro Wurf

1. Feste Parameter geprüft (Kapitel 3).
2. `THROW_DETECT` setzen → **Write Params**.
3. Flugmodus **Throw** (Modus 18).
4. Armieren (Throw wählen, dann armieren).
5. Abschuss.
6. Bodenmeldungen prüfen, z. B. `waiting for throw`,
   `throw detected - spooling motors (detect 3)` (Zahl = Variante).
7. Notieren: Variante, Fangverhalten, Log behalten.
8. Nächster Versuch: nur `THROW_DETECT` ändern.

---

## 7. Sicherheit

- Props, Personen, Freifeld beachten; Not-Disarm griffbereit.
- Varianten 1–5 schalten die Motoren früher ein; Variante 5 kann mangels
  Accel-Bedingung am frühesten (ggf. noch im Abschuss) auslösen.
- **Nach einem Fehlstart (Abschuss ohne Flug): disarmen und neu armieren.**
  Bei Variante 3/4 verfällt der Impuls-Nachweis zwar nach 5 s automatisch,
  Disarm ist trotzdem die saubere Rücksetzung.
- `THROW_MOT_START = 1` (nicht in den ersten 6 Versuchen): Props können vor dem
  Abschuss drehen — nur mit bewusstem Sicherheitskonzept.

---

## 8. Optionale Folgetests

Nur bei einer bewährten Variante, einzeln:

| Parameter | Test | Zurück |
|-----------|------|--------|
| `THROW_MOT_START` | `1` | `0` |
| `MOT_SPOOL_TIME` | `0.1` | `0.3` |

Nicht mit den ersten 6 Versuchen mischen.

---

## 9. Technik

- Branch: `fabian/throw-detect-variants`
- Code: `ArduCopter/mode_throw.cpp`, Parameter in `Parameters.cpp`
- SITL-Build der Änderungen geprüft.

Quellen:

- https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html
- https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html
