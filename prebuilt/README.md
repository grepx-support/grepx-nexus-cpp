# Prebuilt nexus_server

`nexus_server-linux-x86_64` - a static build (runs on any 64-bit Linux) of this repo's nexus_server, for servers that
cannot compile it (deploy.sh in grepx-blood-donation uses it when the cmake build fails or is skipped).
Rebuild after changing the server code:

    cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static"
    cmake --build build-static --target nexus_server -j"$(nproc)"
    strip -o prebuilt/nexus_server-linux-x86_64 build-static/apps/nexus_server/nexus_server
