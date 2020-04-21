all: oss user clock readme

oss: oss.o
	gcc -lpthread -g -o oss oss.o
	
user: user.o
	gcc -lpthread -g -o user user.o
	
clock: clock.o
	gcc -g -o clock clock.o
	
oss.o: oss.c
	gcc -lpthread -g -c oss.c

user.o: user.c 
	gcc -lpthread -g -c user.c
	
clock.o: clock.c
	gcc -g -c clock.c
	
clean: remove

remove:
	rm *.o oss user clock *.out

clear: 
	clear
	
success: 
	$(info SUCCESS)
	
readme:
	cat README