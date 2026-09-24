#!/usr/bin/env bash
set -euo pipefail

# iDotMatrix ESP32 Emulator update/build helper
#
# Workflow:
#   1. Select newest generated source ZIP from the Windows Downloads folder.
#   2. Extract it to a temporary directory.
#   3. Synchronize it into the Git working repository while preserving .git/.pio.
#   4. Force the emulator translation unit to rebuild while preserving caches.
#   5. Run the PlatformIO upload target directly (build + programming).  There is
#      intentionally no pre-upload JTAG endpoint check: that USB identity appears
#      only after PlatformIO has already started the programming transition.
#   6. Optionally report the linked firmware signature after upload, then wait for
#      the Adafruit runtime serial endpoint and optionally open the monitor.
#
# Every operational step requires explicit confirmation.

ARCHIVE_DIR="${ARCHIVE_DIR:-/mnt/c/Users/PJ/Downloads}"
ARCHIVE_PATTERN="${ARCHIVE_PATTERN:-IDotMatrix-ESP32-Emulator-*.zip}"

REPO="${REPO:-$HOME/repo/idotmatrix-esp32-emulator}"
PIO_ENV="${PIO_ENV:-matrixportal_s3_hub75_64}"

# PlatformIO itself handles the transient Espressif JTAG/programming identity.
# The helper only resolves the stable Adafruit runtime serial endpoint used by
# the post-upload monitor.
MONITOR_SERIAL_PATTERN="${MONITOR_SERIAL_PATTERN:-/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3_*-if00}"
MONITOR_SERIAL_PORT_OVERRIDE="${MONITOR_SERIAL_PORT:-${SERIAL_PORT:-}}"
SERIAL_ROLE="runtime/monitor"
SERIAL_PORT_OVERRIDE="$MONITOR_SERIAL_PORT_OVERRIDE"
SERIAL_PATTERN="$MONITOR_SERIAL_PATTERN"
SERIAL_PORT=""
SERIAL_BAUD="${SERIAL_BAUD:-115200}"

select_serial_role() {
    [[ "$1" == "monitor" ]] || die "Only the runtime/monitor serial role is resolved by this helper"
    SERIAL_ROLE="runtime/monitor"
    SERIAL_PATTERN="$MONITOR_SERIAL_PATTERN"
    SERIAL_PORT_OVERRIDE="$MONITOR_SERIAL_PORT_OVERRIDE"
    SERIAL_PORT=""
}

confirm() {
    local prompt="$1"
    local reply
    read -r -p "$prompt [Y/n] " reply
    case "${reply:-Y}" in
        Y|y|YES|Yes|yes) return 0 ;;
        *) return 1 ;;
    esac
}

die() {
    echo "ERROR: $*" >&2
    exit 1
}

extract_fw_release() {
    local file="$1"
    sed -n 's/^[[:space:]]*#define[[:space:]]\+FW_RELEASE[[:space:]]\+"\([^"]*\)".*/\1/p' "$file" \
        | head -n 1
}

extract_fw_build() {
    local file="$1"
    sed -n 's/^[[:space:]]*#define[[:space:]]\+FW_BUILD[[:space:]]\+\([0-9][0-9]*\).*/\1/p' "$file" \
        | head -n 1
}

