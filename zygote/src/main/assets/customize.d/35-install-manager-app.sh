#!/system/bin/sh

APK_FILE=$MODPATH/manager.apk

# Only user 0 is managed. Remove the existing manager app first so the bundled
# APK always replaces it; HMA's configuration in /data/misc is untouched.
if pm path --user 0 org.frknkrc44.hma_oss > /dev/null 2>&1
then
  ui_print "- Uninstalling existing manager app for user 0"
  pm uninstall --user 0 org.frknkrc44.hma_oss > /dev/null 2>&1
fi

ui_print "- Installing manager app for user 0"
pm install --user 0 "$APK_FILE" 2>&1

[ $? -ne 0 ] && ui_print "! Cannot install the manager app for user 0"
