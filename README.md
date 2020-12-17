1. You have to use make to compile the software
  $ make
gcc -Wall -Werror -g -o oss oss.c -o oss
gcc -Wall -Werror -g -o user user.c -o user

2. Run the program
  $ ./oss -v > log.txt
