# MOD98 sources

This source corresponds to MOD98 fixes LAN spectator token renewal during long sessions and abandoned start attempts that left rooms unable to restart. The player notice now explains current controls and spectator refresh behavior. Authenticated direct pictures, snapshot fallback, team progression and collections remain. SOURCE-MAP.json records the accepted files and keeps the previous remote history.

PC: VS2022 MSVC x86, CMake/Ninja Release. Extract Source-Dependencies.zip beside the checkout to supply third_party. Android: SDK36/min24, NDK28.2.13676358, arm64, CMake3.22.1 and Gradle8.12. Licensed dependencies are retained; signed full player packages are in Releases. No player data, credentials or signing keys are included.
