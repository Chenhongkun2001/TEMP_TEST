#!/usr/bin/env bash

set -euo pipefail


# ============================================================
# Paths
# ============================================================

TOOLS_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")" &&
    pwd
)"

REPO_ROOT="$(
    cd "${TOOLS_DIR}/../.." &&
    pwd
)"

CONFIG_FILE="${TOOLS_DIR}/release.conf"


if [[ ! -f "${CONFIG_FILE}" ]]; then

    echo "[ERROR] Missing:"
    echo "  ${CONFIG_FILE}"

    exit 1
fi


# shellcheck source=/dev/null
source "${CONFIG_FILE}"

SECRETS_FILE="${TOOLS_DIR}/yunxiao_token.conf"

if [[ -f "${SECRETS_FILE}" ]]; then

    # shellcheck source=/dev/null
    source "${SECRETS_FILE}"

fi

cd "${REPO_ROOT}"


# ============================================================
# Defaults
# ============================================================

AUTO_CONFIRM="0"

COMMIT_MESSAGE=""

SANITY_ONLY="0"

declare -a SELECTED_BUILD_IDS=()


# ============================================================
# Helpers
# ============================================================

log()
{
    printf '[INFO] %s\n' "$*"
}


die()
{
    printf '[ERROR] %s\n' "$*" >&2
    exit 1
}


section()
{
    echo
    echo "============================================================"
    echo "$*"
    echo "============================================================"
}


set_commit_metadata()
{
    FULL_COMMIT="$(git rev-parse HEAD)"

    SHORT_COMMIT="$(git rev-parse --short=8 HEAD)"

    RELEASE_DATETIME="$(date '+%Y%m%d_%H%M')"
}


# ============================================================
# Usage
# ============================================================

usage()
{
    cat <<'EOF'

Usage:

  ./tools/flow_tools/release.sh \
      --build GW_ALL_NEW_24_PRODUCTS \
      --message "[bugfix] xxx"


Multiple Builds:

  ./tools/flow_tools/release.sh \
      --build GW_ALL_NEW_24_PRODUCTS \
      --build GW_ALLACC_NEW_24_PRODUCTS \
      --message "[bugfix] xxx"


Or use SELECTED_BUILDS:

  SELECTED_BUILDS='["GW_ALL_NEW_24_PRODUCTS"]' \
      ./tools/flow_tools/release.sh \
      --message "[bugfix] xxx"


Options:

  --build ID
      Select one Flow Build.
      Can be specified multiple times.

  --message TEXT
      Git commit message.

  --yes
      Skip interactive confirmations.
      Do NOT normally use this with Copilot.

  -h, --help
      Show this help.

EOF
}


# ============================================================
# Parse arguments
# ============================================================

while [[ $# -gt 0 ]]; do

    case "$1" in

        --build)

            [[ $# -ge 2 ]] ||
                die "--build requires a value."

            SELECTED_BUILD_IDS+=("$2")

            shift 2
            ;;


        --message)

            [[ $# -ge 2 ]] ||
                die "--message requires a value."

            COMMIT_MESSAGE="$2"

            shift 2
            ;;


        --sanity-only)

            SANITY_ONLY="1"

            shift
            ;;

        
        --yes)

            AUTO_CONFIRM="1"

            shift
            ;;


        -h|--help)

            usage
            exit 0
            ;;


        *)

            die "Unknown argument: $1"
            ;;

    esac
done


# ============================================================
# Required programs
# ============================================================

for TOOL in \
    git \
    curl \
    python3 \
    tar \
    find \
    sha256sum \
    awk

do

    command -v "${TOOL}" >/dev/null 2>&1 ||
        die "Required command not found: ${TOOL}"

done


# ============================================================
# Allow SELECTED_BUILDS environment variable
#
# Example:
#
# SELECTED_BUILDS='[
#   "GW_ALL_NEW_24_PRODUCTS"
# ]'
#
# If --build is supplied, --build takes precedence.
# ============================================================

if [[ "${#SELECTED_BUILD_IDS[@]}" -eq 0 ]] &&
   [[ -n "${SELECTED_BUILDS:-}" ]]; then

    PARSED_SELECTED_BUILDS="$(
        python3 - "${SELECTED_BUILDS}" <<'PY'

import json
import sys


try:

    value = json.loads(sys.argv[1])

