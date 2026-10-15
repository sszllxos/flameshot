# Offline text recognition

Start `flameshot gui`, select an area, then click **Recognize Text** (the framed
T icon) or press **Ctrl+Shift+O**. Without a selection, the whole screenshot is
recognized. OCR reads the current selection, including any annotations.

The result window recognizes Simplified Chinese and English locally. Edit the
result if needed, then choose **Copy Text** to copy it and finish the capture.
**Cancel** or **Escape** stops recognition and returns to the screenshot editor.
Nothing is uploaded. Temporary images are removed when the result window closes.

If you have customized the toolbar, enable **Recognize Text** in the Interface
settings. The shortcut works even when the button is hidden. OCR is unavailable
when `gui` has an explicit output action such as `--clipboard` or `--raw`, so an
image output cannot overwrite the recognized text.

## Runtime dependencies

Tesseract is an optional runtime dependency; it is not needed to build Flameshot.
Flameshot first looks for a local installation at `../local-deps/usr` relative to
its executable directory, and configures that child process's library and
language-data paths automatically. Otherwise, `tesseract` must be on `PATH`, with
both `eng` and `chi_sim` language data available. On Ubuntu:

```sh
sudo apt-get install tesseract-ocr tesseract-ocr-eng tesseract-ocr-chi-sim
```

For a local installation, add its `bin` directory to `PATH`, its library directory
to `LD_LIBRARY_PATH` if needed, and set `TESSDATA_PREFIX` to the directory containing
`eng.traineddata` and `chi_sim.traineddata`. Verify with `tesseract --list-langs`.

For this workspace's local build, `./build/run-flameshot.sh gui` sets those paths;
the OCR engine is also found when starting `build/src/flameshot` directly.

## Tests

On Unix, configure with `-DBUILD_OCR_TESTS=ON`, build, and run
`ctest --test-dir build --output-on-failure`. The Qt tests use a temporary fake
Tesseract executable to exercise accepting edited Unicode text, missing dependencies,
empty output, engine errors, timeouts, cancellation, and temporary-file cleanup.

To also exercise real recognition, set `FLAMESHOT_OCR_REAL_PATH` to a `PATH`
containing Tesseract before running the tests. That smoke test requires the two
language packs and the Noto Sans CJK SC font; it renders Chinese and English text
and verifies the text returned through the result dialog.
