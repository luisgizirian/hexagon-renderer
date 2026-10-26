#!/usr/bin/env bash
# Extract the build/validation dependencies into a user-local prefix without root.
# Equivalent system-wide alternative (needs sudo):
#   sudo apt install glslang-tools vulkan-validationlayers libglfw3-dev
set -euo pipefail
ROOT="${HEX_DEPS_ROOT:-$HOME/.local/hexdeps}"
PKGS=(glslang-tools vulkan-validationlayers libglfw3 libglfw3-dev)
mkdir -p "$ROOT/debs" "$ROOT/root"
cd "$ROOT/debs"
apt-get download "${PKGS[@]}"
for deb in *.deb; do dpkg -x "$deb" "$ROOT/root"; done
# The packaged layer manifest uses a bare library name; point it at the extracted library.
LAYER_DIR="$ROOT/root/usr/share/vulkan/explicit_layer.d"
LIB="$ROOT/root/usr/lib/aarch64-linux-gnu/libVkLayer_khronos_validation.so"
sed -i "s|\"library_path\": \"[^\"]*\"|\"library_path\": \"$LIB\"|" "$LAYER_DIR/VkLayer_khronos_validation.json"
echo "Installed into $ROOT/root/usr. Configure with -DHEX_DEPS_PREFIX=$ROOT/root/usr and 'source scripts/env.sh'."
