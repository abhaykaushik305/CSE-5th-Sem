
cat 'Task'$1/'testcase'$2'.c' > ./init.c
scp -P 2020 ./init.c osws@localhost:./gemOS/src/user/init.c
cat 'Task'$1/'testcase'$2'.output'
