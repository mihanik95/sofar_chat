#!/bin/sh
set -euo pipefail

echo Running codesign --verbose=4 --force --sign - \"${CONFIGURATION_BUILD_DIR}/${FULL_PRODUCT_NAME}\"
codesign --verbose=4 --force --sign - "${CONFIGURATION_BUILD_DIR}/${FULL_PRODUCT_NAME}"

echo Running \"${CONFIGURATION_BUILD_DIR}/juce_vst3_helper\" -create -version \"0.0086\" -path \"${CONFIGURATION_BUILD_DIR}/${FULL_PRODUCT_NAME}\" -output \"${CONFIGURATION_BUILD_DIR}/${FULL_PRODUCT_NAME}/Contents/Resources/moduleinfo.json\"
"${CONFIGURATION_BUILD_DIR}/juce_vst3_helper" -create -version "0.0086" -path "${CONFIGURATION_BUILD_DIR}/${FULL_PRODUCT_NAME}" -output "${CONFIGURATION_BUILD_DIR}/${FULL_PRODUCT_NAME}/Contents/Resources/moduleinfo.json"

