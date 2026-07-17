# Throw-Erkennung im Feld testen — Anleitung (ohne Fachchinesisch)

Diese Anleitung erklärt, **wie du am Boden per Computer** zwischen verschiedenen
„Wann starten die Motoren nach dem Katapult?“-Varianten umschaltest.

Du brauchst:

- Die Drohne mit **dieser** Firmware (Branch mit den neuen Parametern)
- Einen Laptop/Tablet mit **Mission Planner** (Windows ist üblich)
- USB- oder Telemetrie-Verbindung zur Drohne

---

## 1. Was ist das Problem in einem Satz?

Die Drohne wird mit Druckluft geschossen. Die Software wartet standardmäßig
ziemlich lange (nahe dem höchsten Punkt der Flugbahn), bis die Motoren angehen.
Dann trudelt sie oft schon. Die neuen Einstellungen erlauben **frühere** Starts
zum Vergleichen.

---

## 2. Wo stelle ich das in Mission Planner ein?

Mission Planner hat **kein** eigenes Fenster nur für Throw. Alles läuft über
**Parameter** (Einstellwerte in der Flugsteuerung).

### Menüweg (offizielle Struktur)

Quelle: [Mission Planner — Configuration and Tuning](https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html)

1. Mission Planner starten.
2. Drohne verbinden (COM-Port wählen, **Connect**).
3. Oben in der Leiste auf **`CONFIG`** klicken  
   (deutsch manchmal **„Konfig“** / **„Config/Tuning“** — der Reiter heißt in der
   Doku **CONFIG**).
4. In der **linken Seitenleiste** den Punkt wählen:

   **`Full Parameter List`**  
   (deutsch oft: **„Vollständige Parameterliste“**)

   Alternativ:

   **`Full Parameter Tree`**  
   (Baumansicht derselben Parameter)

5. Oben rechts im Parameter-Fenster das **Suchfeld** (`Search`) nutzen.
6. Tippe: **`THROW`**
7. Es erscheinen alle Throw-Einstellungen (siehe Tabelle unten).

### Parameter ändern und speichern

| Knopf in Mission Planner | Bedeutung |
|--------------------------|-----------|
| Wert in der Zeile ändern | nur in der Liste, noch nicht in der Drohne |
| **`Write Params`** | schreibt die geänderten Werte **in die Flugsteuerung** |
| **`Refresh Params`** | lädt die Liste neu von der Drohne (z. B. nach Firmware-Update) |

**Wichtig:** Nach dem Ändern von `THROW_DETECT` (und den anderen THROW-Werten)
**`Write Params`** drücken. Ein Neustart der Drohne ist für diese Werte
normalerweise **nicht** nötig. Trotzdem: vor dem Wurf kurz prüfen, dass der
Wert nach Refresh noch stimmt.

### Tipp: Favoriten

In der Full Parameter List kannst du oft genutzte Zeilen markieren/favorisieren
(je nach Mission-Planner-Version). Suchbegriff `THROW` merken reicht auch.

---

## 3. Die Varianten (`THROW_DETECT`)

Der wichtigste Schalter:

| Wert | Name in der Liste | Einfache Beschreibung |
|------|-------------------|------------------------|
| **0** | LegacyPeak | **Werkseinstellung / alt.** Motoren starten erst, wenn die Software den „Gipfel“ der Bahn erkennt (Geschwindigkeit nach oben bricht ein). Zum Vergleich immer zuerst messen. |
| **1** | EarlyCoast | **Früh, sobald der Abschuss-Druck vorbei ist.** Die Sensoren „fühlen“ keine starke Beschleunigung mehr (unter `THROW_ACCEL_MAX`, Standard ca. 0,7 g), die Drohne steigt noch und ist schnell genug. |
| **2** | EarlyCoast1g | Wie 1, aber die Grenze „Abschuss vorbei“ ist **fest 1,0 g** (wie die alte „Kraft vorbei“-Logik), ohne auf den Gipfel zu warten. Gut für A/B ohne die 0,7-Einstellung zu vergessen. |
| **3** | PostImpulse | **Empfohlen für Katapult.** Zuerst muss ein **starker Schlag** gesehen werden (`THROW_IMPULSE_G`, z. B. 8 g bei ~25–40 g Peak), danach wie EarlyCoast. Verhindert Fehlstart beim Herumtragen. |
| **4** | PostImpulseFast | Wie 3, aber **kürzeres Warten** (halb so lange wie `THROW_DET_MS`, mindestens 20 ms). Aggressiver / früher. |
| **5** | ClimbingFast | Früh wie 1 (steigen + schnell + „Kraft vorbei“), **ohne** den Impuls-Zwang von 3/4. Etwas höheres Risiko für Fehlauslösung. |

