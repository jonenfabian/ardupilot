# SITL-Builds zum Weitergeben (Throw-Mode-Branch)

Dieser Ordner enthält fertig kompilierte **Simulator-Binaries** (SITL =
Software In The Loop) von ArduCopter mit den Throw-Mode-Änderungen dieses
Branches. Damit kann man die Firmware **ohne Drohne und ohne Kompilieren**
am Rechner testen — Verbindung z. B. mit Mission Planner.

Was die Änderungen inhaltlich tun, steht in `ArduCopter/THROW_CHANGES.md`;
die Feldtest-Anleitung in `ArduCopter/FELDTEST.md`.

## Dateien

| Datei | Plattform |
|---|---|
| `arducopter-sitl-linux-x86_64` | Linux 64-bit (läuft unter Windows via WSL, siehe unten) |
| `ArduCopter-SITL-Windows.zip` | Windows nativ (64-bit; Exe + benötigte Cygwin-DLLs + `start-sitl.bat`) — **das ist die Datei für die Kollegen** |

## Variante A: Nativ unter Windows (Zip)

1. `ArduCopter-SITL-Windows.zip` herunterladen und **komplett entpacken**
   (die `cyg*.dll`-Dateien müssen neben der Exe liegen).
2. Doppelklick auf `start-sitl.bat` (startet die Exe mit sinnvollen
   Standard-Argumenten). Beim ersten Start ggf. die Windows-Firewall-Nachfrage
   bestätigen.
3. Mission Planner öffnen → oben rechts Verbindungsart **TCP** wählen →
   **Connect** → Host `127.0.0.1`, Port `5760`.

## Variante B: Windows mit WSL (Linux-Binary)

Einmalig (Administrator-PowerShell, danach Neustart):

```powershell
wsl --install -d Ubuntu-24.04
```

Dann im Ubuntu-Terminal (Datei z. B. in den Home-Ordner kopieren):

```bash
chmod +x arducopter-sitl-linux-x86_64
./arducopter-sitl-linux-x86_64 --model + --wipe
```

Der Simulator wartet dann auf eine Verbindung. Mission Planner unter Windows:
**TCP** → `127.0.0.1`, Port `5760`. (`--wipe` nur beim ersten Start — setzt
die Parameter auf Standardwerte.)

## Hinweise zum Testen des Throw-Modus

- Die neuen Parameter (`THROW_DETECT` usw.) sind in der vollen Parameterliste
  von Mission Planner sichtbar, sobald verbunden.
- Einen Wurf simuliert man wie in den Autotests: Mode **THROW** wählen,
  armen, dann per `SIM_SHOVE`-Parametern oder einfach gemäß
  `ArduCopter/FELDTEST.md` vorgehen.
- Beide Binaries entsprechen dem Flight-Code-Stand dieses Branches
  (Basis: ArduCopter 4.6.3; die Windows-Exe wurde aus Commit `24f6cf4`
  gebaut — seit dem Linux-Build kamen nur CI-Änderungen dazu, kein
  Flugcode). Das exakte Build-Commit steht in der `git.txt` im Zip.
