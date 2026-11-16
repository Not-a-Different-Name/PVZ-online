# MOD115 sources

MOD115 fixes spectator admission from the menu and read-only watch screen, rearms host ACK ownership only for newly admitted fighters, and preserves endless lawns while catch-up completes before deck selection. Both host and participant use build 115; older builds have separate matchmaking pools. The wire payload and save formats are unchanged, and the existing relay remains compatible. SOURCE-MAP.json binds this release to the accepted snapshot.

PC: VS2022 MSVC x86, CMake/Ninja Release. Extract Source-Dependencies.zip beside the checkout to supply third_party. Android: SDK36/min24, NDK28.2.13676358, arm64, CMake3.22.1 and Gradle8.12. Licensed dependencies are retained; signed full player packages are in Releases. No player data, credentials or signing keys are included.