except Exception as exc:

    print(
        f"Invalid SELECTED_BUILDS JSON: {exc}",
        file=sys.stderr
    )

    sys.exit(2)


if not isinstance(value, list) or not value:

    print(
        "SELECTED_BUILDS must be a non-empty JSON array.",
        file=sys.stderr
    )

    sys.exit(2)


for item in value:

    if not isinstance(item, str) or not item:

        print(
            "Every SELECTED_BUILDS item must be a non-empty string.",
            file=sys.stderr
        )

        sys.exit(2)

    print(item)

PY
    )" ||
        die "Cannot parse SELECTED_BUILDS."


    mapfile -t SELECTED_BUILD_IDS \
        <<< "${PARSED_SELECTED_BUILDS}"

fi

if [[ "${SANITY_ONLY}" == "1" ]]; then

    if [[ "${#SELECTED_BUILD_IDS[@]}" -ne 0 ]]; then

        die "--sanity-only cannot be used together with --build or SELECTED_BUILDS."

    fi 

else 

    if [[ "${#SELECTED_BUILD_IDS[@]}" -eq 0 ]]; then

        die "No Build selected. Use --build or --sanity-only."

    fi

fi


# ============================================================
# Remove duplicate Build IDs
# ============================================================

mapfile -t SELECTED_BUILD_IDS < <(

    printf '%s\n' "${SELECTED_BUILD_IDS[@]}" |
    awk 'NF && !seen[$0]++'

)


# ============================================================
# Validate Build IDs
# ============================================================

is_allowed_build()
{
    local CANDIDATE="$1"
    local ALLOWED


    for ALLOWED in "${FLOW_BUILD_IDS[@]}"; do

        if [[ "${CANDIDATE}" == "${ALLOWED}" ]]; then

            return 0

        fi

    done


    return 1
}


for BUILD_ID in "${SELECTED_BUILD_IDS[@]}"; do

    is_allowed_build "${BUILD_ID}" ||
        die "Unsupported Build ID: ${BUILD_ID}"


    [[ -n "${FLOW_JOB_NAME_BY_BUILD[$BUILD_ID]:-}" ]] ||
        die "Missing FLOW_JOB_NAME_BY_BUILD mapping for ${BUILD_ID}"

done


# ============================================================
# Validate Flow configuration
# ============================================================

[[ -n "${YUNXIAO_TOKEN:-}" ]] ||
    die "YUNXIAO_TOKEN is not set."


[[ -n "${FLOW_API_BASE:-}" ]] ||
    die "FLOW_API_BASE is empty."


[[ -n "${FLOW_ORGANIZATION_ID:-}" ]] ||
    die "FLOW_ORGANIZATION_ID is empty."


[[ "${FLOW_ORGANIZATION_ID}" != YOUR_* ]] ||
    die "FLOW_ORGANIZATION_ID is not configured."


[[ -n "${FLOW_PIPELINE_ID:-}" ]] ||
    die "FLOW_PIPELINE_ID is empty."


[[ "${FLOW_PIPELINE_ID}" != YOUR_* ]] ||
    die "FLOW_PIPELINE_ID is not configured."


[[ -n "${DOWNLOAD_ROOT:-}" ]] ||
    die "DOWNLOAD_ROOT is empty."


# ============================================================
# Validate Git repository
# ============================================================

git rev-parse \
    --is-inside-work-tree \
    >/dev/null 2>&1 ||
    die "Not inside a Git repository."


CURRENT_BRANCH="$(git branch --show-current)"


if [[ -z "${CURRENT_BRANCH}" ]]; then

    die "Detached HEAD is not supported."

fi


# ============================================================
# Branch guard
#
# IMPORTANT:
# Current YAML uses static branch-specific sources.
# Therefore this release workflow must build the same branch
# that we push here.
# ============================================================

if [[ -n "${RELEASE_BRANCH:-}" ]] &&
   [[ "${CURRENT_BRANCH}" != "${RELEASE_BRANCH}" ]]; then

    die \
        "Current branch '${CURRENT_BRANCH}' does not match RELEASE_BRANCH '${RELEASE_BRANCH}'."

fi


# ============================================================
# Merge / rebase guard
# ============================================================

GIT_DIR="$(git rev-parse --git-dir)"


if [[ -f "${GIT_DIR}/MERGE_HEAD" ]]; then

    die "Merge is in progress."

fi


