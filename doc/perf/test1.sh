
# test streaming connectons over localhost
# it repeats each test twice

for i in `seq 1000 100 10000`; do
	for j in `seq 1 2`; do
		printf "%d " $i;
		bash -c "TIMEFORMAT=\"%3R\"; time ./bin/oldtest localhost $i > /dev/null"
	done
done


