for i in {0..19}
do
  idx="$i"
  if [ $i -lt 10 ]; then
    idx="0$i"
  fi
  clang -emit-llvm -c -O0 -g3 test$idx.c -o test$idx.bc
  clang -emit-llvm -S -O0 -g3 test$idx.c -o test$idx.ll
done