if [[ -d "${GIT_DIR}/rebase-merge" ]] ||
   [[ -d "${GIT_DIR}/rebase-apply" ]]; then

    die "Rebase is in progress."

fi


# ============================================================
# Release information
# ============================================================

section "GW Release"


VERSION="$(
    "${TOOLS_DIR}/get_version.sh" dotted
)"


[[ -n "${VERSION}" ]] ||
    die "Cannot read application version."


log "Repository: ${REPO_ROOT}"

log "Branch: ${CURRENT_BRANCH}"

log "Version: ${VERSION}"


echo
echo "Selected Builds:"

printf '  - %s\n' \
    "${SELECTED_BUILD_IDS[@]}"


# ============================================================
# Remote check
# ============================================================

section "Git Remote Check"


git fetch "${GIT_REMOTE}" --prune


REMOTE_REF="refs/remotes/${GIT_REMOTE}/${CURRENT_BRANCH}"


if git show-ref \
    --verify \
    --quiet \
    "${REMOTE_REF}"
then

    git merge-base \
        --is-ancestor \
        "${GIT_REMOTE}/${CURRENT_BRANCH}" \
        HEAD ||
        die \
            "Remote branch has commits not in local HEAD. Run: git pull --rebase ${GIT_REMOTE} ${CURRENT_BRANCH}"

fi


# ============================================================
# Working tree / release mode
# ============================================================

section "Working Tree"


git status --short


HAS_LOCAL_CHANGES="1"

if [[ -z "$(git status --porcelain)" ]]; then

    HAS_LOCAL_CHANGES="0"

fi


SANITY_STATE_FILE="$(
    git rev-parse --git-path flow_tools_last_sanity_ok_commit
)"

SKIP_FLOW_SANITY="0"


if [[ "${HAS_LOCAL_CHANGES}" == "0" ]]; then

    set_commit_metadata


    if git show-ref \
        --verify \
        --quiet \
        "${REMOTE_REF}"
    then

        REMOTE_COMMIT="$(
            git rev-parse "${GIT_REMOTE}/${CURRENT_BRANCH}"
        )"


        if [[ "${FULL_COMMIT}" != "${REMOTE_COMMIT}" ]]; then

            die \
                "Working tree is clean, but local HEAD is not the same as ${GIT_REMOTE}/${CURRENT_BRANCH}. Push/sync it before direct Build."

        fi

    fi


    section "Clean Working Tree Mode"

    log "No local code changes."
    log "Skip local sanity, local build, Git add, commit and push."
    log "Use existing commit: ${FULL_COMMIT}"


    if [[ "${SANITY_ONLY}" != "1" ]] &&
       [[ -f "${SANITY_STATE_FILE}" ]] &&
       [[ "$(<"${SANITY_STATE_FILE}")" == "${FULL_COMMIT}" ]]; then

        SKIP_FLOW_SANITY="1"

        log "This commit already has a recorded Flow sanity PASS."
        log "Request Flow to skip sanity and run only the selected Build(s)."

    else

        log "Flow sanity will run for this commit."

    fi

