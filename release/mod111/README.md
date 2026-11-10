# MOD111 sources

MOD111 adds four independent spectator slots, host-approved admission between stages, spectator-first reconnects, and ongoing public room listings. It includes eleven plant buffs and fixes for watching, waiting after team buffs, and Endless settlement. Full native PC and MuMu Apps passed 426 and 685 regression checks. Cross-platform room and Endless tests use a controlled isolated relay. Both public relay nodes passed old-build and MOD111 private-room protocol tests. The full Android package was rebuilt with Gradle using the same signing identity. SOURCE-MAP.json records the accepted files and keeps the previous remote history.

PC: VS2022 MSVC x86, CMake/Ninja Release. Extract Source-Dependencies.zip beside the checkout to supply third_party. Android: SDK36/min24, NDK28.2.13676358, arm64, CMake3.22.1 and Gradle8.12. Licensed dependencies are retained; signed full player packages are in Releases. No player data, credentials or signing keys are included.
