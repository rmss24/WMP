import { app, BrowserWindow, ipcMain, dialog, shell } from 'electron'
import { join, dirname } from 'path'
import { readFileSync, writeFileSync, existsSync, readdirSync, mkdirSync, watch } from 'fs'
import { execSync, spawn } from 'child_process'

const defaultConfig = {
  version: 1,
  autoStart: false,
  debugMode: false,
  launchSettingsOnStart: true,
  character: {
    enabled: true,
    path: 'characters\\frieren\\frieren.gif',
    maxWidth: 260,
    maxHeight: 260,
    margin: 28
  },
  crosshair: {
    enabled: true,
    length: 18,
    gap: 5,
    thickness: 2,
    color: '#00FF50'
  },
  border: {
    enabled: true,
    thickness: 3,
    color: '#FF3232'
  },
  hud: {
    enabled: true,
    fontSize: 16
  },
  overlay: {
    visible: true,
    clickThrough: true
  }
}

function getResourcesPath(): string {
  // When packaged: resources/ dir sits next to the exe and contains overlay.exe, characters/, config.json
  if (app.isPackaged) return process.resourcesPath
  // Dev: project root (contains overlay/, characters/, config.json)
  if (process.env.OVERLAY_ROOT) return process.env.OVERLAY_ROOT
  const candidates = [
    join(__dirname, '..', '..', '..'),
    join(process.cwd(), '..'),
    process.cwd(),
  ]
  for (const c of candidates) {
    if (existsSync(join(c, 'characters'))) return c
    if (existsSync(join(c, 'overlay'))) return c
  }
  return join(__dirname, '..', '..', '..')
}

function getConfigPath(): string {
  return join(getResourcesPath(), 'config.json')
}

function getOverlayExePath(): string {
  if (app.isPackaged) return join(process.resourcesPath, 'overlay.exe')
  // Dev: compiled overlay
  const root = getResourcesPath()
  for (const p of ['overlay\\overlay_new.exe', 'overlay\\overlay_static.exe', 'overlay\\overlay.exe']) {
    const full = join(root, p)
    if (existsSync(full)) return full
  }
  return join(root, 'overlay', 'overlay.exe')
}

function getCharactersPath(): string {
  return join(getResourcesPath(), 'characters')
}

function readConfig(): typeof defaultConfig {
  const p = getConfigPath()
  if (!existsSync(p)) {
    writeFileSync(p, JSON.stringify(defaultConfig, null, 2), 'utf-8')
    return defaultConfig
  }
  try {
    return { ...defaultConfig, ...JSON.parse(readFileSync(p, 'utf-8')) }
  } catch {
    return defaultConfig
  }
}

function writeConfig(cfg: unknown): void {
  writeFileSync(getConfigPath(), JSON.stringify(cfg, null, 2), 'utf-8')
}

let win: BrowserWindow | null = null

function createWindow(): void {
  win = new BrowserWindow({
    width: 920,
    height: 660,
    minWidth: 780,
    minHeight: 520,
    frame: false,
    transparent: true,
    backgroundColor: '#00000000',
    show: false,
    icon: join(__dirname, '../../resources/icon.png'),
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false
    }
  })

  if (process.platform === 'win32') {
    try { win.setBackgroundMaterial('acrylic') } catch {}
  }

  win.on('ready-to-show', () => win!.show())

  if (!app.isPackaged && process.env.ELECTRON_RENDERER_URL) {
    win.loadURL(process.env.ELECTRON_RENDERER_URL)
  } else {
    win.loadFile(join(__dirname, '../renderer/index.html'))
  }

  let watchDebounce: ReturnType<typeof setTimeout> | null = null
  try {
    const cfgPath = getConfigPath()
    if (existsSync(cfgPath)) watch(cfgPath, () => {
      if (watchDebounce) clearTimeout(watchDebounce)
      watchDebounce = setTimeout(() => {
        const fresh = readConfig()
        win?.webContents.send('config-changed', fresh)
      }, 200)
    })
  } catch {}
}

ipcMain.handle('get-config', () => readConfig())

ipcMain.handle('save-config', (_, cfg: unknown) => {
  writeConfig(cfg)
  return true
})

ipcMain.handle('select-gif', async () => {
  if (!win) return null
  const res = await dialog.showOpenDialog(win, {
    title: 'Seleziona personaggio GIF',
    filters: [{ name: 'GIF animata', extensions: ['gif'] }],
    properties: ['openFile']
  })
  return res.canceled ? null : res.filePaths[0]
})

ipcMain.handle('get-characters', () => {
  const charsDir = getCharactersPath()
  const results: { name: string; path: string; previewUrl: string }[] = []
  if (!existsSync(charsDir)) return results
  try {
    for (const entry of readdirSync(charsDir, { withFileTypes: true })) {
      if (!entry.isDirectory()) continue
      const sub = join(charsDir, entry.name)
      for (const file of readdirSync(sub)) {
        if (!file.toLowerCase().endsWith('.gif')) continue
        const fullPath = join(sub, file)
        results.push({
          name: entry.name,
          path: fullPath.replace(/\//g, '\\'),
          previewUrl: `file:///${fullPath.replace(/\\/g, '/')}`
        })
      }
    }
  } catch {}
  return results
})

ipcMain.handle('open-characters-folder', () => {
  const charsDir = getCharactersPath()
  if (!existsSync(charsDir)) mkdirSync(charsDir, { recursive: true })
  shell.openPath(charsDir)
})

ipcMain.handle('set-autostart', (_, enable: boolean) => {
  try {
    const overlayExe = getOverlayExePath()
    if (enable) {
      execSync(
        `reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" /v TopOverlay /t REG_SZ /d "${overlayExe}" /f`
      )
    } else {
      execSync(
        `reg delete "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" /v TopOverlay /f`
      )
    }
    return true
  } catch {
    return false
  }
})

ipcMain.handle('get-autostart', () => {
  try {
    execSync(
      `reg query "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" /v TopOverlay`
    )
    return true
  } catch {
    return false
  }
})

ipcMain.handle('launch-overlay', () => {
  const overlayExe = getOverlayExePath()
  if (existsSync(overlayExe)) {
    spawn(overlayExe, ['--no-ui'], { detached: true, stdio: 'ignore' }).unref()
    return true
  }
  return false
})

ipcMain.handle('window-minimize', () => win?.minimize())
ipcMain.handle('window-close', () => win?.close())

app.whenReady().then(createWindow)
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit() })