else

    # ========================================================
    # Local sanity
    # ========================================================

    section "Local Sanity Check"


    if [[ ! -x "${TOOLS_DIR}/sanity_check.sh" ]]; then

        die "${TOOLS_DIR}/sanity_check.sh is not executable."

    fi


    "${TOOLS_DIR}/sanity_check.sh"


    # ========================================================
    # Local build
    # ========================================================

    if [[ "${LOCAL_BUILD_ENABLED:-1}" == "1" ]]; then

        section "Local Build"


        log "${LOCAL_BUILD_COMMAND}"


        bash -lc \
            "${LOCAL_BUILD_COMMAND}"


        log "Local build PASS."

    fi


    # ========================================================
    # Diff
    # ========================================================

    section "Git Diff"


    git diff --stat

    echo

    git diff


    # ========================================================
    # Confirmation 1
    # ========================================================

    if [[ "${AUTO_CONFIRM}" != "1" ]]; then

        echo

        read -r -p \
            "Continue and stage ALL current changes? [y/N] " \
            ANSWER


        case "${ANSWER}" in

            y|Y|yes|YES)

                ;;

            *)

                die "Release cancelled."
                ;;

        esac

    fi


    # ========================================================
    # Stage
    # ========================================================

    section "Git Stage"


    git add -A


    if git diff --cached --quiet; then

        die "Nothing staged."

    fi


    git diff --cached --stat

    echo

    git diff --cached


    # ========================================================
    # Commit message
    # ========================================================

    if [[ -z "${COMMIT_MESSAGE}" ]]; then

        COMMIT_MESSAGE="[release] gateway v${VERSION}"

    fi


    section "Release Summary"


    echo "Branch:"
    echo "  ${CURRENT_BRANCH}"

    echo

    echo "Version:"
    echo "  ${VERSION}"

    echo

    echo "Commit message:"
    echo "  ${COMMIT_MESSAGE}"

    echo

    echo "Builds:"

    printf '  - %s\n' \
        "${SELECTED_BUILD_IDS[@]}"


    # ========================================================
    # Confirmation 2
    # ========================================================

    if [[ "${AUTO_CONFIRM}" != "1" ]]; then

        echo

        read -r -p \
            "Commit, push and start Alibaba Flow? [y/N] " \
            ANSWER


        case "${ANSWER}" in

            y|Y|yes|YES)

                ;;

            *)

                die "Release cancelled before commit."
                ;;

        esac

    fi


    # ========================================================
    # Commit
    # ========================================================

    section "Git Commit"


    git commit \
        -m "${COMMIT_MESSAGE}"


    set_commit_metadata


    log "Commit: ${FULL_COMMIT}"


    # ========================================================
    # Push
    # ========================================================

    section "Git Push"


    git push \
        -u \
        "${GIT_REMOTE}" \
        "${CURRENT_BRANCH}"


    log "Push PASS."

fi


# ============================================================
# Convert Build IDs into JSON
# ============================================================

SELECTED_BUILDS_JSON="$(
    python3 \
        - "${SELECTED_BUILD_IDS[@]}" <<'PY'

import json
import sys


print(
    json.dumps(
        sys.argv[1:],
        separators=(",", ":")
    )
)

PY
)"


log "SELECTED_BUILDS=${SELECTED_BUILDS_JSON}"


# ============================================================
# Flow API URL
# ============================================================

FLOW_RUN_URL="${FLOW_API_BASE%/}/oapi/v1/flow/organizations/${FLOW_ORGANIZATION_ID}/pipelines/${FLOW_PIPELINE_ID}/runs"


# ============================================================
# Start Flow
#
# IMPORTANT:
#
# No runningBranchs here.
#
# Current YAML has multiple branch-specific sources pointing to
# the same IndustrialGW/GW repository.
#
# Only runtime envs are sent.
# ============================================================

start_flow()
{
    local PARAMS
    local BODY

    local RESPONSE_FILE
    local HTTP_CODE
    local RESPONSE
    local RUN_ID


    PARAMS="$(
        python3 \
            - "${SELECTED_BUILDS_JSON}" \
            "${CURRENT_BRANCH}" \
            "${FULL_COMMIT}" \
            "${VERSION}" \
            "${SKIP_FLOW_SANITY}" <<'PY'

import json
import sys


selected_builds = sys.argv[1]

branch = sys.argv[2]

commit = sys.argv[3]

version = sys.argv[4]

skip_sanity = sys.argv[5]


params = {

    "envs": {

        "SELECTED_BUILDS": selected_builds,

        "RELEASE_BRANCH": branch,

        "RELEASE_COMMIT": commit,

        "RELEASE_VERSION": version,

        "SKIP_SANITY": skip_sanity
    },

    "comment":
        "GW release "
        + branch
        + " "
        + commit[:8]
        + " v"
        + version
}


print(
    json.dumps(
        params,
        separators=(",", ":")
    )
)

PY
    )"


    # CreatePipelineRun requires:
    #
    # {
    #   "params": "<JSON STRING>"
    # }

    BODY="$(
        python3 \
            - "${PARAMS}" <<'PY'

import json
import sys


print(
    json.dumps(
        {
            "params": sys.argv[1]
        },
        separators=(",", ":")
    )
)

