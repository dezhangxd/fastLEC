clear;
clear;

./build.sh

BUILD_DIR="build"

# ulimit -t 5

command="-m pES -c 1 -v 2 -t 5"

./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit2/mul2o2.aig $command |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit5/mul5o5.aig $command |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit13/mul13o13.aig $command |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit16/mul16o16.aig $command |gnomon


# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit2/miter2o2.aig $command |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit13/miter13o13.aig $command |gnomon

