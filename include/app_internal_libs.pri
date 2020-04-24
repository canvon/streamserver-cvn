# Allow to find internal libs at runtime via relative path,
# or using a well-known absolute path where they can be expected.
!isEmpty(QMAKE_REL_RPATH_BASE): QMAKE_RPATHDIR += .
unix: QMAKE_RPATHDIR += /usr/lib/streamserver-cvn

# Link against specified internal libraries.
for(SSCVN_LIB_NAME, SSCVN_LIB_NAMES): include($${SSCVN_REL_ROOT}/include/internal_lib.pri)