PY
    )"


    RESPONSE_FILE="$(
        mktemp
    )"


    if ! HTTP_CODE="$(
        curl \
            --silent \
            --show-error \
            --retry 7 \
            --retry-all-errors \
            --retry-delay 3 \
            --connect-timeout 15 \
            --max-time 60 \
            --output "${RESPONSE_FILE}" \
            --write-out '%{http_code}' \
            --request POST \
            --header "Content-Type: application/json" \
            --header "x-yunxiao-token: ${YUNXIAO_TOKEN}" \
            --data "${BODY}" \
            "${FLOW_RUN_URL}"
            )"
    then

        rm -f "${RESPONSE_FILE}"

        die "Network error while starting Alibaba Flow."

    fi


    RESPONSE="$(
        cat "${RESPONSE_FILE}"
    )"


    rm -f "${RESPONSE_FILE}"


    case "${HTTP_CODE}" in

        200|201|202)

            ;;

        *)

            die \
                "CreatePipelineRun failed: HTTP ${HTTP_CODE}; response: ${RESPONSE}"
            ;;

    esac


    RUN_ID="$(
        python3 \
            - "${RESPONSE}" <<'PY'

import json
import sys


raw = sys.argv[1].strip()


try:

    value = json.loads(raw)

except Exception:

    value = raw


if isinstance(value, int):

    print(value)


elif isinstance(value, str) and value.isdigit():

    print(value)


elif isinstance(value, dict):

    candidate = (
        value.get("pipelineRunId")
        or value.get("id")
    )

    if candidate is not None:

        print(candidate)

PY
    )"


    if [[ ! "${RUN_ID}" =~ ^[0-9]+$ ]]; then

        die \
            "Cannot parse pipelineRunId from response: ${RESPONSE}"

    fi


    printf '%s\n' "${RUN_ID}"
}


# ============================================================
# Get Flow Run
# ============================================================

get_flow_run()
{
    local RUN_ID="$1"

    local URL
    local RESPONSE_FILE
    local HTTP_CODE
    local RESPONSE


    URL="${FLOW_API_BASE%/}/oapi/v1/flow/organizations/${FLOW_ORGANIZATION_ID}/pipelines/${FLOW_PIPELINE_ID}/runs/${RUN_ID}"


    RESPONSE_FILE="$(
        mktemp
    )"


    if ! HTTP_CODE="$(
        curl \
            --silent \
            --show-error \
            --http1.1 \
            --tlsv1.2 \
            --tls-max 1.2 \
            --retry 7 \
            --retry-all-errors \
            --retry-delay 2 \
            --connect-timeout 15 \
            --max-time 60 \
            --output "${RESPONSE_FILE}" \
            --write-out '%{http_code}' \
            --header "Content-Type: application/json" \
            --header "x-yunxiao-token: ${YUNXIAO_TOKEN}" \
            "${URL}"
    )"
    then

        rm -f "${RESPONSE_FILE}"

        return 1

    fi


    RESPONSE="$(
        cat "${RESPONSE_FILE}"
    )"


    rm -f "${RESPONSE_FILE}"


    if [[ "${HTTP_CODE}" != "200" ]]; then

        echo \
            "GetPipelineRun HTTP ${HTTP_CODE}: ${RESPONSE}" \
            >&2

        return 1

    fi


    printf '%s' "${RESPONSE}"
}


# ============================================================
# Wait for Flow
# ============================================================

wait_for_flow()
{
    local RUN_ID="$1"

    local STARTED
    local NOW
    local ELAPSED

    local RESPONSE
    local STATUS


    STARTED="$(
        date +%s
    )"


    while true; do

        RESPONSE="$(
            get_flow_run "${RUN_ID}"
        )" ||
            die \
                "GetPipelineRun failed for run ${RUN_ID}."


        STATUS="$(
            printf '%s' "${RESPONSE}" |
            python3 -c '
import json
import sys

print(
    json.load(sys.stdin).get(
        "status",
        "UNKNOWN"
    )
)
'
        )"


        log "Flow #${RUN_ID}: ${STATUS}"


        case "${STATUS}" in

            SUCCESS)

                return 0
                ;;


            FAIL|FAILED|ERROR|CANCELED|CANCELLED|STOPPED)

                return 1
                ;;

        esac


        NOW="$(
            date +%s
        )"


        ELAPSED=$((NOW - STARTED))


        if (( ELAPSED >= FLOW_TIMEOUT_SECONDS )); then

            die \
                "Timed out waiting for Flow #${RUN_ID}."

        fi


        sleep "${FLOW_POLL_INTERVAL}"

    done
}


# ============================================================
# Verify selected Job succeeded
# ============================================================

assert_job_success()
{
    local RUN_ID="$1"

    local JOB_NAME="$2"

    local RESPONSE
    local STATUS


    RESPONSE="$(
        get_flow_run "${RUN_ID}"
    )" ||
        die \
            "GetPipelineRun failed for run ${RUN_ID}."


    STATUS="$(
        printf '%s' "${RESPONSE}" |
        python3 \
            -c '
