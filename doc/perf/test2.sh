
# opens two oldtest in paralell and time its execution

for i in `seq 1 10`; do
	bash -i -l -c "TIMEFORMAT=\"%3R\"; \
		time { \
			./bin/oldtest localhost 10000 > /dev/null & \
			./bin/oldtest localhost 10000 > /dev/null & \
			wait %1 > /dev/null; \
			wait %2 > /dev/null; \
		}; 2> /dev/null"
done

