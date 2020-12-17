# mokara.3
1. You have to use make to compile the software
  $ make
gcc -Wall -Werror -g -o oss oss.c -lpthread
gcc -Wall -Werror -g -o user user.c -lpthread

2. Run the program
  $ ./oss -v > log.txt
