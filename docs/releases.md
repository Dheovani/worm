# Release policy

Worm follows [Semantic Versioning 2.0.0](https://semver.org/) for published releases. The installed C++ headers and `Worm::*` CMake targets, documented CMake options, CLI commands and exit behavior, configuration keys, and explicitly versioned persistent formats form the public contract. ABI compatibility is not promised across releases, so C++ consumers must rebuild when upgrading.

## Version rules

- Development before `1.0.0` uses `0.y.z`; breaking changes are allowed but must be deliberate and recorded in the changelog and upgrade guide.
- Alpha, beta, and release-candidate builds for the stable API use `1.0.0-alpha.N`, `1.0.0-beta.N`, and `1.0.0-rc.N` tags in increasing order.
- After `1.0.0`, incompatible public-contract changes increment MAJOR, backward-compatible features increment MINOR, and backward-compatible fixes increment PATCH.
- A published version and its tag are immutable. Corrections require a new version; packaging-only corrections use the package manager's port revision when available.
- Deprecations after `1.0.0` remain available for at least one MINOR release before removal unless retaining them would preserve a security vulnerability or data-corruption defect.

## Severity gates

A critical defect permits credential disclosure, SQL injection, arbitrary code execution, silent unfiltered mutation, or unrecoverable corruption through a supported workflow. A high-severity defect can produce persistent incorrect data, violate transaction or thread-affinity guarantees, or make a documented supported workflow unusable without a safe workaround. Release gates count only reproducible defects in Worm or its supported packaging, not unavailable third-party services.

## Alpha gate

An alpha release requires all minimum and current compiler lanes and optional-driver configurations to pass at the release commit, the installed-package consumer test to pass, the changelog and public limitations to describe the release, the security policy and license to be present, and zero unresolved critical defects. Alpha APIs may still change.

## Beta gate

A beta release requires the alpha gate plus a frozen documented feature scope for `1.0.0`, documentation for every public API intended for `1.0.0`, service-backed contract coverage for every database advertised as supported by `1.0.0` or an explicit exclusion from that support list, green sanitizer and static-analysis workflows, zero unresolved critical or high-severity defects, and a reviewed compatibility and upgrade policy. New features after beta require resetting the beta sequence unless they close a release-blocking gap without expanding the public contract.

## Release-candidate gate

A release candidate requires the beta gate plus a clean external build that installs Worm and consumes it through `find_package(Worm)` on Windows, Linux, and macOS, a candidate vcpkg port built for the supported triplets, complete release notes, and no known change still planned for the `1.0.0` public contract. A change to that contract resets the release-candidate sequence.

## `1.0.0` gate

The stable release requires two consecutive release candidates separated by at least 14 days, no unresolved critical or high-severity defects, all supported database contracts green at the release commit, successful sanitizer and static-analysis workflows, an immutable tagged source archive, a published and validated vcpkg port, and every item under the roadmap's `Criteria for version 1.0` checked. Conan packaging remains demand-driven and is not a `1.0.0` gate.

## Package publication

The source repository's pinned vcpkg baseline makes dependency resolution repeatable during development, but it does not publish Worm itself. A Worm port requires metadata plus a `portfile.cmake`, and a versioned registry entry points to immutable port contents as described by the [vcpkg ports](https://learn.microsoft.com/en-us/vcpkg/concepts/ports) and [registry](https://learn.microsoft.com/en-us/vcpkg/maintainers/registries) documentation. Create the port from a tagged source archive, verify its SHA-512 digest, build its default and optional features, validate a clean consumer, and then add the version entry with `vcpkg x-add-version`; never publish a port that follows a mutable branch.

## Release checklist

1. Confirm the applicable gate and all required CI jobs are green at the release commit.
2. Move relevant changelog entries from `Unreleased` into the exact version and release date without rewriting earlier entries.
3. Verify the project, manifest, CLI, CMake package version, tag, and package metadata agree once version propagation is implemented.
4. Create and push the signed or annotated immutable tag, then build release artifacts from that tag.
5. Publish and validate the vcpkg port when required by the gate, and record any packaging-only revision separately from the Worm version.
