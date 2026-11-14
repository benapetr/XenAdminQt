Places to update version in:

* src/xenadmin-ui/globals.h
* packaging/config
* debian/changelog
* CMakeLists.txt

For a release, move the `CHANGELOG.md` Unreleased entries into a dated release
section. Version fields above use the numeric version (for example `0.1.0`);
the changelog release label and Git tag include the prerelease suffix (for
example `v0.1.0-beta`). Preserve historical release labels.
