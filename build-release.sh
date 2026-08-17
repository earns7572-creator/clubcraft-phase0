#!/usr/bin/env bash
# Club Craft macOS Universal Release builder.
#
# Required: macOS, Xcode Command Line Tools (or Xcode), CMake, Ninja, and JUCE/
# Optional signing: export CLUBCRAFT_CODESIGN_IDENTITY='Developer ID Application: ...'
# Optional notarization: export CLUBCRAFT_NOTARY_PROFILE='notarytool-keychain-profile'

set -Eeuo pipefail

readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${PROJECT_ROOT}/build-release-universal"
readonly DIST_DIR="${PROJECT_ROOT}/dist"
readonly LOG_FILE="${DIST_DIR}/build-release.log"
readonly BUILD_AU="${CLUBCRAFT_BUILD_AU:-1}"
readonly CODESIGN_IDENTITY="${CLUBCRAFT_CODESIGN_IDENTITY:-}"
readonly NOTARY_PROFILE="${CLUBCRAFT_NOTARY_PROFILE:-}"

BUILD_JOBS="${CLUBCRAFT_BUILD_JOBS:-}"
if [[ -z "${BUILD_JOBS}" ]]; then
    BUILD_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || printf '2')"
fi

fail() {
    printf '\n[Club Craft Release] ERROR: %s\n' "$*" >&2
    exit 1
}

note() {
    printf '[Club Craft Release] %s\n' "$*"
}

on_error() {
    local line="$1"
    local command="$2"
    printf '\n[Club Craft Release] FAILED at line %s\n' "${line}" >&2
    printf '[Club Craft Release] Command: %s\n' "${command}" >&2
    printf '[Club Craft Release] Full log: %s\n' "${LOG_FILE}" >&2
}

trap 'on_error "$LINENO" "$BASH_COMMAND"' ERR

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

bundle_binary() {
    local bundle="$1"
    local executable_directory="${bundle}/Contents/MacOS"
    [[ -d "${executable_directory}" ]] || fail "Bundle has no macOS executable directory: ${bundle}"

    local binary
    binary="$(find "${executable_directory}" -maxdepth 1 -type f -print -quit)"
    [[ -n "${binary}" ]] || fail "Bundle has no macOS executable: ${bundle}"
    printf '%s\n' "${binary}"
}

verify_universal_bundle() {
    local bundle="$1"
    local binary
    binary="$(bundle_binary "${bundle}")"

    local architectures
    architectures="$(lipo -archs "${binary}")"
    note "$(basename "${bundle}"): ${architectures}"

    [[ " ${architectures} " == *" x86_64 "* ]] || fail "Missing x86_64 slice in ${bundle}"
    [[ " ${architectures} " == *" arm64 "* ]] || fail "Missing arm64 slice in ${bundle}"
}

sign_bundle_if_configured() {
    local bundle="$1"

    if [[ -z "${CODESIGN_IDENTITY}" ]]; then
        note "Code signing skipped for $(basename "${bundle}") (CLUBCRAFT_CODESIGN_IDENTITY is not set)."
        return
    fi

    note "Signing $(basename "${bundle}") with configured Developer ID identity."
    codesign --force --deep --options runtime --timestamp --sign "${CODESIGN_IDENTITY}" "${bundle}"
    codesign --verify --deep --strict --verbose=2 "${bundle}"
}

create_archive() {
    rm -f "${ARCHIVE_PATH}"
    ditto -c -k --sequesterRsrc --keepParent "${RELEASE_DIR}" "${ARCHIVE_PATH}"
}

[[ "$(uname -s)" == "Darwin" ]] || fail "This script must run on macOS."
[[ "${BUILD_AU}" == "0" || "${BUILD_AU}" == "1" ]] || fail "CLUBCRAFT_BUILD_AU must be 0 or 1."

require_command cmake
require_command ninja
require_command xcodebuild
require_command lipo
require_command ditto
require_command codesign

[[ -d "${PROJECT_ROOT}/JUCE" ]] || fail "JUCE/ is missing. Run the documented JUCE clone step first."
[[ -f "${PROJECT_ROOT}/CMakeLists.txt" ]] || fail "CMakeLists.txt is missing from ${PROJECT_ROOT}."

if [[ -n "${NOTARY_PROFILE}" && -z "${CODESIGN_IDENTITY}" ]]; then
    fail "Notarization requires CLUBCRAFT_CODESIGN_IDENTITY as well as CLUBCRAFT_NOTARY_PROFILE."
