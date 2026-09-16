#!/usr/bin/env bash
set -euo pipefail

# iDotMatrix ESP32 Emulator update/build helper
#
# Workflow:
#   1. Select newest generated source ZIP from the Windows Downloads folder.
#   2. Extract it to a temporary directory.
#   3. Synchronize it into the Git working repository while preserving .git/.pio.
#   4. Verify that the MatrixPortal persistent serial device is attached.
#   5. Compile the selected PlatformIO environment only.
#   6. Verify the generated firmware signature.
#   7. Re-check the MatrixPortal serial device before upload. This intentionally
#      gives WSL/usbip users a chance to re-bind/re-attach the board if it moved
#      from the runtime/data endpoint to the programming/JTAG endpoint.
#   8. Upload the already compiled firmware.
#   9. Force a post-upload USB detach/reattach cycle and verify runtime serial.
#  10. Open the MatrixPortal S3 serial monitor through its persistent /dev/serial/by-id path.
#
# Every operational step requires explicit confirmation.

ARCHIVE_DIR="${ARCHIVE_DIR:-/mnt/c/Users/PJ/Downloads}"
ARCHIVE_PATTERN="${ARCHIVE_PATTERN:-IDotMatrix-ESP32-Emulator-*.zip}"

REPO="${REPO:-$HOME/repo/idotmatrix-esp32-emulator}"
PIO_ENV="${PIO_ENV:-matrixportal_s3_hub75_64}"

SERIAL_PORT_OVERRIDE="${SERIAL_PORT:-}"
SERIAL_PORT=""
SERIAL_PATTERN="${SERIAL_PATTERN:-/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3*-if00}"
SERIAL_BAUD="${SERIAL_BAUD:-115200}"

confirm() {
    local prompt="$1"
    local reply
    read -r -p "$prompt [Y/n] " reply
    case "${reply:-Y}" in
        Y|y|YES|Yes|yes) return 0 ;;
        *) return 1 ;;
    esac
}

print_command() {
    printf '    '
    printf '%q ' "$@"
    echo
}

run_step() {
    local description="$1"
    shift

    echo
    echo "==> $description"
    print_command "$@"

    if confirm "Continue?"; then
        "$@"
    else
        echo "Skipped."
        return 2
    fi
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
        echo "Multiple MatrixPortal serial devices match:" >&2
        printf '  %s\n' "${matches[@]}" >&2
        echo "Set SERIAL_PORT explicitly to choose one." >&2
        return 2
    fi
    return 1
}

