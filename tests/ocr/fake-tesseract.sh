#!/bin/sh
if [ "$FLAMESHOT_OCR_TEST_MODE" = bundled ]; then
    [ -f "$TESSDATA_PREFIX/eng.traineddata" ] &&
        [ -f "$TESSDATA_PREFIX/chi_sim.traineddata" ] || exit 8
    case "$LD_LIBRARY_PATH" in
        *local-deps/usr/lib/x86_64-linux-gnu*) ;;
        *) exit 8 ;;
    esac
fi
if [ "$1" = "--list-langs" ]; then
    printf 'List of available languages (2):\neng\n'
    if [ "$FLAMESHOT_OCR_TEST_MODE" != missing_language ]; then
        printf 'chi_sim\n'
    fi
    exit 0
fi
[ "$#" = 6 ] && [ -f "$1" ] && [ "$2" = stdout ] &&
    [ "$3" = -l ] && [ "$4" = chi_sim+eng ] &&
    [ "$5" = --psm ] && [ "$6" = 11 ] || exit 9
printf '%s' "$1" > "$FLAMESHOT_OCR_TEST_DIR/image"
printf '%s' "$$" > "$FLAMESHOT_OCR_TEST_DIR/pid"
case "$FLAMESHOT_OCR_TEST_MODE" in
    empty) exit 0 ;;
    failure) printf 'Invalid OCR image' >&2; exit 7 ;;
    slow) exec /bin/sleep 60 ;;
    *) printf '中文识别\nHello OCR 123\n' ;;
esac
