import * as fs   from 'fs';
import * as path from 'path';
import type { PackageMetadata, InstalledPackage } from './types.js';

const PACKAGE_JSON = 'package.json';
const INDEX_FILE   = 'installed.json';

export class PackageManager {
  constructor(private readonly libraryDir: string) {
    if (!fs.existsSync(libraryDir)) {
      fs.mkdirSync(libraryDir, { recursive: true });
    }
  }

  // ── Query ──────────────────────────────────────────────────────────────────

  listInstalled(): InstalledPackage[] {
    const indexPath = path.join(this.libraryDir, INDEX_FILE);
    if (!fs.existsSync(indexPath)) return [];

    try {
      return JSON.parse(fs.readFileSync(indexPath, 'utf8')) as InstalledPackage[];
    } catch {
      return [];
    }
  }

  isInstalled(name: string): boolean {
    return this.listInstalled().some(p => p.name === name);
  }

  getInstalled(name: string): InstalledPackage | null {
    return this.listInstalled().find(p => p.name === name) ?? null;
  }

  // ── Install from local directory ───────────────────────────────────────────

  installLocal(sourcePath: string): InstalledPackage {
    const metaPath = path.join(sourcePath, PACKAGE_JSON);
    if (!fs.existsSync(metaPath)) {
      throw new Error(`No package.json found in: ${sourcePath}`);
    }

    const meta = JSON.parse(fs.readFileSync(metaPath, 'utf8')) as PackageMetadata;
    const destPath = path.join(this.libraryDir, meta.name);

    if (fs.existsSync(destPath)) {
      this.remove(meta.name);
    }

    // Copy package directory
    this.copyDir(sourcePath, destPath);

    const installed: InstalledPackage = {
      ...meta,
      installPath: destPath,
      installedAt: new Date().toISOString(),
    };

    this.saveToIndex(installed);
    return installed;
  }

  // ── Remove ─────────────────────────────────────────────────────────────────

  remove(name: string): void {
    const pkg = this.getInstalled(name);
    if (!pkg) return;

    if (fs.existsSync(pkg.installPath)) {
      fs.rmSync(pkg.installPath, { recursive: true, force: true });
    }

    const packages = this.listInstalled().filter(p => p.name !== name);
    this.writeIndex(packages);
  }

  // ── Search ─────────────────────────────────────────────────────────────────

  search(query: string): InstalledPackage[] {
    const q = query.toLowerCase();
    return this.listInstalled().filter(p =>
      p.name.toLowerCase().includes(q) ||
      p.description.toLowerCase().includes(q) ||
      p.keywords.some(k => k.toLowerCase().includes(q))
    );
  }

  // ── Private ────────────────────────────────────────────────────────────────

  private saveToIndex(pkg: InstalledPackage): void {
    const packages = this.listInstalled().filter(p => p.name !== pkg.name);
    packages.push(pkg);
    this.writeIndex(packages);
  }

  private writeIndex(packages: InstalledPackage[]): void {
    const indexPath = path.join(this.libraryDir, INDEX_FILE);
    fs.writeFileSync(indexPath, JSON.stringify(packages, null, 2), 'utf8');
  }

  private copyDir(src: string, dest: string): void {
    fs.mkdirSync(dest, { recursive: true });
    for (const entry of fs.readdirSync(src, { withFileTypes: true })) {
      const srcPath  = path.join(src,  entry.name);
      const destPath = path.join(dest, entry.name);
      if (entry.isDirectory()) {
        this.copyDir(srcPath, destPath);
      } else {
        fs.copyFileSync(srcPath, destPath);
      }
    }
  }
}
