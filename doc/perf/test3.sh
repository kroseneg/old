
for i in `seq 1000 100 10000`; do
	for j in `seq 1 2`; do
		printf "%d " $i;
		bash -c "TIMEFORMAT=\"%3R\"; time ./bin/oldtest2 localhost $i > /dev/null"
	done
done


