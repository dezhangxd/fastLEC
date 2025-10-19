clear;
clear;

# ./clean.sh
./build.sh

BUILD_DIR="build"

# ulimit -t 5

# command="-m ES -c 1 -v 2 -t 5 -p ies_u64 1"
# command="-m pES -c 4 -v 2 -t 20 -p use_pes_pbit 0"
# command="-m SAT_sweeping -c 4 -v 2 -t 100 -p log_sub_cnfs 1 -p log_dir ./logs/"
# command="-m pBDD -c 32 -t 50"
command="-m p_hybrid_sweeping -c 16 -v 2 -t 500"
# command="-m pBDD -c 4 -v 2 -t 5 "

# rm -rf *.aig
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/all/test_11_TOP4.aiger $command |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/all/test_16_TOP30.aiger $command |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/all/test_16_TOP11.aiger $command |gnomon

# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit5/mul5o5.aig $command |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit14/mul14o14.aig $command |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit16/mul16o16.aig $command |gnomon

# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit13/miter13o13.aig $command |gnomon

# for i in $(ls *.aig | sort -V);do ./build/bin/fastLEC -i ./$i -m SAT;done