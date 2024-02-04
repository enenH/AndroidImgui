REM  m.bat | $CMakeCurrentTargetName$ | $ProjectFileDir$
REM adb connect ip:port
adb push cmake-build-debug\%1 /data/local/tmp
adb shell su -c chmod +x /data/local/tmp/%1
adb shell su -c /data/local/tmp/%1 -h
