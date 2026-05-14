#!/usr/bin/env bash
# simforge/toolchain/env.sh
#
# Usage (without direnv):
#   source libs/simforge/toolchain/env.sh
#
# Or add to your .bashrc / .zshrc:
#   source /path/to/project/libs/simforge/toolchain/env.sh

SIMFORGE_TOOLCHAIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMFORGE_BIN_DIR="$SIMFORGE_TOOLCHAIN_DIR/bin"

if [[ ":$PATH:" != *":$SIMFORGE_BIN_DIR:"* ]]; then
    export PATH="$SIMFORGE_BIN_DIR:$PATH"
fi