import json
import sys


wanted = sys.argv[1]

data = json.load(sys.stdin)

values = []


for stage in data.get("stages", []):

    info = stage.get("stageInfo") or {}


    for job in info.get("jobs", []):

        if job.get("name") == wanted:

            values.append(
                job.get(
                    "status",
                    "UNKNOWN"
                )
            )


print(
    values[-1]
    if values
    else "NOT_FOUND"
)
' "${JOB_NAME}"
    )"


    if [[ "${STATUS}" != "SUCCESS" ]]; then

        die \
            "Selected Flow job '${JOB_NAME}' status is ${STATUS}."

    fi
}


# ============================================================
# Find the Artifact belonging to ONE specific Build Job
#
# GetPipelineRun officially exposes Job actions.
#
# We only accept:
#
# action.type == GetPipelineArtifactUrl
#
# for the exact selected Job.
# ============================================================

get_job_artifact_record()
{
    local RUN_ID="$1"

    local JOB_NAME="$2"

    local RESPONSE


    RESPONSE="$(
        get_flow_run "${RUN_ID}"
    )" ||
        return 1


    printf '%s' "${RESPONSE}" |
    python3 \
        -c '
import json
import sys


wanted = sys.argv[1]

data = json.load(sys.stdin)

records = []


for stage in data.get("stages", []):

    info = stage.get("stageInfo") or {}


    for job in info.get("jobs", []):

        if job.get("name") != wanted:

            continue


        for action in job.get("actions", []):

            if action.get("type") != "GetPipelineArtifactUrl":

                continue


            params = action.get("params") or {}


            if isinstance(params, str):

                try:

                    params = json.loads(params)

                except Exception:

                    continue


            for item in params.get("files", []):

                file_name = item.get("fileName")

                file_path = item.get("filePath")


                if file_name and file_path:

                    records.append(
                        (
                            file_name,
                            file_path
                        )
                    )


if len(records) != 1:

    print(
        "Expected exactly one pipeline artifact "
        f"for job {wanted!r}, "
        f"found {len(records)}",
        file=sys.stderr
    )

    sys.exit(2)


print(
    records[0][0]
    + "\t"
    + records[0][1]
)
' "${JOB_NAME}"
}


# ============================================================
# Get temporary Artifact download URL
# ============================================================

get_artifact_download_url()
{
    local FILE_NAME="$1"

    local FILE_PATH="$2"

    local ENDPOINT

    local RESPONSE_FILE
    local HTTP_CODE
    local RESPONSE
    local URL


    ENDPOINT="${FLOW_API_BASE%/}/oapi/v1/flow/organizations/${FLOW_ORGANIZATION_ID}/pipelines/getArtifactDownloadUrl"


    RESPONSE_FILE="$(
        mktemp
    )"


    if ! HTTP_CODE="$(
        curl \
            --silent \
            --show-error \
            --output "${RESPONSE_FILE}" \
            --write-out '%{http_code}' \
            --get \
            --header "Content-Type: application/json" \
            --header "x-yunxiao-token: ${YUNXIAO_TOKEN}" \
            --data-urlencode "filePath=${FILE_PATH}" \
            --data-urlencode "fileName=${FILE_NAME}" \
            "${ENDPOINT}"
    )"
    then

        rm -f "${RESPONSE_FILE}"

        return 1

    fi


    RESPONSE="$(
        cat "${RESPONSE_FILE}"
    )"


    rm -f "${RESPONSE_FILE}"


    if [[ "${HTTP_CODE}" != "200" ]]; then

        echo \
            "GetPipelineArtifactUrl HTTP ${HTTP_CODE}: ${RESPONSE}" \
            >&2

        return 1

    fi


    URL="$(
        python3 \
            - "${RESPONSE}" <<'PY'

import json
import sys


raw = sys.argv[1].strip()


try:

    value = json.loads(raw)

except Exception:

    value = raw


if isinstance(value, str):

    print(value)

