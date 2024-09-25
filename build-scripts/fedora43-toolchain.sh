#!/bin/bash
set -euo pipefail

# Minimal, readable toolchain setup for Fedora 43 distrobox
# Builds TomeNET SDL2 (Linux) and MinGW32 Windows cross binary.
#
# Example commands for setting up build environment and building tomenet (run commands in tomenet root folder):
#
# Creat new build container and install packages needed for building (run only once).
# $ distrobox create --name tomenet-fedora-43 --image registry.fedoraproject.org/fedora:43
# $ distrobox enter tomenet-fedora-43 -- bash ./build-scripts/fedora43-toolchain-compact.sh
# Build tomenet & tomenet.exe executables (should appear in tomenet root folder).
# $ distrobox enter tomenet-fedora-43 -- bash -c 'cd ./src; make -f makefile.sdl2 clean install'
#
# Note: For running the tomenet.exe, you'll need to copy out these DLLs from the container's /usr/i686-w64-mingw32/sys-root/mingw/bin/*.dll directory to the directory where the exe file is runnig from:
# iconv.dll libarchive-13.dll libbz2-1.dll libcrypto-3.dll libcurl-4.dll libfreetype-6.dll libgcc_s_dw2-1.dll libglib-2.0-0.dll libgnurx-0.dll libharfbuzz-0.dll libidn2-0.dll libintl-8.dll libjpeg-62.dll liblzma-5.dll libnettle-8.dll libpcre2-8-0.dll libpng16-16.dll libpsl-5.dll libsharpyuv-0.dll libssh2-1.dll libssl-3.dll libtiff-5.dll libunistring-2.dll libwebp-7.dll libwebpdemux-2.dll libwinpthread-1.dll libxml2-2.dll SDL2.dll SDL2_image.dll SDL2_mixer.dll SDL2_net.dll SDL2_ttf.dll SDL3.dll zlib1.dll
#

# Install necessary and optional packages for building linux and mingw32 versions of sdl2 tomenet client (and SDL2_net).
sudo dnf -y --refresh install \
  @development-tools clang autoconf automake libtool wget \
  SDL2-devel SDL2_net-devel SDL2_ttf-devel SDL2_mixer-devel SDL2_image-devel \
  libcurl-devel openssl-devel libarchive-devel \
  mingw32-gcc mingw32-gcc-c++ mingw32-binutils mingw32-pkg-config \
  mingw32-sdl2-compat mingw32-SDL2_ttf mingw32-SDL2_image mingw32-SDL2_mixer \
  mingw32-libarchive mingw32-curl mingw32-openssl mingw32-libgnurx

# Build and install SDL_net for MinGW32 (not in Fedora repos).
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
(
	SDL2_NET_VERSION=2.2.0
  cd "$tmpdir"
  wget "https://github.com/libsdl-org/SDL_net/releases/download/release-${SDL2_NET_VERSION}/SDL2_net-${SDL2_NET_VERSION}.tar.gz"
  tar xf "SDL2_net-${SDL2_NET_VERSION}.tar.gz"
  cd "SDL2_net-${SDL2_NET_VERSION}"

  MINGW_TARGET=i686-w64-mingw32
  PREFIX=/usr/$MINGW_TARGET/sys-root/mingw
  export PATH=$PREFIX/bin:$PATH
  export SDL2_CONFIG=$PREFIX/bin/sdl2-config

  ./configure --host="$MINGW_TARGET" --prefix="$PREFIX" \
    --disable-static --disable-sdltest --disable-examples
  make -j"$(nproc)"
  sudo make install
)
