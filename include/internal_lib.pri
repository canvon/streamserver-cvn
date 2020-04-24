!defined(SSCVN_REL_ROOT, var): error(Need qmake project variable SSCVN_REL_ROOT set before including internal_lib.pri)
!defined(SSCVN_LIB_NAME, var): error(Need qmake project variable SSCVN_LIB_NAME set before including internal_lib.pri)

SSCVN_LIB_LINK_LIB = -l$${SSCVN_LIB_NAME}
SSCVN_LIB_REL_DIR  = $${SSCVN_REL_ROOT}/lib$${SSCVN_LIB_NAME}
SSCVN_LIB_LINK_DIR = -L$${OUT_PWD}/$${SSCVN_LIB_REL_DIR}

# Link against internal library as specified by variable values during qmake include.
win32:CONFIG(release, debug|release):    LIBS += $${SSCVN_LIB_LINK_DIR}/release/ $$SCVN_LIB_LINK_LIB
else:win32:CONFIG(debug, debug|release): LIBS += $${SSCVN_LIB_LINK_DIR}/debug/   $$SCVN_LIB_LINK_LIB
else:unix: LIBS += $$SSCVN_LIB_LINK_DIR/ $$SSCVN_LIB_LINK_LIB

# Allow to find the lib at runtime via relative path.
!isEmpty(QMAKE_REL_RPATH_BASE): QMAKE_RPATHDIR += $$SSCVN_LIB_REL_DIR

# N.B.: PWD will point at this project include file's directory!
#   It'll *not* point at the directory where an in-place build would place
#   its Makefile... So using SSCVN_REL_ROOT would be wrong, here.
INCLUDEPATH += $$PWD/../lib$${SSCVN_LIB_NAME}
DEPENDPATH  += $$PWD/../lib$${SSCVN_LIB_NAME}

# Unset our temporary variables again.
unset(SSCVN_LIB_LINK_DIR)
unset(SSCVN_LIB_REL_DIR)
unset(SSCVN_LIB_LINK_LIB)

# Unset input variable, too, so it needs to be given again before another include will work.
unset(SSCVN_LIB_NAME)
