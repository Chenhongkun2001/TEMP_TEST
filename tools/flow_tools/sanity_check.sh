#!/usr/bin/env bash

set -euo pipefail


TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

REPO_ROOT="$(cd "${TOOLS_DIR}/../.." && pwd)"


# shellcheck source=/dev/null
source "${TOOLS_DIR}/release.conf"


cd "${REPO_ROOT}"


# ------------------------------------------------------------
# Git repository
# ------------------------------------------------------------

git rev-parse \
    --is-inside-work-tree \
    >/dev/null 2>&1 ||
{
    echo "ERROR: not inside Git repository."
    exit 1
}


# ------------------------------------------------------------
# Version file
# ------------------------------------------------------------

if [[ ! -f "${GLOBAL_H}" ]]; then

    echo "ERROR: missing:"
    echo "  ${GLOBAL_H}"

    exit 1
fi


"${TOOLS_DIR}/get_version.sh" dotted >/dev/null


# ------------------------------------------------------------
# Git whitespace/conflict checks
# ------------------------------------------------------------

git diff --check

git diff --cached --check


# ------------------------------------------------------------
# Merge / rebase
# ------------------------------------------------------------

GIT_DIR="$(git rev-parse --git-dir)"


if [[ -f "${GIT_DIR}/MERGE_HEAD" ]]; then

    echo "ERROR: merge is currently in progress."

    exit 1
fi


if [[ -d "${GIT_DIR}/rebase-merge" ]] ||
   [[ -d "${GIT_DIR}/rebase-apply" ]]; then

    echo "ERROR: rebase is currently in progress."

    exit 1
fi


echo "[INFO] Local sanity check PASS."