**Feld-Reihenfolge zum Ausprobieren (Vorschlag):**

1. `0` (Baseline)  
2. `3` (PostImpulse)  
3. `4` (PostImpulseFast)  
4. `1` oder `2`  
5. optional `5`

---

## 4. Weitere sinnvolle Einstellungen (alle mit Suche `THROW` bzw. `MOT_SPOOL`)

| Parameter | Typischer Startwert | Einfache Beschreibung |
|-----------|---------------------|------------------------|
| **THROW_DETECT** | `3` für Katapult-Tests | Welche Erkennungs-Variante (Tabelle oben) |
| **THROW_ACCEL_MAX** | `0.7` | „Abschuss-Kraft ist vorbei“-Grenze in g (für Varianten 1,3,4,5). Höher = eher früher. |
| **THROW_DET_MS** | `80` | Wie viele Millisekunden die Bedingung stabil wahr sein muss, bevor Motoren starten. |
| **THROW_SPD_MIN** | `5` | Mindest-Geschwindigkeit (m/s), damit nicht am Boden ausgelöst wird. |
| **THROW_VELZ_MIN** | `1` | Mindest-Steiggeschwindigkeit (m/s) bei Wurf nach oben. |
| **THROW_IMPULSE_G** | `8` | Nur Variante 3 und 4: so stark muss der Abschuss-Schlag mindestens gewesen sein (g). Bei ~25–40 g Peak oft 5–10. |
| **THROW_UPR_THR** | `0.5` | Gasanteil (0…1) beim Aufrichten nach dem Erkennen. Früher fest 50 %. |
| **THROW_MOT_START** | `0` aus / `1` an | `1` = Propeller drehen schon **vor** dem Erkennen langsam (kann Hochlauf verkürzen; **Vorsicht** am Launcher). |
| **THROW_ALT_ACSND** | für Detect-Tests **`0`** | Wie viele Meter die Drohne **nach** dem Aufrichten noch steigen soll. Hohe Werte (z. B. 15) erzeugen oft Auf/Ab-Pumpen — bei Erkennungs-Tests auf 0 lassen. |
| **THROW_TYPE** | `0` | `0` = nach oben schießen, `1` = fallen lassen. |
| **THROW_NEXTMODE** | `5` | Nach erfolgreichem Festhalten: z. B. `5` = Loiter (Position halten). |
| **MOT_SPOOL_TIME** | Werk oft `0.3` | Zeit (Sekunden), bis die Motoren volle Leistung dürfen. Kürzer (z. B. `0.1`) = Schub früher — vorsichtig testen. Suche: `MOT_SPOOL`. |

---

## 5. Ablauf eines Testwurfs (Checkliste)

1. Firmware mit diesen Parametern geflasht, verbunden, **Refresh Params**.  
2. `THROW` suchen → gewünschtes **THROW_DETECT** setzen → **Write Params**.  
3. Begleitwerte prüfen (`THROW_ALT_ACSND=0` für reine Erkennungs-Tests).  
4. Flugmodus **Throw** (Nummer **18**) wählen, **dann** armieren.  
5. Katapult / Druckluft.  
6. Am Boden: Meldungen der Drohne beobachten, z. B.  
   - `waiting for throw`  
   - `throw detected - spooling motors (detect 3)` ← die Zahl ist die Variante  
7. Log speichern (DataFlash). Für den Vergleich zählen:  
   - Wie schnell nach dem Abschuss erscheint „throw detected“?  
   - Trudelt die Drohne schon stark, bevor die Motoren greifen?

---

## 6. Sicherheit (kurz)

- Props können bei `THROW_MOT_START=1` **vor** dem Schuss drehen.  
- Frühe Varianten (1–5) starten Motoren **früher in der Luft** — Freifeld, Abstände, Not-Disarm.  
- Variante **3/4** sind für Katapult sicherer gegen Fehltrigger als 1/2/5.  
- Default **0** bleibt das alte Verhalten (keine Überraschung für andere Nutzer).

---

## 7. Technischer Hinweis (für Entwickler)

Code: `ArduCopter/mode_throw.cpp`, Parameter in `Parameters.cpp` / `ParametersG2`.  
AI-unterstützte Änderung; Feldtests und Verantwortung liegen beim Piloten/Team.

Bei Fragen zur Menüstruktur: Mission Planner Doku  
https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html  
→ Abschnitte **Full Parameter List** und **Full Parameter Tree**.
