import { existsSync } from 'node:fs';
import { delimiter, join } from 'node:path';

export function browserExecutable() {
  if (process.env.FOLIA_BROWSER) return process.env.FOLIA_BROWSER;
  const windows = process.platform === 'win32';
  if (windows) {
    for (const root of [process.env['ProgramFiles(x86)'], process.env.ProgramFiles, process.env.LOCALAPPDATA]) {
      if (!root) continue;
      const edge = join(root, 'Microsoft', 'Edge', 'Application', 'msedge.exe');
      if (existsSync(edge)) return edge;
    }
  }
  const names = windows ? ['msedge.exe', 'chrome.exe'] : ['chromium', 'chromium-browser', 'google-chrome'];
  for (const directory of (process.env.PATH || '').split(delimiter).filter(Boolean)) {
    for (const name of names) {
      const candidate = join(directory, name);
      if (existsSync(candidate)) return candidate;
    }
  }
  // Playwright can use its own installed Chromium when no system browser is available.
  return undefined;
}
