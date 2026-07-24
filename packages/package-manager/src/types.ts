export interface PackageMetadata {
  name: string;
  version: string;
  description: string;
  author: string;
  license: string;
  dependencies: string[];
  targets: string[];        // e.g. ['avr', 'arm']
  keywords: string[];
  repository?: string;
}

export interface InstalledPackage extends PackageMetadata {
  installPath: string;
  installedAt: string;      // ISO date string
}

export interface PackageRegistry {
  packages: PackageMetadata[];
  lastUpdated: string;
}
