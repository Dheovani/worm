# Compiler support

Worm requires C++20 and treats a compiler family as supported only while its minimum and current lanes build and run the test suite in CI. The version numbers below identify compiler major releases or Xcode releases; patch updates supplied by the pinned runner image remain supported within the same lane.

## Supported matrix

| Platform | Minimum tested | Current tested | CI environment |
| --- | --- | --- | --- |
| Linux with GCC | GCC 13 | GCC 14 | Ubuntu 24.04 |
| Linux with Clang and libstdc++ | Clang 16 | Clang 18 | Ubuntu 24.04 |
| Windows with MSVC | MSVC 19.44 | MSVC 19.44 | Windows Server 2022 |
| macOS with AppleClang | Xcode 15.4 | Xcode 16.4 | macOS 14 and macOS 15 |

The MSVC minimum and current lanes currently coincide because the Windows runner provides one actively serviced Visual Studio 2022 toolset. A new MSVC release becomes the current tested version after the existing lane passes with it; the minimum changes only through an explicit compatibility decision.

## Support policy

- The minimum lane is a compatibility contract. Code changes must continue to compile and pass tests there unless a documented change deliberately raises the requirement.
- The current lane detects newly introduced compiler diagnostics and standard-library compatibility changes before they become the next minimum.
- Runner operating systems and compiler selections are pinned in the workflow. Moving a lane to a new runner operating system or compiler major version requires updating this document in the same pull request.
- Compilers older than the listed minimums may work, but they are not supported because the project does not test them.
- Native database libraries remain optional. The compiler matrix exercises the configured default drivers, while the independent optional-driver matrix verifies disabled-driver and single-driver configurations.

## Changing the matrix

Raise a minimum only when keeping the earlier compiler would block a required language, standard-library, dependency, security, or maintenance improvement. The change must update CI, this document, the changelog, and the upgrade guide when consumers need to change their toolchain. Record the reason and migration impact in the pull request; use a technical decision when the change establishes a broader portability boundary.
