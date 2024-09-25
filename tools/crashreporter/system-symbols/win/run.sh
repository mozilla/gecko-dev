#!/bin/sh

set -e

pwd

mkdir -p "/builds/worker/artifacts"

DUMP_SYMS="${MOZ_FETCHES_DIR}/dump_syms/dump_syms"
SYMBOL_STORE=$(mktemp -d -p "/builds/worker/")
SYMBOL_CACHE=$(mktemp -d -p "/builds/worker/")
SYMBOL_SERVER_URLS="SRV*${SYMBOL_CACHE}*https://msdl.microsoft.com/download/symbols;SRV*${SYMBOL_CACHE}*https://software.intel.com/sites/downloads/symbols;SRV*${SYMBOL_CACHE}*https://download.amd.com/dir/bin;SRV*${SYMBOL_CACHE}*https://driver-symbols.nvidia.com"

# List Windows crashes from the various release channels.
for release_channel in release beta nightly esr; do
    supersearch --num=500 --release_channel="${release_channel}" --platform=Windows | while read line; do
        printf "https://crash-stats.mozilla.org/api/ProcessedCrash/?crash_id=${line}\n" >> crashes.list
    done
done

# Fetch the raw JSON for each crash.
CRASHES_DIR=$(mktemp -d -p "/builds/worker/")
cd "${CRASHES_DIR}"
wget --no-verbose --waitretry=100 --retry-on-http-error=429 --compression=auto -i ../crashes.list
cd ..

# Find the missing modules entries, in the crash modules list, and extract the
# code id and debug filename, we'll store them in a file to process them later.
find "${CRASHES_DIR}" -name "index.html*" -exec jq -r ".json_dump.modules[] | select(.missing_symbols == true)  | .code_id,.filename,.debug_file" {} \; | while read line; do
    if [ -z "${code_id}" ]; then
        code_id="${line}"
    elif [ -z "${filename}" ]; then
        filename="${line}"
    else
        debug_file="${line}"
        printf "${filename},${code_id},${debug_file}\n" >> debuginfo.list

        code_id=""
        filename=""
        debug_file=""
    fi
done

# Dump symbols using the first parameter as the filename and the second
# parameter as the code ID. Exit the script if we encounter an unexpected
# failure when invoking dump_syms. Return the exit code captured from dump_syms
dump_symbols() {
    local filename="${1}"
    local code_id="${2}"

    # dump_syms may fail, but we don't always want to stop the script if it
    # does, so we capture the return value and use the output to decide whether
    # we want to move on or not.
    "${DUMP_SYMS}" "${filename}" --code-id "${code_id}" --inlines --check-cfi \
        --store "${SYMBOL_STORE}" --symbol-server "${SYMBOL_SERVER_URLS}" \
        --verbose error 2>dump_syms.errors && rv=0 || rv=$?

    if [ ${rv} -ne 0 ]; then
      errors=$(cat dump_syms.errors)
      printf "error: ${errors}\n"

      if [ "${errors}" != "Impossible to get file ${filename} with id ${code_id}" ] && \
         [ "${errors}" != "No CFI data" ]; then
          exit 1
      fi
    fi

    return ${rv}
}

# Dump every missing module we found, some will not be available.
sort -u debuginfo.list | while read line; do
    filename=$(echo "${line}" | cut -d',' -f1)
    code_id=$(echo "${line}" | cut -d',' -f2)
    debug_file=$(echo "${line}" | cut -d',' -f3)
    alt_filename=$(echo "${debug_file}" | rev | cut -d'\' -f1 | cut -d'.' -f2- | rev)
    alt_filename="${alt_filename}.dll"

    set +e
    printf "Attempting to dump file ${filename} with id ${code_id}\n"
    dump_symbols "${filename}" "${code_id}"

    if [ $? -ne 0 ] && [ -n "${debug_file}" ] && \
       [ "${filename}" != "${alt_filename}" ]; then
        # Try again but using the debug file name
        printf "Attempting to dump alternative file ${alt_filename} with id ${code_id}\n"
        dump_symbols "${alt_filename}" "${code_id}"
    fi
    set -e
done

# Create the symbols archive only if we actually dumped something.
symbols=$(find "${SYMBOL_STORE}" -type f)

if [ -n "${symbols}" ]; then
    cd "${SYMBOL_STORE}" && \
    7zz a "/builds/worker/artifacts/target.crashreporter-symbols.zip"
fi
