# MOD116 sources

MOD116 preserves the newest combat entry when spectator and fighter role messages arrive together, closes obsolete watch subscriptions after roster-only promotion, and recovers the menu when a watch session is released. It also includes the previously merged scene backdrops and Fourfold Garden. Both host and participant use build 116; older builds have separate matchmaking pools. The wire payload and save formats are unchanged, and the existing relay remains compatible. SOURCE-MAP.json binds this release to the accepted snapshot.

PC: VS2022 MSVC x86, CMake/Ninja Release. Extract Source-Dependencies.zip beside the checkout to supply third_party. Android: SDK36/min24, NDK28.2.13676358, arm64, CMake3.22.1 and Gradle8.12. Licensed dependencies are retained; signed full player packages are in Releases. No player data, credentials or signing keys are included.