PY
    )"


    if [[ "${URL}" != http://* ]] &&
       [[ "${URL}" != https://* ]]; then

        return 1

    fi


    printf '%s\n' "${URL}"
}


# ============================================================
# Render final filename
# ============================================================

render_name()
{
    local TEMPLATE="$1"

    local BUILD_ID="$2"

    local RESULT
    local SAFE_BRANCH


    SAFE_BRANCH="$(
        printf '%s' "${CURRENT_BRANCH}" |
        tr '/ :' '___' |
        tr -cd 'A-Za-z0-9._-'
    )"


    RESULT="${TEMPLATE}"


    RESULT="${RESULT//\{VERSION\}/${VERSION}}"
    
    RESULT="${RESULT//\{DATETIME\}/${RELEASE_DATETIME}}"

    RESULT="${RESULT//\{SHORT_COMMIT\}/${SHORT_COMMIT}}"

    RESULT="${RESULT//\{BUILD_ID\}/${BUILD_ID}}"

    RESULT="${RESULT//\{BRANCH\}/${SAFE_BRANCH}}"


    printf '%s\n' "${RESULT}"
}


# ============================================================
# Find exactly one file
# ============================================================

find_unique_file()
{
    local ROOT="$1"

    local BASENAME="$2"

    local -a MATCHES=()


    mapfile -t MATCHES < <(

        find \
            "${ROOT}" \
            -type f \
            -name "${BASENAME}" \
            -print

    )


    if [[ "${#MATCHES[@]}" -ne 1 ]]; then

        die \
            "Expected exactly one '${BASENAME}' in artifact, found ${#MATCHES[@]}."

    fi


    printf '%s\n' \
        "${MATCHES[0]}"
}


# ============================================================
# Download Artifact for one selected Build
# ============================================================

download_build_artifact()
{
    local RUN_ID="$1"

    local BUILD_ID="$2"

    local JOB_NAME

    local RECORD

    local FILE_NAME
    local FILE_PATH

    local DOWNLOAD_URL

    local WORK_DIR
    local BUNDLE
    local EXTRACT_DIR
    local OUTPUT_DIR

    local GATEWAY_SRC
    local PACKAGE_SRC
    local DUMMY_SRC

    local GATEWAY_NAME
    local PACKAGE_NAME
    local DUMMY_NAME


    JOB_NAME="${FLOW_JOB_NAME_BY_BUILD[$BUILD_ID]}"


    assert_job_success \
        "${RUN_ID}" \
        "${JOB_NAME}"


    RECORD="$(
        get_job_artifact_record \
            "${RUN_ID}" \
            "${JOB_NAME}"
    )" ||
        die \
            "Cannot locate flowPublic artifact for job '${JOB_NAME}'. Check ArtifactUpload and FLOW_JOB_NAME_BY_BUILD."


    IFS=$'\t' \
        read -r \
        FILE_NAME \
        FILE_PATH \
        <<< "${RECORD}"


    DOWNLOAD_URL="$(
        get_artifact_download_url \
            "${FILE_NAME}" \
            "${FILE_PATH}"
    )" ||
        die \
            "Cannot obtain temporary artifact download URL for '${JOB_NAME}'."


    WORK_DIR="$(
        mktemp -d
    )"


    BUNDLE="${WORK_DIR}/${FILE_NAME}"

    EXTRACT_DIR="${WORK_DIR}/extract"


    mkdir -p "${EXTRACT_DIR}"


    log \
        "Downloading Flow artifact for ${BUILD_ID}: ${FILE_NAME}"


    if ! curl \
        --fail \
        --silent \
        --show-error \
        --location \
        "${DOWNLOAD_URL}" \
        --output "${BUNDLE}"
    then

        rm -rf "${WORK_DIR}"

        die \
            "Artifact download failed for ${BUILD_ID}."

    fi


    if [[ ! -s "${BUNDLE}" ]]; then

        rm -rf "${WORK_DIR}"

        die "Downloaded artifact is empty."

    fi


    if ! tar \
        -tzf "${BUNDLE}" \
        >/dev/null
    then

        rm -rf "${WORK_DIR}"

        die \
            "Downloaded artifact is not a valid .tgz archive."

    fi


    tar \
        -xzf "${BUNDLE}" \
        -C "${EXTRACT_DIR}"


    # --------------------------------------------------------
    # Locate original build outputs
    # --------------------------------------------------------

    GATEWAY_SRC="$(
        find_unique_file \
            "${EXTRACT_DIR}" \
            "gatewayApp.tar.gz"
    )"


    PACKAGE_SRC="$(
        find_unique_file \
            "${EXTRACT_DIR}" \
            "skf_package.tar.gz"
    )"


    DUMMY_SRC="$(
        find_unique_file \
            "${EXTRACT_DIR}" \
            "gatewayAppDummy.tar.gz"
    )"


    # --------------------------------------------------------
    # Final output directory
    # --------------------------------------------------------

    OUTPUT_DIR="${DOWNLOAD_ROOT}/${VERSION}_${SHORT_COMMIT}/${RELEASE_DATETIME}_run${RUN_ID}/${BUILD_ID}"


    mkdir -p "${OUTPUT_DIR}"


    # --------------------------------------------------------
    # Final filenames
    # --------------------------------------------------------

    GATEWAY_NAME="$(
        render_name \
            "${GATEWAY_OUTPUT_TEMPLATE}" \
            "${BUILD_ID}"
    )"


    PACKAGE_NAME="$(
        render_name \
            "${PACKAGE_OUTPUT_TEMPLATE}" \
            "${BUILD_ID}"
    )"


    DUMMY_NAME="$(
        render_name \
            "${DUMMY_OUTPUT_TEMPLATE}" \
            "${BUILD_ID}"
    )"


    # --------------------------------------------------------
    # Copy
    # --------------------------------------------------------

    cp \
        "${GATEWAY_SRC}" \
        "${OUTPUT_DIR}/${GATEWAY_NAME}"


    cp \
        "${PACKAGE_SRC}" \
        "${OUTPUT_DIR}/${PACKAGE_NAME}"


    cp \
        "${DUMMY_SRC}" \
        "${OUTPUT_DIR}/${DUMMY_NAME}"


    # --------------------------------------------------------
    # Checksums
    # --------------------------------------------------------

    (
        cd "${OUTPUT_DIR}"


        sha256sum \
            "${GATEWAY_NAME}" \
            "${PACKAGE_NAME}" \
            "${DUMMY_NAME}" \
            > SHA256SUMS
    )


    # --------------------------------------------------------
    # Clean temporary download
    # --------------------------------------------------------

    rm -rf "${WORK_DIR}"


    # --------------------------------------------------------
    # Result
    # --------------------------------------------------------

    echo

    log \
        "Artifacts saved to ${OUTPUT_DIR}"


    echo

    echo "  ${GATEWAY_NAME}"

    echo "  ${PACKAGE_NAME}"

    echo "  ${DUMMY_NAME}"

    echo "  SHA256SUMS"
}


