clear;
clear;

# ./clean.sh
./build.sh

BUILD_DIR="build"

# ulimit -t 5

command="-m ES -c 1 -v 2 -t 5 -p ies_u64 1"
command="-m pES -c 4 -v 2 -t 20 -p use_pes_pbit 0"
command="-m SAT_sweeping -c 4 -v 2 -t 20 "

# ./${BUILD_DIR}/bin/fastLEC -i ../ins/all/test_16_TOP11.aiger $command |gnomon

# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit5/mul5o5.aig $command |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit13/mul13o13.aig $command |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit16/mul16o16.aig $command |gnomon

# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit13/miter13o13.aig $command |gnomon

