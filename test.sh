clear;
clear;

./build.sh no_cuda

BUILD_DIR="build"

command="-m schedule_sweeping -c 8 -v 2 -t 100"

./${BUILD_DIR}/bin/fastLEC -i ./data/test_16_TOP11.aiger $command