fi

VERSION="$(sed -nE 's/^project\(ClubCraftPhase0 VERSION ([^ ]+).*/\1/p' "${PROJECT_ROOT}/CMakeLists.txt" | head -n 1)"
[[ -n "${VERSION}" ]] || VERSION="0.0.0"

readonly RELEASE_NAME="ClubCraft-${VERSION}-macos-universal"
readonly RELEASE_DIR="${DIST_DIR}/${RELEASE_NAME}"
readonly ARCHIVE_PATH="${DIST_DIR}/${RELEASE_NAME}.zip"
readonly VST3_SOURCE="${BUILD_DIR}/ClubCraft_artefacts/Release/VST3/Club Craft.vst3"
readonly AU_SOURCE="${BUILD_DIR}/ClubCraft_artefacts/Release/AU/Club Craft.component"

rm -rf "${BUILD_DIR}" "${DIST_DIR}"
mkdir -p "${DIST_DIR}"
exec > >(tee -a "${LOG_FILE}") 2>&1

note "Project: ${PROJECT_ROOT}"
note "Clean build directory: ${BUILD_DIR}"
note "Architectures: x86_64;arm64"
note "Build AU: ${BUILD_AU}"
note "Parallel jobs: ${BUILD_JOBS}"

CMAKE_ARGS=(
    -S "${PROJECT_ROOT}"
    -B "${BUILD_DIR}"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    '-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64'
    -DCLUBCRAFT_BUILD_TESTS=ON
    "-DCLUBCRAFT_BUILD_AU=${BUILD_AU}"
)

note "Configuring Release build."
cmake "${CMAKE_ARGS[@]}"

note "Building Release artifacts."
cmake --build "${BUILD_DIR}" --config Release --parallel "${BUILD_JOBS}"

note "Running CTest."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

[[ -d "${VST3_SOURCE}" ]] || fail "VST3 bundle was not generated: ${VST3_SOURCE}"

mkdir -p "${RELEASE_DIR}"

sign_bundle_if_configured "${VST3_SOURCE}"
verify_universal_bundle "${VST3_SOURCE}"
cp -R "${VST3_SOURCE}" "${RELEASE_DIR}/"

BUNDLES=("${RELEASE_DIR}/Club Craft.vst3")

if [[ "${BUILD_AU}" == "1" ]]; then
    if [[ -d "${AU_SOURCE}" ]]; then
        sign_bundle_if_configured "${AU_SOURCE}"
        verify_universal_bundle "${AU_SOURCE}"
        cp -R "${AU_SOURCE}" "${RELEASE_DIR}/"
        BUNDLES+=("${RELEASE_DIR}/Club Craft.component")
    else
        note "AU was requested but no AU bundle was produced; VST3 Release artifact will still be packaged."
    fi
fi

{
    printf 'Club Craft Release Manifest\n'
    printf 'Version: %s\n' "${VERSION}"
    printf 'Architectures: x86_64 arm64\n'
    printf 'Code signing identity: %s\n' "${CODESIGN_IDENTITY:-not configured}"
    printf 'Notarization profile: %s\n\n' "${NOTARY_PROFILE:-not configured}"

    for bundle in "${BUNDLES[@]}"; do
        binary="$(bundle_binary "${bundle}")"
        printf '%s\n' "$(basename "${bundle}")"
        printf '  Binary: %s\n' "${binary}"
        printf '  Architectures: %s\n' "$(lipo -archs "${binary}")"
    done
} > "${RELEASE_DIR}/MANIFEST.txt"

create_archive

if [[ -n "${NOTARY_PROFILE}" ]]; then
    require_command xcrun
    note "Submitting archive for notarization with keychain profile: ${NOTARY_PROFILE}"
    xcrun notarytool submit "${ARCHIVE_PATH}" --keychain-profile "${NOTARY_PROFILE}" --wait

    for bundle in "${BUNDLES[@]}"; do
        note "Stapling notarization ticket to $(basename "${bundle}")."
        xcrun stapler staple "${bundle}"
    done

    create_archive
else
    note "Notarization skipped (CLUBCRAFT_NOTARY_PROFILE is not set)."
fi

printf '\n[Club Craft Release] SUCCESS\n'
printf '[Club Craft Release] Release folder: %s\n' "${RELEASE_DIR}"
printf '[Club Craft Release] ZIP archive:     %s\n' "${ARCHIVE_PATH}"
printf '[Club Craft Release] Build log:       %s\n' "${LOG_FILE}"
