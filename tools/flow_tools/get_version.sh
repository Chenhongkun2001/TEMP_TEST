#!/usr/bin/env bash

set -euo pipefail


TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

REPO_ROOT="$(cd "${TOOLS_DIR}/../.." && pwd)"


# shellcheck source=/dev/null
CONFIG_FILE="${TOOLS_DIR}/release.conf"

if [[ ! -f "${CONFIG_FILE}" ]]; then

    echo "ERROR: config file not found: ${CONFIG_FILE}"
    echo "  ${CONFIG_FILE}"

    exit 1
fi

source "${CONFIG_FILE}"


cd "${REPO_ROOT}"


MODE="${1:-dotted}"


if [[ ! -f "${GLOBAL_H}" ]]; then

    echo "ERROR: version file not found:"
    echo "  ${GLOBAL_H}"

    exit 1
fi


LINE="$(
    grep -E \
        "^[[:space:]]*#[[:space:]]*define[[:space:]]+${VERSION_DEFINE}([[:space:]]+|$)" \
        "${GLOBAL_H}" |
    head -n 1 ||
    true
)"


if [[ -z "${LINE}" ]]; then

    echo "ERROR:"
    echo "  ${VERSION_DEFINE}"
    echo "not found in:"
    echo "  ${GLOBAL_H}"

    exit 1
fi

VALUE="$(
    python3 - "${GLOBAL_H}" "${VERSION_DEFINE}" <<'PY'
import re
import sys

file_path = sys.argv[1]
macro_name = sys.argv[2]

pattern = re.compile(
    r'^\s*#\s*define\s+'
    + re.escape(macro_name)
    + r'\s+(.+?)\s*$'
)

with open(file_path, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        match = pattern.match(line)

        if not match:
            continue

        value = match.group(1)

        # Remove trailing // comment
        value = re.sub(r'\s*//.*$', '', value)

        # Remove trailing /* ... */ comment
        value = re.sub(r'\s*/\*.*?\*/\s*$', '', value)

        value = value.strip()

        # Remove surrounding quotes
        if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
            value = value[1:-1]

        print(value)
        sys.exit(0)

sys.exit(1)
PY
)" || {
    echo "ERROR:"
    echo "  ${VERSION_DEFINE}"
    echo "not found in:"
    echo "  ${GLOBAL_H}"
    exit 1
}


if [[ "${VALUE}" == \"*\" &&
      "${VALUE}" == *\" ]]; then

    VALUE="${VALUE:1:${#VALUE}-2}"
fi


if [[ -z "${VALUE}" ]]; then

    echo "ERROR:"
    echo "  ${VERSION_DEFINE}"
    echo "has an empty value."

    exit 1
fi


case "${MODE}" in

    dotted|raw)

        printf '%s\n' "${VALUE}"
        ;;

    *)

        echo "ERROR: unsupported mode:"
        echo "  ${MODE}"

        exit 1
        ;;

esac