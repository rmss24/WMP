import { contextBridge, ipcRenderer } from 'electron'

contextBridge.exposeInMainWorld('api', {
  getConfig: () => ipcRenderer.invoke('get-config'),
  saveConfig: (cfg: unknown) => ipcRenderer.invoke('save-config', cfg),
  selectGif: () => ipcRenderer.invoke('select-gif'),
  getCharacters: () => ipcRenderer.invoke('get-characters'),
  openCharactersFolder: () => ipcRenderer.invoke('open-characters-folder'),
  setAutostart: (enable: boolean) => ipcRenderer.invoke('set-autostart', enable),
  getAutostart: () => ipcRenderer.invoke('get-autostart'),
  launchOverlay: () => ipcRenderer.invoke('launch-overlay'),
  minimize: () => ipcRenderer.invoke('window-minimize'),
  close: () => ipcRenderer.invoke('window-close'),
  onConfigChanged: (cb: (cfg: unknown) => void) => {
    ipcRenderer.on('config-changed', (_, cfg) => cb(cfg))
    return () => ipcRenderer.removeAllListeners('config-changed')
  }
})
