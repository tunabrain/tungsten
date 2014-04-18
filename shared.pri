QMAKE_CXXFLAGS += -std=c++0x -mavx2 -mfma -Wno-unused-local-typedefs
INCLUDEPATH += $$quote($$top_srcdir/src)
DEPENDPATH += $$quote($$top_srcdir/src)

debug {
	CORELIB = $$top_builddir/src/core/debug
} else {
	CORELIB = $$top_builddir/src/core/release
}
CORELIBFILE = $$CORELIB/libcore.a

RC_FILE = $$quote($$top_builddir/Tungsten.rc)

LIBS += -lembree -lsys -ltbb -lglew32 -L$$quote($$CORELIB)