# ============================================================
# Start Alibaba Flow
# ============================================================

section "Start Alibaba Flow"


log \
    "SELECTED_BUILDS=${SELECTED_BUILDS_JSON}"


PIPELINE_RUN_ID="$(
    start_flow
)"


log \
    "Pipeline Run ID: ${PIPELINE_RUN_ID}"


# ============================================================
# Wait
# ============================================================

section "Wait for Alibaba Flow"


wait_for_flow \
    "${PIPELINE_RUN_ID}" ||
    die \
        "Alibaba Flow #${PIPELINE_RUN_ID} failed."


log \
    "Alibaba Flow SUCCESS."


if [[ "${SKIP_FLOW_SANITY}" != "1" ]]; then

    printf '%s\n' "${FULL_COMMIT}" > "${SANITY_STATE_FILE}"

    log "Recorded Flow sanity PASS for commit ${FULL_COMMIT}."

fi


# ============================================================
# Download selected Build artifacts
# ============================================================

section "Download Flow Artifacts"


if [[ "${SANITY_ONLY}" == "1" ]]; then

    section "Download Flow Artifacts (Sanity Only)"

else

    section "Download Flow Artifacts (Selected Builds)"

    for BUILD_ID in "${SELECTED_BUILD_IDS[@]}"; do

        download_build_artifact \
            "${PIPELINE_RUN_ID}" \
            "${BUILD_ID}"

    done

fi


# ============================================================
# Complete
# ============================================================

section "Release Complete"


echo "Branch:"
echo "  ${CURRENT_BRANCH}"

echo

echo "Commit:"
echo "  ${FULL_COMMIT}"

echo

echo "Version:"
echo "  ${VERSION}"

echo

echo "Pipeline Run:"
echo "  ${PIPELINE_RUN_ID}"

echo

echo "Artifacts root:"
echo "  ${DOWNLOAD_ROOT}/${VERSION}_${SHORT_COMMIT}/${RELEASE_DATETIME}_run${PIPELINE_RUN_ID}"

echo

echo "SUCCESS"