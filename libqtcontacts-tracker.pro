CONFIG += ordered
TEMPLATE = subdirs

SUBDIRS = src tests benchmarks tools

equals(ENABLE_DOCS,yes) {
  SUBDIRS += doc
}

OTHER_FILES += configure FUTURE HACKING EXPECTFAIL
OTHER_FILES += debian/changelog debian/control debian/rules
OTHER_FILES += meego/qtcontacts-tracker.spec
OTHER_FILES += libqtcontacts-tracker.supp

# =================================================================================================
# Generate HTML reports from a previous gcov run
# =================================================================================================

coverage-html.commands = $$PWD/tests/gcov-summary.sh lcov docs/coverage

QMAKE_EXTRA_TARGETS += coverage-html
QMAKE_CLEAN += gcov.analysis gcov.analysis.summary

# =================================================================================================
# Remove output of configure script on "make distclean"
# =================================================================================================

confclean.commands = $(DEL_FILE) $$TOP_BUILDDIR/.qmake.cache
distclean.depends = confclean

QMAKE_EXTRA_TARGETS += confclean distclean

# =================================================================================================
# Run configure script when building the project from tools like QtCreator
# =================================================================================================

isEmpty(CONFIGURED):system('$$PWD/configure')

confstamp.target = configure-stamp .qmake.cache
confstamp.commands = $$PWD/configure $$CONFIGURE_FLAGS
confstamp.depends = $$PWD/configure

QMAKE_EXTRA_TARGETS += confstamp

# =================================================================================================
# Install backup configuration file
# =================================================================================================

backupconf.path=$$PREFIX/share/backup-framework/applications/
backupconf.files=libqtcontacts-tracker.conf
INSTALLS += backupconf
