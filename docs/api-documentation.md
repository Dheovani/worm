# API documentation

Worm generates browsable API documentation from its public C++ headers and Markdown guides with Doxygen. Documentation generation is optional and does not add Doxygen to normal builds or make it a dependency of any database driver.

## Requirements

Install Doxygen before configuring the documentation target. Graphviz is optional and adds class and include diagrams when available.

On Windows with WinGet:

```powershell
winget install --id DimitriVanHeesch.Doxygen -e
winget install --id Graphviz.Graphviz -e
```

On Ubuntu or Debian:

```bash
sudo apt-get update
sudo apt-get install doxygen graphviz
```

## Generate the HTML documentation

The configure command must run before the build command because `WormDocs` only exists when `WORM_BUILD_DOCS=ON`. Configure a small documentation-only build so database client libraries are not required.

PowerShell:

```powershell
cmake -S . -B build/docs `
  -DCMAKE_TOOLCHAIN_FILE= `
  -DWORM_BUILD_DOCS=ON `
  -DBUILD_TESTING=OFF `
  -DWORM_BUILD_EXAMPLES=OFF `
  -DWORM_BUILD_CLI=OFF `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=OFF `
  -DWORM_ENABLE_MSSQL=OFF

cmake --build build/docs --target WormDocs
```

Git Bash, Bash, or another POSIX-compatible shell:

```bash
cmake -S . -B build/docs \
  -DCMAKE_TOOLCHAIN_FILE= \
  -DWORM_BUILD_DOCS=ON \
  -DBUILD_TESTING=OFF \
  -DWORM_BUILD_EXAMPLES=OFF \
  -DWORM_BUILD_CLI=OFF \
  -DWORM_ENABLE_POSTGRESQL=OFF \
  -DWORM_ENABLE_MYSQL=OFF \
  -DWORM_ENABLE_SQLITE=OFF \
  -DWORM_ENABLE_MSSQL=OFF

cmake --build build/docs --target WormDocs
```

Open `build/docs/api/html/index.html` in a browser. Rebuild `WormDocs` after changing a public header or guide.

If MSBuild reports that `WormDocs.vcxproj` does not exist, inspect `build/docs/CMakeCache.txt` and rerun the configure command when it contains `WORM_BUILD_DOCS:BOOL=OFF`. If configuration reports `Could NOT find Doxygen`, install Doxygen, open a new terminal so `PATH` is refreshed, and rerun the configure command.

## How the configuration works

The root `WORM_BUILD_DOCS` CMake option controls whether `docs/CMakeLists.txt` is loaded. When enabled, CMake requires a Doxygen executable, substitutes project paths and the Worm version into `docs/Doxyfile.in`, and writes the generated configuration to the build tree. The `WormDocs` target then runs Doxygen without placing generated HTML in the source tree.

The `INPUT`, `FILE_PATTERNS`, and `RECURSIVE` settings select public headers under `src/` and the Markdown documentation. `STRIP_FROM_PATH` removes local absolute paths from rendered file names, while `USE_MDFILE_AS_MAINPAGE` makes the project README the landing page.

`EXTRACT_ALL=YES` keeps the initial reference useful even for public symbols that do not yet contain Doxygen comments. Private members and local implementation classes remain excluded. `WARN_IF_UNDOCUMENTED=NO` deliberately avoids thousands of warnings while documentation is adopted incrementally; malformed documentation, incomplete commands, and missing parameter descriptions on documented functions are still reported.

Only HTML is generated. Search and tree navigation are enabled, LaTeX, man pages, and XML are disabled, and Graphviz diagrams are generated only when CMake discovers `dot`. Warnings are written to `build/docs/api/doxygen-warnings.log` for local inspection and CI artifacts.

## Continuous integration and publishing

The `API documentation` workflow generates and validates `index.html` for pull requests that change source code, documentation, or the Doxygen configuration. Each run uploads the generated HTML as a review artifact retained for 14 days. Generated files are never committed to the repository.

After a documentation change reaches `main`, the same workflow publishes the current HTML through GitHub Pages. Enable `GitHub Actions` as the Pages source once under the repository's **Settings > Pages > Build and deployment** section. Releases do not need to regenerate identical documentation separately; versioned release documentation can be added later when Worm begins guaranteeing stable release lines.

## Document public APIs

Use `///` comments for concise declarations and `/** ... */` when a description needs multiple paragraphs or tags. Document observable contracts rather than implementation details:

```cpp
/// Finds an entity by its persistent identifier.
///
/// @param id Identifier encoded according to the entity primary-key field.
/// @return The managed entity, or `nullptr` when no row matches.
/// @throws QueryExecutionException when the driver cannot execute the query.
[[nodiscard]]
std::shared_ptr<User> find(const std::int64_t& id) const;
```

Useful tags include `@param`, `@return`, `@throws`, `@tparam`, `@note`, and `@warning`. Do not copy private implementation details into the public contract and never place credentials, connection strings, or real application data in examples.
