# TopOverlay

Overlay desktop per Windows con mirino, bordo schermo, HUD testuale e personaggio GIF animato. Il progetto include anche una piccola app Electron/React per modificare le impostazioni senza intervenire a mano su `config.json`.

## Funzionalita

- Overlay sempre in primo piano e trasparente.
- Modalita click-through, cosi mouse e click passano all'app sotto l'overlay.
- Mirino configurabile: colore, lunghezza, gap e spessore.
- Bordo schermo configurabile: colore e spessore.
- HUD testuale con scorciatoie rapide.
- Personaggio GIF animato in basso a destra, con dimensioni e margine regolabili.
- App impostazioni con pagine per Personaggi, Display e Sistema.
- Sincronizzazione tra overlay e settings UI tramite `config.json`.
- Avvio automatico con Windows tramite chiave di registro utente.

## Struttura del progetto

```text
.
|-- config.json
|-- launch-settings.bat
|-- characters/
|   `-- frieren/
|       `-- frieren.gif
|-- overlay/
|   |-- main.cpp
|   |-- CMakeLists.txt
|   |-- build_msvc.bat
|   |-- build_mingw.bat
|   |-- replace.bat
|   `-- json.hpp
`-- settings-ui/
    |-- package.json
    |-- electron.vite.config.ts
    |-- src/main/
    |-- src/preload/
    `-- src/renderer/
```

## Requisiti

- Windows.
- Node.js e npm per l'app impostazioni.
- Electron viene installato tramite npm.
- Per compilare l'overlay C++ serve una delle seguenti opzioni:
  - Visual Studio Build Tools / Developer Command Prompt, oppure
  - MinGW con `g++` nel PATH, oppure
  - CMake 3.15+ con un compilatore C++17.

## Installazione

Installa le dipendenze della settings UI:

```bat
cd settings-ui
npm install
```

Se `node_modules` e gia presente, questo passaggio puo non essere necessario.

## Avvio rapido

Per aprire la finestra impostazioni dalla root del progetto:

```bat
launch-settings.bat
```

Per avviare l'overlay senza aprire automaticamente la settings UI:

```bat
overlay\overlay.exe --no-ui
```

Se usi l'eseguibile generato nella cartella `overlay\build`, l'app impostazioni lo cerca in:

```text
overlay\build\overlay.exe
```

## Sviluppo settings UI

Dalla cartella `settings-ui`:

```bat
npm run dev
```

Build della UI:

```bat
npm run build
```

Preview della build:

```bat
npm run start
```

La settings UI usa Electron + React + TypeScript. Il main process legge e scrive `config.json`, espone le API IPC al renderer tramite preload e puo lanciare l'overlay.

## Compilazione overlay

### MSVC

Apri un Visual Studio Developer Command Prompt nella cartella `overlay`, poi esegui:

```bat
build_msvc.bat
```

### MinGW

Con `g++` disponibile nel PATH:

```bat
cd overlay
build_mingw.bat
```

### CMake

```bat
cd overlay
cmake -S . -B build
cmake --build build --config Release
```

Il `CMakeLists.txt` imposta l'output dell'eseguibile direttamente nella cartella `build`.

## Configurazione

Le impostazioni condivise vivono in `config.json`.

```json
{
  "debugMode": false,
  "launchSettingsOnStart": true,
  "character": {
    "enabled": true,
    "path": "characters\\frieren\\frieren.gif",
    "maxWidth": 260,
    "maxHeight": 260,
    "margin": 28
  },
  "crosshair": {
    "enabled": true,
    "length": 18,
    "gap": 5,
    "thickness": 2,
    "color": "#00FF50"
  },
  "border": {
    "enabled": true,
    "thickness": 3,
    "color": "#FF3232"
  },
  "hud": {
    "enabled": true,
    "fontSize": 16
  },
  "overlay": {
    "visible": true,
    "clickThrough": true
  }
}
```

Campi principali:

- `debugMode`: mostra informazioni extra nell'HUD.
- `launchSettingsOnStart`: apre la settings UI quando parte `overlay.exe`.
- `character.enabled`: mostra o nasconde la GIF.
- `character.path`: percorso assoluto o relativo alla root del progetto.
- `character.maxWidth` / `maxHeight`: limiti massimi della GIF in pixel.
- `character.margin`: distanza dal bordo basso/destro.
- `crosshair`: controlli del mirino.
- `border`: controlli del bordo schermo.
- `hud.enabled`: mostra o nasconde il testo in alto.
- `overlay.visible`: stato iniziale dell'overlay.
- `overlay.clickThrough`: stato iniziale del click-through.

## Personaggi GIF

I personaggi vengono cercati dentro `characters/`, usando una cartella per personaggio:

```text
characters/
`-- nome-personaggio/
    `-- animazione.gif
```

La settings UI mostra automaticamente le GIF trovate. Puoi anche selezionare una GIF esterna con il pulsante di aggiunta.

## Scorciatoie

Quando l'overlay e attivo:

- `Ctrl+Shift+H`: mostra/nasconde overlay.
- `Ctrl+Shift+T`: attiva/disattiva click-through.
- `Ctrl+Shift+S`: apre le impostazioni.
- `Ctrl+Shift+Q`: chiude l'overlay.

Quando cambi visibilita o click-through con una scorciatoia, l'overlay salva lo stato in `config.json`, cosi la settings UI rimane aggiornata.

## Avvio automatico

La pagina Sistema puo aggiungere o rimuovere l'avvio automatico tramite:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

La voce punta a `overlay\build\overlay.exe`. Se usi una posizione diversa per l'eseguibile, aggiorna il codice o copia l'eseguibile nella posizione attesa.

## Note operative

- L'overlay usa GDI+ per caricare e disegnare GIF animate.
- La trasparenza usa una color key quasi nera: `RGB(1, 1, 1)`.
- L'app Electron usa `OVERLAY_ROOT` quando viene avviata da `launch-settings.bat`, cosi trova correttamente `config.json`, `characters/` e `overlay/`.
- `json.hpp` e la libreria header-only nlohmann/json usata dal lato C++.

## Problemi comuni

### La settings UI non trova i personaggi

Avvia l'app da `launch-settings.bat` oppure imposta `OVERLAY_ROOT` alla root del progetto.

### Il pulsante "Avvia Overlay" non parte

Controlla che esista:

```text
overlay\build\overlay.exe
```

In alternativa compila l'overlay e copia l'eseguibile nella posizione attesa.

### L'overlay blocca i click

Premi `Ctrl+Shift+T` per riattivare il click-through, oppure abilitalo dalla pagina Sistema.

### Le modifiche non si vedono subito

L'overlay osserva `config.json` e applica le modifiche. Se qualcosa resta bloccato, chiudi l'overlay con `Ctrl+Shift+Q` e riaprilo.
