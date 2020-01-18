cp -v manifest_chrome.json MagicCardSpotter/manifest.json
VERSION='cat VERSION.txt'
sed -iv 's/VERSION/${VERSION}/g' MagicCardSpotter/manifest.json

zip MagicCardSpotter/* MagicCardSpotter${VERSION}.zip