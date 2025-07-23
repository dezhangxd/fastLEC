clear;
clear;

# check command for build with cuda 
if [ "$1" = "no-cuda" ]; then
    echo "=== no cuda ==="
    rm -rf build
    mkdir -p build
    cd build
    cmake -DUSE_CUDA=OFF ..
    make -j
    cd ..
    BUILD_DIR="build"
else
    echo "=== with cuda ==="
    rm -rf build
    mkdir -p build
    cd build
    cmake ..
    make -j
    cd ..
    BUILD_DIR="build"
fi

# clear

# ./${BUILD_DIR}/bin/fastLEC -i /home/zhangxd/Experiment/EC/ins/dp2_pairs/test_11_TOP5_0.aiger -m ES -p use_ies 1
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit12/miter12o12.aig -m pES -c 10 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/all/test_11_TOP5.aiger -m pES -c 1 |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit16/mul16o16.aig -m pES -c 128 -v 2 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit12/mul12o12.aig -m pES -c 1 -v 2 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit12/miter12o12.aig -m gpuES -c 1 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit12/miter12o12.aig -m ES -p use_ies 1
# -c 1 -t 3600 -v 1 -m BDD -p max_iterations 1000000 -p seed 2

# dot -O -Tpdf ./logs_bdd/graph.dot

# backup: 
# 


# for i in {2..5}; do 
#     ./build/bin/fastLEC -i ../ins/miter_right/bit${i}/mul${i}o${i}.aig -m gpuES -c 1 -v 2 |gnomon  > log/mul${i}o${i}.log
# done

# for i in {6..9}; do 
#     ./build/bin/fastLEC -i ../ins/miter_right/bit${i}/mul${i}o0${i}.aig -m gpuES -c 1 -v 2 |gnomon  > log/mul${i}o0${i}.log
# done

# for i in {10..20}; do 
#     ./build/bin/fastLEC -i ../ins/miter_right/bit${i}/mul${i}o${i}.aig -m gpuES -c 1 -v 2 |gnomon  > log/mul${i}o${i}.log
# done