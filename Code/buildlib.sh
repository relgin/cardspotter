if ! [ -x "$(command -v python)" ]; then
  echo 'Error: python is not installed. Try sudo apt-get install python' >&2
  exit 1
fi

# source emsdk_set_env.sh

OPENCV=opencv

rm -f cardspotter.js
rm -f cardspotter.wasm

rm -f ../MagicCardSpotter/cardspotter.js
rm -f ../MagicCardSpotter/cardspotter.wasm

echo "Building..."
# -s USE_PTHREADS=1 -Wl,--shared-memory,--no-check-features -s PTHREAD_POOL_SIZE=4
em++ -s WASM=1 -std=c++11 -O3 --llvm-lto 1 -s FETCH=1 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s EXCEPTION_DEBUG=0 -s ALLOW_MEMORY_GROWTH=0\
 -s STRICT=1 -s MODULARIZE=1 -s 'EXPORT_NAME="CardSpotter"' \
 -s NO_EXIT_RUNTIME=1 -s DEMANGLE_SUPPORT=0 -s DISABLE_EXCEPTION_CATCHING=1 -s INITIAL_MEMORY=268435456 -s USE_LIBPNG=1 -s VERBOSE=0 -s ERROR_ON_UNDEFINED_SYMBOLS=1 -s USE_ZLIB=1 cardspotterlib.cpp\
  QueryThread.cpp CardDatabase.cpp CardData.cpp\
   $OPENCV/build_wasm/lib/libopencv_features2d.a\
   $OPENCV/build_wasm/lib/libopencv_imgproc.a\
   $OPENCV/build_wasm/lib/libopencv_imgcodecs.a\
   $OPENCV/build_wasm/lib/libopencv_core.a\
   $OPENCV/build_wasm/lib/libopencv_flann.a\
   -I$OPENCV/build_wasm/\
   -I$OPENCV/modules/core/include/\
   -I$OPENCV/modules/imgcodecs/include/\
   -I$OPENCV/modules/highgui/include/\
   -I$OPENCV/modules/photo/include/\
   -I$OPENCV/modules/imgproc/include/\
   -I$OPENCV/modules/flann/include/\
   -I$OPENCV/modules/features2d/include/\
   --bind\
   -o cardspotter.js

cp -v cardspotter.js ../MagicCardSpotter/cardspotter.js
cp -v cardspotter.wasm ../MagicCardSpotter/cardspotter.wasm

