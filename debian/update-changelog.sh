#!/bin/bash

MAINTAINER_NAME="happiness"
MAINTAINER_EMAIL="realcoozoo@gmail.com"

echo "--- Starting changelog generation ---"

BASE_VERSION=$(cat main.cpp | grep 'QString APP_VERSION' | tr -d ' ' | grep -oP '(?<=constQStringAPP_VERSION=").*(?=\";)')
if [ -z "$BASE_VERSION" ]; then
    echo "ERROR: Could not determine base version from main.cpp."
    exit 1
fi
echo "Base version: $BASE_VERSION"

TMP_CHANGELOG=$(mktemp)
FIRST_ENTRY=true

# --- CHANGE 1: Added %D to the format string to get ref names (tags) ---
git log --format="%H%x00%s%n%b%x00%an%x00%cI%x00%D%x00" | \
# --- CHANGE 2: Added REF_NAMES to the read command ---
while IFS= read -r -d '' FULL_HASH && IFS= read -r -d '' MESSAGE && IFS= read -r -d '' AUTHOR_NAME && IFS= read -r -d '' COMMIT_ISO_DATE && IFS= read -r -d '' REF_NAMES; do
    
    FULL_HASH=$(echo "$FULL_HASH" | tr -d '[:space:]')
    SHORT_HASH="${FULL_HASH:0:7}"
    DISTRO=$(lsb_release -sc)
    COMMIT_RFC_DATE=$(date -d "$COMMIT_ISO_DATE" -R)

    FORMATTED_MESSAGE=$(echo "$MESSAGE" | sed -e '/^$/d' -e 's/^/  * /')

    if $FIRST_ENTRY; then
        DEBIAN_VERSION="${BASE_VERSION}-$(date +'%Y%m%d%H%M')"
        FIRST_ENTRY=false
        echo "Processing newest commit ($SHORT_HASH) with date version."
    else
        # --- CHANGE 3: The new logic to prefer tags over hashes ---
        # Look for 'tag: ' in the ref names and extract the tag name.
        TAG=$(echo "$REF_NAMES" | grep -oP 'tag: \K[^,)]+' | head -n 1)

        if [ -n "$TAG" ]; then
            # If a tag was found, use it as the version.
            DEBIAN_VERSION="$TAG"
            echo "Processing older commit ($SHORT_HASH) with TAG version: $TAG"
        else
            # If no tag, fall back to the commit hash.
            DEBIAN_VERSION="$SHORT_HASH"
            echo "Processing older commit ($SHORT_HASH) with HASH version."
        fi
    fi

    SINGLE_ENTRY="qtrfpowermeter (${DEBIAN_VERSION}) ${DISTRO}; urgency=low

${FORMATTED_MESSAGE}

 -- ${AUTHOR_NAME} <${MAINTAINER_EMAIL}>  ${COMMIT_RFC_DATE}
"

    echo -e "${SINGLE_ENTRY}\n" >> "$TMP_CHANGELOG"

done

echo "Writing fresh debian/changelog file..."
mv "$TMP_CHANGELOG" debian/changelog

echo "Changelog successfully generated."
