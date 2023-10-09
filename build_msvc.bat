for %d in (*.c) do cl /DON_WINDOWS /DUNICODE /O2 /W4 /std:c17 /c %d
link /lib *.obj /out:libjodycode.lib