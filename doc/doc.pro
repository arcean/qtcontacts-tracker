TARGET = dummy
TEMPLATE = lib
CONFIG = static

doxygen.name = doxygen
doxygen.input = DOXYFILES
doxygen.output = html/*.{css,html,png,qhp}
doxygen.commands = ( \
      cat ${QMAKE_FILE_IN} ; \
      echo "PROJECT_NUMBER=\"$$VERSION_LABEL"\" ; \
      echo "INPUT=\"$$TOP_SOURCEDIR/doc/mainpage.h $$TOP_SOURCEDIR/src/lib $$TOP_SOURCEDIR/src/engine/engine.cpp\"" \
    ) | doxygen -
doxygen.CONFIG += target_predeps
doxygen.variable_out = docs.files
doxygen.clean = -r html

xpatterns.name = xpatterns
xpatterns.input = XPATTERNS
xpatterns.output = ${QMAKE_FILE_BASE}.html
xpatterns.commands = \
    xmlpatterns ${QMAKE_FILE_IN} \
    -param package=$$quote($$PACKAGE) \
    -param version=$$quote($$VERSION_LABEL) \
    -output $$xpatterns.output || \
    rm $$xpatterns.output
xpatterns.CONFIG += target_predeps
xpatterns.variable_out = docs.files

docs.path = $$PREFIX/share/doc/qtcontacts-tracker/html
docs.CONFIG += no_check_exist

docsearch.files = html/search/*.{css,html,js,png}
docsearch.path = $$PREFIX/share/doc/qtcontacts-tracker/html/search
docsearch.CONFIG += no_check_exist
docsearch.depends = $$docs.target

QMAKE_EXTRA_COMPILERS += doxygen xpatterns
DOXYFILES += Doxyfile
XPATTERNS += engineparams.xq
INSTALLS += docs docsearch
