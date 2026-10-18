#!/system/bin/sh
# CQ_LANG is passed by the WebUI; installers use the device's primary language.
case "${CQ_LANG:-}" in
    zh|en) ;;
    *)
        cq_locale=$(getprop persist.sys.locale 2>/dev/null || true)
        [ -n "$cq_locale" ] || cq_locale=$(getprop persist.sys.language 2>/dev/null || true)
        [ -n "$cq_locale" ] || cq_locale=$(getprop ro.product.locale 2>/dev/null || true)
        case "$cq_locale" in zh|zh-*|zh_*) CQ_LANG=zh;; *) CQ_LANG=en;; esac
        ;;
esac
export CQ_LANG
cq_message() {
    if [ "$CQ_LANG" = zh ]; then printf '%s\n' "$1"; else printf '%s\n' "$2"; fi
}
