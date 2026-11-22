<!--
 fileVersion: [3] 22-11-2026 - 07:45

 PUBLISHING.md - how to publish a component to the ESP Component Registry.
 Copyright (C) 2026 Willem Aandewiel (mrWheel)
 SPDX-License-Identifier: GPL-3.0-or-later
-->

# Publish parserDSMR to the ESP Component Registry

The repository and local ESP-IDF app are named `parserDSMR`. The registry
component is named `parserDSMR` in the namespace `mrwheel`. The component
directory `components/parserDSMR/` is self-contained: manifest, CMake
registration, public headers, C sources, registry README, LICENSE
(GPL-3.0-or-later) and the two example projects. The root app and the root
`examples/` folders are for local development in VS Code.

## Release checklist

- `version` in `components/parserDSMR/idf_component.yml` equals
  `PROG_VERSION` in `components/parserDSMR/src/dsmr_parser.c` (without the
  leading `v`). The example manifests reference the same version through
  `mrwheel/parserDSMR: "^<version>"`.
- `repository` and `repository_info.path` point at the public repository
  and at `components/parserDSMR` inside it.
- The host tests (`cd test/host && make run`) and both example programs
  build before every release.
- The namespace `mrwheel` belongs to your publishing account.

## Package locally

Open an ESP-IDF terminal in the repository root, then run:

```sh
cd components/parserDSMR
compote component pack --name parserDSMR
```

Inspect the generated archive in `dist/` (it holds `src/`, `include/`,
`adapter/`, `examples/`, the manifest, the README and the LICENSE, and
nothing else). `dist/` is git-ignored.

## Upload and install

After signing in to the Registry and configuring a publishing profile,
validate first with a dry run from the same component directory:

```sh
compote component upload --name parserDSMR --namespace mrwheel --dry-run
```

Then upload for real:

```sh
compote component upload --name parserDSMR --namespace mrwheel
```

To install the released component from another ESP-IDF project, run:

```sh
idf.py add-dependency "mrwheel/parserDSMR^0.9.2"
```

Use the actual namespace and release version. Keep Registry credentials
outside the project and its archives.

## Examples

`examples/live_uart` and `examples/stored_telegram` inside the component
are standalone ESP-IDF projects; their `main/idf_component.yml` resolves
`mrwheel/parserDSMR` from the registry, so they can only be built after the
component is published. The Registry detects them automatically. During
development use the wrappers in the root `examples/` directory: they build
the same `app_main.c` files against the local checkout.

[Espressif publishing documentation](https://docs.espressif.com/projects/idf-component-manager/en/latest/publish/how_to_upload_component.html)
