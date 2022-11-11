pass=0
count=0
for i in {0..19}
do
  count=`expr ${count} + 1`
  idx="$i"
  if [ $i -lt 10 ]; then
    idx="0$i"
  fi
  ans_file=ans$idx.txt
  ref_file=ref$idx.txt
  ../build/llvmassignment test$idx.bc &> $ans_file
  if [ "$(diff $ans_file $ref_file)" ];
   then
      echo "test$idx fail...."
   else
      echo "test$idx pass!!"
      pass=`expr ${pass} + 1`
   fi
done

echo $pass/$count passed!!