any_serial_present() {
    local -a matches=()
    mapfile -t matches < <(serial_matches)
    [[ ${#matches[@]} -gt 0 ]]
}

wait_for_serial_detach() {
    while any_serial_present; do
        echo
        echo "A MatrixPortal serial device is still attached to Linux/WSL."
        if [[ -n "$SERIAL_PORT" ]]; then
            echo "  Current port: $SERIAL_PORT"
        else
            echo "  Pattern: $SERIAL_PATTERN"
        fi
        echo
        echo "Detach the MatrixPortal USB device from Linux/WSL, then retry."
        if ! confirm "Retry after detaching the MatrixPortal?"; then
            echo "Aborted while waiting for the post-upload USB detach."
            exit 0
        fi
    done

    SERIAL_PORT=""
    echo
    echo "MatrixPortal serial device detached."
}

wait_for_serial_port() {
    while true; do
        local status=0
        if resolve_serial_port; then
            echo
            echo "MatrixPortal serial device found:"
            echo "  $SERIAL_PORT"
            echo "  -> $(readlink -f "$SERIAL_PORT" 2>/dev/null || true)"
            return 0
        else
            status=$?
        fi

        echo
        if [[ $status -eq 2 ]]; then
            echo "More than one matching MatrixPortal serial device is present."
        else
            echo "MatrixPortal serial device not found."
            if [[ -n "$SERIAL_PORT_OVERRIDE" ]]; then
                echo "  Expected: $SERIAL_PORT_OVERRIDE"
            else
                echo "  Pattern : $SERIAL_PATTERN"
            fi
        fi
        echo
        echo "Attach the MatrixPortal USB device from Windows to Linux, then retry."
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
command -v realpath >/dev/null 2>&1 || die "realpath not found"
command -v strings >/dev/null 2>&1 || die "strings not found (install binutils)"

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
echo "  Serial pattern    : $SERIAL_PATTERN"
if [[ -n "$SERIAL_PORT_OVERRIDE" ]]; then
    echo "  Serial override   : $SERIAL_PORT_OVERRIDE"
fi
echo "  Serial current    : ${SERIAL_PORT:-currently unavailable}"
echo "  Serial target     : ${SERIAL_TARGET:-currently unavailable}"
echo "  Serial baud       : $SERIAL_BAUD"
echo
echo "Important:"
echo "  .git/ and .pio/ are preserved during repository synchronization."
echo "  Compile and upload are separate operations."
echo "  A second mandatory serial check runs after compilation and before upload."

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
echo "    Preserved   : .git/ .pio/ serial.log"
echo "    NOTE        : other repository files not present in the archive will be deleted."

if ! confirm "Replace repository source with BUILD ${SOURCE_BUILD:-UNKNOWN}?"; then
    echo "Aborted before repository synchronization."
    exit 0
fi

rsync -a --delete \
    --exclude='.git/' \
    --exclude='.pio/' \
    --exclude='serial.log' \
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
# STEP 5 - Verify serial attachment before compile
# -----------------------------------------------------------------------------
echo
echo "==> [5] Verify MatrixPortal serial attachment before compile"
if ! confirm "Proceed to the MatrixPortal serial check before compilation?"; then
    echo "Aborted before serial verification."
    exit 0
fi
wait_for_serial_port

# -----------------------------------------------------------------------------
# STEP 6 - Compile only
# -----------------------------------------------------------------------------
echo
echo "==> [6] Compile with PlatformIO (no upload)"
echo "    pio run -d '$REPO' -e '$PIO_ENV'"
if confirm "Compile ${REPO_RELEASE:-firmware} BUILD ${REPO_BUILD:-?}?"; then
    pio run -d "$REPO" -e "$PIO_ENV"
else
    echo "Compilation skipped."
fi

FIRMWARE="$REPO/.pio/build/$PIO_ENV/firmware.bin"
if [[ -f "$FIRMWARE" ]]; then
    echo
    echo "Generated firmware:"
    ls -lh "$FIRMWARE"

    EXPECTED_SIGNATURE="IDOTMATRIX_FW=${REPO_RELEASE}-B${REPO_BUILD}"
    if strings "$FIRMWARE" | grep -Fq "$EXPECTED_SIGNATURE"; then
        echo "Firmware signature: $EXPECTED_SIGNATURE [MATCH]"
    else
        die "Compiled firmware does not contain expected signature '$EXPECTED_SIGNATURE'. Upload blocked."
    fi
else
    die "No firmware.bin found at: $FIRMWARE. Compile before upload."
fi

# -----------------------------------------------------------------------------
# STEP 7 - Mandatory serial re-check after compile, before upload
# -----------------------------------------------------------------------------
echo
echo "==> [7] Re-check MatrixPortal serial attachment before upload"
echo "    The board may have changed USB mode/endpoints while preparing the upload."
echo "    If WSL/usbip lost the runtime serial device, re-bind/re-attach it now."
SERIAL_PORT=""
if ! confirm "Proceed to the mandatory post-build serial check?"; then
    echo "Aborted before upload serial verification."
    exit 0
fi
wait_for_serial_port

# -----------------------------------------------------------------------------
# STEP 8 - Upload the compiled firmware
# -----------------------------------------------------------------------------
echo
echo "==> [8] Upload compiled firmware with PlatformIO"
echo "    pio run -d '$REPO' -e '$PIO_ENV' -t upload --upload-port '$SERIAL_PORT'"
if confirm "Upload ${REPO_RELEASE:-firmware} BUILD ${REPO_BUILD:-?} using $SERIAL_PORT?"; then
    # The build has already completed and its signature was verified above.
    # PlatformIO may still perform dependency/timestamp checks for the upload
    # target, but compilation and upload are deliberately separate user steps.
    pio run -d "$REPO" -e "$PIO_ENV" -t upload --upload-port "$SERIAL_PORT"
else
    echo "Upload skipped."
fi

# -----------------------------------------------------------------------------
# STEP 9 - Force post-upload USB detach/reattach, then open monitor
# -----------------------------------------------------------------------------
echo
echo "==> [9] Reattach MatrixPortal runtime serial and open monitor"
echo "    Persistent port: $SERIAL_PORT"
echo "    After upload this board can remain on the programming USB endpoint."
echo "    A detach/reattach cycle is therefore required before the runtime monitor."

if confirm "Start the mandatory post-upload USB detach/reattach check?"; then
    wait_for_serial_detach
    echo
    echo "Reattach the MatrixPortal USB device from Windows to Linux/WSL."
    wait_for_serial_port
    if confirm "Open the runtime serial monitor now?"; then
        open_serial_monitor
    else
        echo "Serial monitor skipped."
    fi
else
    echo "Post-upload serial check and monitor skipped."
fi

echo
echo "Workflow completed."
echo "Repository : $REPO"
echo "Release    : ${REPO_RELEASE:-UNKNOWN}"
echo "Build      : ${REPO_BUILD:-UNKNOWN}"
echo "Firmware   : $FIRMWARE"
echo "Serial     : ${SERIAL_PORT:-not attached}"
