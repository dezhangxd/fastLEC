clear;
clear;


BUILD_DIR="build"

# ulimit -t 5


# ./${BUILD_DIR}/bin/fastLEC -i /home/zhangxd/Experiment/EC/ins/dp2_pairs/test_11_TOP5_0.aiger -m ES -p use_ies 1
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit12/miter12o12.aig -m pES -c 10 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/all/test_11_TOP6.aiger -m SAT -c 1 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_error/bit11/miter11o11.aig -m ES -c 1 -v 2 -t 5 |gnomon
./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit13/mul13o13.aig -m ES -c 1 -v 2 -t 5 |gnomon
# ./${BUILD_DIR}/bin/fastLEC -i ../ins/miter_right/bit19/mul19o19.aig -m BDD -c 1 -v 2 -t 5 |gnomon
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