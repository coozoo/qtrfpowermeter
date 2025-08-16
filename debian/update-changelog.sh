#!/usr/bin/bash

# 1. Extract version from main.cpp
VERSION=$(curl --silent 'https://raw.githubusercontent.com/coozoo/qtjsondiff/master/main.cpp' | grep 'QString APP_VERSION' | tr -d ' ' | grep -oP '(?<=constQStringAPP_VERSION=").*(?=\";)')

# 2. Get commit info
COMMIT_HASH=$(git rev-parse --short HEAD)
COMMIT_DATE=$(git log -1 --format=%cd --date=short)
AUTHOR=$(git log -1 --format=%an)

# 3. Format changelog entry
CHANGELOG_ENTRY="qtrfpowermeter (${VERSION}) unstable; urgency=low

  * Automated update: version from source, commit ${COMMIT_HASH} (${COMMIT_DATE}), author ${AUTHOR}

 -- ${AUTHOR}  ${COMMIT_DATE}
"

# 4. Prepend to debian/changelog
if [ -f debian/changelog ]; then
    cp debian/changelog debian/changelog.bak
fi

echo "$CHANGELOG_ENTRY" > debian/changelog.tmp
cat debian/changelog >> debian/changelog.tmp 2>/dev/null
mv debian/changelog.tmp debian/changelog
