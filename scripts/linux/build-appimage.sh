#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-}"
INSTALL_DIR="${2:-}"
OUTPUT_DIR="${3:-}"
APPDIR="${4:-}"

if [[ -z "${BUILD_DIR}" ]]; then
	echo "usage: $0 <build-dir> [install-dir] [output-dir] [appdir]" >&2
	exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
INSTALL_DIR="${INSTALL_DIR:-${BUILD_DIR}/appimage-install}"
OUTPUT_DIR="${OUTPUT_DIR:-${BUILD_DIR}/appimage-out}"
APPDIR="${APPDIR:-${BUILD_DIR}/CloudCompare.AppDir}"

LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"
DESKTOP_FILE="${SCRIPT_DIR}/org.cloudcompare.CloudCompare.desktop"
APPDATA_FILE="${SCRIPT_DIR}/org.cloudcompare.CloudCompare.appdata.xml"
ICON_FILE="${REPO_DIR}/qCC/images/icon/cc_icon_256.png"

if ! command -v "${LINUXDEPLOY}" >/dev/null 2>&1; then
	echo "linuxdeploy was not found. Set LINUXDEPLOY to its full path or install it first." >&2
	exit 1
fi

if [[ ! -f "${DESKTOP_FILE}" || ! -f "${APPDATA_FILE}" || ! -f "${ICON_FILE}" ]]; then
	echo "Missing AppImage asset files." >&2
	exit 1
fi

rm -rf "${INSTALL_DIR}" "${OUTPUT_DIR}" "${APPDIR}"
mkdir -p "${INSTALL_DIR}" "${OUTPUT_DIR}" "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/metainfo" "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cmake --install "${BUILD_DIR}" --prefix "${INSTALL_DIR}"
cp -a "${INSTALL_DIR}/." "${APPDIR}/usr/"
cp "${DESKTOP_FILE}" "${APPDIR}/usr/share/applications/org.cloudcompare.CloudCompare.desktop"
cp "${APPDATA_FILE}" "${APPDIR}/usr/share/metainfo/org.cloudcompare.CloudCompare.appdata.xml"
cp "${ICON_FILE}" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/cloudcompare.png"

mkdir -p "${OUTPUT_DIR}"
pushd "${OUTPUT_DIR}" >/dev/null
"${LINUXDEPLOY}" \
	--appdir "${APPDIR}" \
	--desktop-file "${APPDIR}/usr/share/applications/org.cloudcompare.CloudCompare.desktop" \
	--icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/cloudcompare.png" \
	--appstream-file "${APPDIR}/usr/share/metainfo/org.cloudcompare.CloudCompare.appdata.xml" \
	--plugin qt \
	--output appimage
popd >/dev/null

echo "AppImage created in ${OUTPUT_DIR}"