find_source_root() {
    local extracted="$1"
    local -a entries=()

    mapfile -t entries < <(find "$extracted" -mindepth 1 -maxdepth 1 -printf '%p\n')

    if [[ ${#entries[@]} -eq 1 && -d "${entries[0]}" ]]; then
        printf '%s\n' "${entries[0]}"
    else
        # Also supports archives whose project files are stored directly at ZIP root.
        printf '%s\n' "$extracted"
    fi
}


serial_matches() {
    if [[ -n "$SERIAL_PORT_OVERRIDE" ]]; then
        [[ -e "$SERIAL_PORT_OVERRIDE" ]] && printf '%s\n' "$SERIAL_PORT_OVERRIDE"
        return 0
    fi
    compgen -G "$SERIAL_PATTERN" | sort || true
}

resolve_serial_port() {
    local -a matches=()
    mapfile -t matches < <(serial_matches)

    if [[ ${#matches[@]} -eq 1 ]]; then
        SERIAL_PORT="${matches[0]}"
        return 0
    fi

    SERIAL_PORT=""
    if [[ ${#matches[@]} -gt 1 ]]; then
        echo "Multiple MatrixPortal $SERIAL_ROLE devices match:" >&2
        printf '  %s\n' "${matches[@]}" >&2
        echo "Set SERIAL_PORT explicitly to choose one." >&2
        return 2
    fi
    return 1
}

wait_for_serial_port() {
    while true; do
        local status=0
        if resolve_serial_port; then
            echo
            echo "MatrixPortal $SERIAL_ROLE device found:"
            echo "  $SERIAL_PORT"
            echo "  -> $(readlink -f "$SERIAL_PORT" 2>/dev/null || true)"
            return 0
        else
            status=$?
        fi

        echo
        if [[ $status -eq 2 ]]; then
            echo "More than one matching MatrixPortal $SERIAL_ROLE device is present."
        else
            echo "MatrixPortal $SERIAL_ROLE device not found."
            if [[ -n "$SERIAL_PORT_OVERRIDE" ]]; then
                echo "  Expected: $SERIAL_PORT_OVERRIDE"
            else
                echo "  Pattern : $SERIAL_PATTERN"
            fi
        fi
        echo
        echo "Attach/re-bind the MatrixPortal $SERIAL_ROLE USB device from Windows to Linux/WSL, then retry."
        if ! confirm "Retry serial detection?"; then
            echo "Aborted while waiting for the serial device."
            exit 0
        fi
    done
}

open_serial_monitor() {
    while true; do
        wait_for_serial_port

        echo
        echo "Opening PlatformIO serial monitor"
        echo "  Port : $SERIAL_PORT"
        echo "  Baud : $SERIAL_BAUD"
        echo "  Quit : Ctrl+C"
        echo

        # Run from the repository so PlatformIO also picks up monitor_filters
        # configured in platformio.ini for the selected environment.
        (
            cd "$REPO"
            pio device monitor -e "$PIO_ENV" -p "$SERIAL_PORT" -b "$SERIAL_BAUD"
        ) || true

        echo
        echo "Serial monitor closed or the USB serial device disconnected."
        if ! confirm "Wait for the MatrixPortal serial device and reopen the monitor?"; then
            break
        fi
    done
}

command -v unzip >/dev/null 2>&1 || die "unzip not found"
command -v rsync >/dev/null 2>&1 || die "rsync not found"
command -v pio >/dev/null 2>&1 || die "PlatformIO CLI (pio) not found"
command -v git >/dev/null 2>&1 || die "git not found"

[[ -d "$ARCHIVE_DIR" ]] || die "Archive directory not found: $ARCHIVE_DIR"
[[ -d "$REPO/.git" ]] || die "Not a Git repository: $REPO"

ARCHIVE="$(
    find "$ARCHIVE_DIR" -maxdepth 1 -type f -name "$ARCHIVE_PATTERN" \
        -printf '%T@ %p\n' 2>/dev/null \
    | sort -nr \
    | head -n 1 \
    | cut -d' ' -f2-
)"

[[ -n "${ARCHIVE:-}" ]] || die "No archive matching '$ARCHIVE_PATTERN' found in $ARCHIVE_DIR"

select_serial_role monitor
SERIAL_TARGET=""
if resolve_serial_port >/dev/null 2>&1; then
    SERIAL_TARGET="$(readlink -f "$SERIAL_PORT" 2>/dev/null || true)"
fi

echo
echo "Configuration:"
echo "  Archive directory : $ARCHIVE_DIR"
echo "  Selected archive  : $ARCHIVE"
echo "  Repository        : $REPO"
echo "  PlatformIO env    : $PIO_ENV"
echo "  Monitor pattern   : $MONITOR_SERIAL_PATTERN"
if [[ -n "$MONITOR_SERIAL_PORT_OVERRIDE" ]]; then
    echo "  Monitor override  : $MONITOR_SERIAL_PORT_OVERRIDE"
fi
echo "  Runtime current   : ${SERIAL_PORT:-currently unavailable}"
echo "  Runtime target    : ${SERIAL_TARGET:-currently unavailable}"
echo "  Serial baud       : $SERIAL_BAUD"
echo
echo "Important:"
echo "  .git/, .pio/ and src/IDotMatrixUserConfig.h are preserved during repository synchronization."
echo "  Build and upload run as one PlatformIO upload target."
echo "  No JTAG endpoint is checked before upload; it appears only during programming."
echo "  After upload the script waits for the Adafruit runtime endpoint used by the serial monitor."

if ! confirm "Start emulator update workflow?"; then
    echo "Aborted."
    exit 0
fi

TMPDIR_WORK="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_WORK"' EXIT

# -----------------------------------------------------------------------------
# STEP 1 - Extract archive
# -----------------------------------------------------------------------------
echo
echo "==> [1] Extract source archive"
echo "    unzip -q '$ARCHIVE' -d '$TMPDIR_WORK'"
if ! confirm "Extract the selected archive?"; then
    echo "Aborted before extraction."
    exit 0
fi
unzip -q "$ARCHIVE" -d "$TMPDIR_WORK"

SOURCE_DIR="$(find_source_root "$TMPDIR_WORK")"
[[ -d "$SOURCE_DIR" ]] || die "Could not determine extracted source root"

echo "Extracted source root: $SOURCE_DIR"
[[ -f "$SOURCE_DIR/platformio.ini" ]] || die "platformio.ini not found in extracted source: $SOURCE_DIR"
[[ -f "$SOURCE_DIR/src/IDotMatrix.ino" ]] || die "src/IDotMatrix.ino not found in extracted source: $SOURCE_DIR"

SOURCE_RELEASE="$(extract_fw_release "$SOURCE_DIR/src/IDotMatrix.ino")"
SOURCE_BUILD="$(extract_fw_build "$SOURCE_DIR/src/IDotMatrix.ino")"

echo "Archive firmware: ${SOURCE_RELEASE:-UNKNOWN} / BUILD ${SOURCE_BUILD:-UNKNOWN}"

# -----------------------------------------------------------------------------
# STEP 2 - Show repository state before replacement
# -----------------------------------------------------------------------------
echo
echo "==> [2] Inspect current Git working tree"
if confirm "Show current git status before replacing files?"; then
    git -C "$REPO" status --short --branch
else
    echo "Skipped."
fi

# -----------------------------------------------------------------------------
# STEP 3 - Synchronize source into repository
# -----------------------------------------------------------------------------
echo
echo "==> [3] Synchronize extracted source into repository"
echo "    Source      : $SOURCE_DIR/"
echo "    Destination : $REPO/"
echo "    Preserved   : .git/ .pio/ serial.log src/IDotMatrixUserConfig.h"
echo "    NOTE        : other repository files not present in the archive will be deleted."

if ! confirm "Replace repository source with BUILD ${SOURCE_BUILD:-UNKNOWN}?"; then
    echo "Aborted before repository synchronization."
    exit 0
fi

rsync -a --delete \
    --exclude='.git/' \
    --exclude='.pio/' \
    --exclude='serial.log' \
    --exclude='src/IDotMatrixUserConfig.h' \
    "$SOURCE_DIR/" "$REPO/"

[[ -f "$REPO/platformio.ini" ]] || die "platformio.ini missing after synchronization"
[[ -f "$REPO/src/IDotMatrix.ino" ]] || die "IDotMatrix.ino missing after synchronization"

REPO_RELEASE="$(extract_fw_release "$REPO/src/IDotMatrix.ino")"
REPO_BUILD="$(extract_fw_build "$REPO/src/IDotMatrix.ino")"

echo "Repository firmware: ${REPO_RELEASE:-UNKNOWN} / BUILD ${REPO_BUILD:-UNKNOWN}"

if [[ -n "$SOURCE_RELEASE" && "$SOURCE_RELEASE" != "$REPO_RELEASE" ]]; then
    die "Release mismatch after synchronization: archive=$SOURCE_RELEASE repository=$REPO_RELEASE"
fi
if [[ -n "$SOURCE_BUILD" && "$SOURCE_BUILD" != "$REPO_BUILD" ]]; then
    die "Build mismatch after synchronization: archive=$SOURCE_BUILD repository=$REPO_BUILD"
fi

# rsync -a preserves archive timestamps while .pio is intentionally preserved.
# A freshly extracted source can therefore look older than the cached object and
# PlatformIO may reuse the previous firmware object. Force only the emulator
# translation unit to rebuild; framework and library caches remain untouched.
PIO_BUILD_DIR="$REPO/.pio/build/$PIO_ENV"
rm -f \
    "$PIO_BUILD_DIR/src/IDotMatrix.ino.cpp.o" \
    "$PIO_BUILD_DIR/src/IDotMatrix.ino.cpp.d"
touch "$REPO/src/IDotMatrix.ino"

echo "Forced emulator source rebuild while preserving PlatformIO framework/library cache."

# -----------------------------------------------------------------------------
# STEP 4 - Show updated repository state
# -----------------------------------------------------------------------------
echo
echo "==> [4] Inspect updated Git working tree"
if confirm "Show git status after repository synchronization?"; then
    git -C "$REPO" status --short --branch
else
    echo "Skipped."
fi

# -----------------------------------------------------------------------------
# STEP 5 - Upload with PlatformIO (build + program in one operation)
# -----------------------------------------------------------------------------
echo
echo "==> [5] Build and upload with PlatformIO"
echo "    pio run -d '$REPO' -e '$PIO_ENV' -t upload"
echo "    The MatrixPortal changes from the Adafruit runtime USB identity to the"
echo "    Espressif JTAG/programming identity only after PlatformIO starts upload."
echo "    Therefore there is intentionally no pre-upload JTAG serial check."

if confirm "Build and upload ${REPO_RELEASE:-firmware} BUILD ${REPO_BUILD:-?}?"; then
    pio run -d "$REPO" -e "$PIO_ENV" -t upload
else
    echo "Upload skipped."
fi

FIRMWARE="$REPO/.pio/build/$PIO_ENV/firmware.bin"
FIRMWARE_ELF="$REPO/.pio/build/$PIO_ENV/firmware.elf"
if [[ -f "$FIRMWARE" ]]; then
    echo
    echo "Generated firmware artifact:"
    ls -lh "$FIRMWARE"
fi

# Re-read the synchronized identifiers after PlatformIO completes.  This is
# informational only: the upload target has already built and programmed the
# image, so signature checking must not gate the transition into programming.
VERIFY_RELEASE="$(extract_fw_release "$REPO/src/IDotMatrix.ino")"
VERIFY_BUILD="$(extract_fw_build "$REPO/src/IDotMatrix.ino")"
EXPECTED_SIGNATURE="IDOTMATRIX_FW=${VERIFY_RELEASE}-B${VERIFY_BUILD}"
if [[ -f "$FIRMWARE_ELF" ]] && grep -aFq "$EXPECTED_SIGNATURE" "$FIRMWARE_ELF"; then
    echo "Post-upload ELF signature: $EXPECTED_SIGNATURE [MATCH]"
else
    echo "Post-upload ELF signature: not verified (non-blocking)."
fi
REPO_RELEASE="$VERIFY_RELEASE"
REPO_BUILD="$VERIFY_BUILD"

# -----------------------------------------------------------------------------
# STEP 6 - Wait for runtime serial endpoint, then optionally monitor
# -----------------------------------------------------------------------------
echo
echo "==> [6] Wait for MatrixPortal runtime monitor endpoint"
echo "    Runtime pattern: $MONITOR_SERIAL_PATTERN"
echo "    If WSL/Linux lost the USB device during programming, re-attach/re-bind it"
echo "    now so the Adafruit MatrixPortal runtime serial endpoint becomes visible."
select_serial_role monitor
if confirm "Wait for the runtime serial endpoint and optionally open the monitor?"; then
    wait_for_serial_port
    if confirm "Open the runtime serial monitor now?"; then
        open_serial_monitor
    else
        echo "Serial monitor skipped."
    fi
else
    echo "Runtime serial check and monitor skipped."
fi

echo
echo "Workflow completed."
echo "Repository : $REPO"
echo "Release    : ${REPO_RELEASE:-UNKNOWN}"
echo "Build      : ${REPO_BUILD:-UNKNOWN}"
echo "Firmware   : $FIRMWARE"
echo "Monitor    : ${SERIAL_PORT:-not attached}"
