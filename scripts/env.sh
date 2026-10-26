# Source this file to use the user-local dependency prefix (see docs/ENVIRONMENT.md):
#   source scripts/env.sh
HEX_DEPS_PREFIX="${HEX_DEPS_PREFIX:-$HOME/.local/hexdeps/root/usr}"
export VK_LAYER_PATH="$HEX_DEPS_PREFIX/share/vulkan/explicit_layer.d"
export LD_LIBRARY_PATH="$HEX_DEPS_PREFIX/lib/aarch64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$HEX_DEPS_PREFIX/bin:$PATH"
