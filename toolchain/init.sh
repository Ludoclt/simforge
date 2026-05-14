#!/usr/bin/env bash
# simforge/toolchain/init.sh
#
# Usage (inside an existing project):
#   source libs/simforge/toolchain/init.sh
#
# Bootstrap from scratch (curl):
#   curl -fsSL https://raw.githubusercontent.com/Ludoclt/simforge/main/toolchain/init.sh | bash

set -euo pipefail

SIMFORGE_REPO="https://github.com/Ludoclt/simforge.git"
SIMFORGE_DEFAULT_PATH="libs/simforge"

# -------------------------------------------------------------------
# Helpers
# -------------------------------------------------------------------

ask_yes_no()
{
    local prompt="$1"
    local answer
    while true; do
        read -rp "$prompt [y/n]: " answer
        case "$answer" in
            y|Y) return 0 ;;
            n|N) return 1 ;;
            *)   echo "  Please answer y or n." ;;
        esac
    done
}

ask_input()
{
    local prompt="$1"
    local default="$2"
    local answer
    read -rp "$prompt [$default]: " answer
    echo "${answer:-$default}"
}

# -------------------------------------------------------------------
# STEP 1 -- Detect context
# -------------------------------------------------------------------

PROJECT_ROOT="$(pwd)"
IS_GIT_REPO=false
SIMFORGE_TOOLCHAIN_DIR=""
BOOTSTRAP_MODE=false

if git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree &>/dev/null; then
    IS_GIT_REPO=true
fi

if [ -n "${BASH_SOURCE[0]+x}" ] && [ "${BASH_SOURCE[0]}" != "$0" ]; then
    SIMFORGE_TOOLCHAIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    BOOTSTRAP_MODE=true
fi

echo ""
echo "[simforge] Toolchain setup"
echo "-------------------------------------------"
echo "  Project root : $PROJECT_ROOT"
echo "  Git repo     : $IS_GIT_REPO"
echo "  Bootstrap    : $BOOTSTRAP_MODE"
echo "-------------------------------------------"
echo ""

# -------------------------------------------------------------------
# STEP 2 -- Clone / add as submodule (bootstrap mode only)
# -------------------------------------------------------------------

if [ "$BOOTSTRAP_MODE" = true ]; then

    if [ "$IS_GIT_REPO" = true ]; then
        echo "This project is a git repository."
        echo "simforge can be added as a git submodule (recommended)"
        echo "or cloned as a plain directory."
        echo ""

        if ask_yes_no "Add simforge as a git submodule?"; then

            SUBMODULE_PATH=$(ask_input "Submodule path" "$SIMFORGE_DEFAULT_PATH")

            # Check if already registered in .gitmodules
            if git -C "$PROJECT_ROOT" submodule status "$SUBMODULE_PATH" &>/dev/null; then
                echo "[simforge] Submodule already registered, updating..."
                git -C "$PROJECT_ROOT" submodule update --init --recursive "$SUBMODULE_PATH"
            elif [ -d "$PROJECT_ROOT/$SUBMODULE_PATH" ]; then
                # Directory exists but not a submodule -- do not touch it
                echo "[simforge] Directory $SUBMODULE_PATH already exists but is not a submodule."
                if ask_yes_no "Remove it and add as submodule?"; then
                    rm -rf "$PROJECT_ROOT/$SUBMODULE_PATH"
                    git -C "$PROJECT_ROOT" submodule add "$SIMFORGE_REPO" "$SUBMODULE_PATH"
                    git -C "$PROJECT_ROOT" submodule update --init --recursive
                else
                    echo "[simforge] Skipping submodule setup, using existing directory."
                fi
            else
                echo "[simforge] Adding submodule at $SUBMODULE_PATH ..."
                git -C "$PROJECT_ROOT" submodule add "$SIMFORGE_REPO" "$SUBMODULE_PATH"
                git -C "$PROJECT_ROOT" submodule update --init --recursive
                echo "[simforge] Submodule added."
            fi

            SIMFORGE_TOOLCHAIN_DIR="$PROJECT_ROOT/$SUBMODULE_PATH/toolchain"

        else
            CLONE_PATH=$(ask_input "Clone path" "$SIMFORGE_DEFAULT_PATH")

            if [ -d "$PROJECT_ROOT/$CLONE_PATH" ]; then
                echo "[simforge] Directory $CLONE_PATH already exists, skipping clone."
            else
                echo "[simforge] Cloning simforge into $CLONE_PATH ..."
                git clone "$SIMFORGE_REPO" "$PROJECT_ROOT/$CLONE_PATH"
                echo "[simforge] Clone done."
            fi

            SIMFORGE_TOOLCHAIN_DIR="$PROJECT_ROOT/$CLONE_PATH/toolchain"
        fi

    else
        echo "This directory is not a git repository."
        echo "simforge will be cloned as a plain directory."
        echo ""

        CLONE_PATH=$(ask_input "Clone path" "$SIMFORGE_DEFAULT_PATH")

        if [ -d "$PROJECT_ROOT/$CLONE_PATH" ]; then
            echo "[simforge] Directory $CLONE_PATH already exists, skipping clone."
        else
            echo "[simforge] Cloning simforge into $CLONE_PATH ..."
            git clone "$SIMFORGE_REPO" "$PROJECT_ROOT/$CLONE_PATH"
            echo "[simforge] Clone done."
        fi

        SIMFORGE_TOOLCHAIN_DIR="$PROJECT_ROOT/$CLONE_PATH/toolchain"
    fi
fi

# -------------------------------------------------------------------
# STEP 3 -- Create or update .simforge.env
# -------------------------------------------------------------------

ENV_FILE="$PROJECT_ROOT/.simforge.env"
ENV_EXAMPLE="$SIMFORGE_TOOLCHAIN_DIR/.env.example"

if [ ! -f "$ENV_FILE" ]; then
    echo ""
    echo "[simforge] Configuring environment..."
    echo ""

    PROJECT_NAME=$(ask_input "Project name" "$(basename "$PROJECT_ROOT")")
    BUILD_TYPE=$(ask_input "Default CMake build type (Release/Debug/RelWithDebInfo)" "Release")

    cat > "$ENV_FILE" << EOF
# Simforge toolchain configuration
# Generated by init.sh -- edit as needed

PROJECT_ROOT=$PROJECT_ROOT
PROJECT_NAME=$PROJECT_NAME
CMAKE_BUILD_TYPE=$BUILD_TYPE
DISPLAY=${DISPLAY:-:0}
EOF

    echo "[simforge] Created $ENV_FILE"

else
    echo "[simforge] $ENV_FILE already exists, checking for missing keys..."

    if [ -f "$ENV_EXAMPLE" ]; then
        ADDED=0
        while IFS= read -r line; do
            # Skip comments and empty lines
            [[ "$line" =~ ^#.*$ || -z "$line" ]] && continue

            KEY="${line%%=*}"
            if ! grep -q "^${KEY}=" "$ENV_FILE"; then
                echo "[simforge] Adding missing key: $KEY"
                echo "$line" >> "$ENV_FILE"
                ADDED=$((ADDED + 1))
            fi
        done < "$ENV_EXAMPLE"

        if [ "$ADDED" -eq 0 ]; then
            echo "[simforge] No missing keys, environment is up to date."
        else
            echo "[simforge] Added $ADDED missing key(s) to $ENV_FILE"
        fi
    else
        echo "[simforge] No .env.example found, skipping key sync."
    fi
fi

# -------------------------------------------------------------------
# STEP 4 -- .gitignore
# -------------------------------------------------------------------

if [ "$IS_GIT_REPO" = true ]; then
    GITIGNORE="$PROJECT_ROOT/.gitignore"
    if ! grep -q "\.simforge\.env" "$GITIGNORE" 2>/dev/null; then
        echo ".simforge.env" >> "$GITIGNORE"
        echo "[simforge] Added .simforge.env to .gitignore"
    fi
fi

# -------------------------------------------------------------------
# STEP 5 -- Shell functions
# -------------------------------------------------------------------

COMPOSE_CMD="docker compose \
    --env-file $ENV_FILE \
    -f $SIMFORGE_TOOLCHAIN_DIR/compose.yaml"

simforge-up()
{
    xhost +local:docker 2>/dev/null || true
    $COMPOSE_CMD up -d --build
}

simforge-shell()
{
    xhost +local:docker 2>/dev/null || true
    $COMPOSE_CMD up -d
    docker exec -it "$(basename "$PROJECT_ROOT")-toolchain" /bin/bash
}

simforge-down()
{
    $COMPOSE_CMD down
}

simforge-rebuild()
{
    xhost +local:docker 2>/dev/null || true
    $COMPOSE_CMD down
    $COMPOSE_CMD up -d --build
}

export -f simforge-up simforge-shell simforge-down simforge-rebuild

# -------------------------------------------------------------------
# STEP 6 -- Suggest permanent setup
# -------------------------------------------------------------------

echo ""
echo "[simforge] Setup complete. Available commands:"
echo "  simforge-up       -- build and start the container"
echo "  simforge-shell    -- open a shell inside the container"
echo "  simforge-down     -- stop the container"
echo "  simforge-rebuild  -- rebuild the image and restart"
echo ""

if ask_yes_no "Add simforge init to your shell rc file for permanent access?"; then

    RC_FILE=""
    case "$SHELL" in
        */zsh)  RC_FILE="$HOME/.zshrc" ;;
        */bash) RC_FILE="$HOME/.bashrc" ;;
        *)
            RC_FILE=$(ask_input "Shell rc file not detected, enter path" "$HOME/.bashrc")
            ;;
    esac

    SOURCE_LINE="source $SIMFORGE_TOOLCHAIN_DIR/init.sh"

    if ! grep -qF "$SOURCE_LINE" "$RC_FILE" 2>/dev/null; then
        echo "" >> "$RC_FILE"
        echo "# simforge toolchain -- $(basename "$PROJECT_ROOT")" >> "$RC_FILE"
        echo "$SOURCE_LINE" >> "$RC_FILE"
        echo "[simforge] Added to $RC_FILE"
        echo "[simforge] Run 'source $RC_FILE' or open a new terminal to apply."
    else
        echo "[simforge] Already present in $RC_FILE, skipping."
    fi
fi

echo ""
echo "[simforge] Done."
echo ""
