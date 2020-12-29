#!/usr/bin/env bash


test () {
    timeout 1m ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> tests/test$1/debug.txt
    diff tests/test$1/output.txt tests/test$1/expected.txt
    if [[ $? -eq 0 ]]
    then
        echo "Test-$1: SUCCESS"
    else
        echo "Test-$1: FAILED"
    fi
    #rm tests/test$1/output.txt
}

exit_test () {
    timeout 1m ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> tests/test$1/debug.txt
	#ret=$?

    if [[ $? -eq 255 ]]
    then
        echo "Exit-Test-$1: SUCCESS"
    else
        echo "Exit-Test-$1: FAILED"
    fi

}

play_match () {
    gcc -D MOVE_FILE=1 -o tests/moves/player1 tests/moves/player.c
    gcc -D MOVE_FILE=2 -o tests/moves/player2 tests/moves/player.c
    gcc -D MOVE_FILE=3 -o tests/moves/player3 tests/moves/player.c
    #./umpire2_oracle tests/moves/players$1.txt > tests/moves/expected$1.txt 2> tests/moves/debug_umpire2_oracle$1.txt
    timeout 1m ./umpire2 tests/moves/players$1.txt > tests/moves/output$1.txt 2> tests/moves/debug_umpire2$1.txt
    diff tests/moves/output$1.txt tests/moves/expected$1.txt

    if [[ $? -eq 0 ]]
    then
        echo "Test-Match-$1: SUCCESS"
    else
        echo "Test-Match-$1: FAILED"
    fi
    # rm tests/moves/player1
    # rm tests/moves/player2
}


play_match_walkover () {
    rm umpire2
    #rm umpire2_oracle
    gcc -o umpire2 umpire2.c gameUtils1.c # use different walkover function
    #gcc -o umpire2_oracle umpire2_oracle.c gameUtils1.c

    gcc -D MOVE_FILE=1 -o tests/moves/player1 tests/moves/player.c
    gcc -D MOVE_FILE=2 -o tests/moves/player2 tests/moves/player.c
    gcc -D MOVE_FILE=3 -o tests/moves/player3 tests/moves/player.c
    #./umpire2_oracle tests/moves/players$1.txt > tests/moves/expected$1.txt 2> tests/moves/debug_umpire2_oracle$1.txt
    timeout 1m ./umpire2 tests/moves/players$1.txt > tests/moves/output$1.txt 2> tests/moves/debug_umpire2$1.txt
    diff tests/moves/output$1.txt tests/moves/expected$1.txt
    if [[ $? -eq 0 ]]
    then
        echo "Test-Match-Walkover-$1: SUCCESS"
    else
        echo "Test-Match-Walkover-$1: FAILED"
    fi
    # rm tests/moves/player1
    # rm tests/moves/player2
}


wait_test () {
    timeout 1m strace -c -o tests/test$1/straceOut ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> /dev/null
    grep -nrw "wait4" tests/test$1/straceOut > /dev/null
	#grep on successfull search returns: 0
	#grep on unsuccessfull search returns: 1

    if [[ $? -eq 0 ]]
    then
        echo "Wait-Test-$1: SUCCESS"
    else
        echo "Wait-Test-$1: FAILED"
    fi
}

exec_test () {
    timeout 1m strace -cf -o tests/test$1/straceOut ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> /dev/null
    #grep "execve" tests/test$1/straceOut  | cut  -c36-40 > tests/test$1/numExec #since we used -c40-50,this means numExec contains 10(11?) chars (can be blank)
    #diff tests/test$1/numExec tests/test$1/expectedNumExec
	totalExecve=$(grep "execve" tests/test$1/straceOut  | cut  -c36-40)
	#grep on successfull search returns: 0
	#grep on unsuccessfull search returns: 1

    if [[ $totalExecve -eq 8 ]]
    then
        echo "Exec-Test-$2: SUCCESS"
    else
        echo "Exec-Test-$2: FAILED"
    fi

}


fork_test () {
    timeout 1m strace -cf -o tests/test$1/straceOut ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> /dev/null
    totalForks=$(grep "clone" tests/test$1/straceOut  | cut  -c36-40) #If we use -c40-50,this means numExec contains 10(11?) chars (can be blank)

    if [[ $totalForks -ge 7 ]]
    then
        echo "Fork-Test-$2: SUCCESS"
    else
        echo "Fork-Test-$2: FAILED"
    fi
}




alt_moves_test (){

    	timeout 1m strace -f -e write -o tests/test$1/straceOut ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> /dev/null
    	grep -o ".*write.*GO.*" tests/test$1/straceOut | cut -b 7- | cut -d' ' -f1,2 > tests/test$1/straceGO
	#echo "Checkout straceGO"

	totalLines=$(wc -l tests/test$1/straceGO| cut -d' ' -f1)
	#echo $totalLines
	lastLine=2
	firstLine=1
	result=1
  if [[ $totalLines -lt 2 ]]; then
    echo "Alternate-Moves-Test-$2: FAILED"
    result=0
  fi
	while [ $lastLine -le $totalLines ]; do
		#echo $(head -$lastLine straceGO | tail +$firstLine)
		numLines=$(head -$lastLine tests/test$1/straceGO | tail --line +$firstLine | uniq | wc -l)
		#echo $numLines
    if [[ $numLines == 1 ]]; then
			echo "Alternate-Moves-Test-$2: FAILED"
			#echo $(head -$lastLine tests/test$1/straceGO | tail +$firstLine)
			result=0
			break
		fi

		((lastLine++))
		((firstLine++))
	done

	if [[ $result == 1 ]]; then
		echo "Alternate-Moves-Test-$2: SUCCESS"
	fi

}


termination_test (){

    	timeout 1m strace -f -o tests/test$1/straceOut ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> /dev/null
	firstExit=$(grep -n -o ".*exited with 0.*" tests/test$1/straceOut | cut -d':' -f1 | head -n 1)
	tail --line +$firstExit tests/test$1/straceOut > tests/test$1/temp
    	goMsgsSent=$(grep -o ".*write.*GO.*" tests/test$1/temp | wc -l)

  if [[ $goMsgsSent -ge 60 ]]; then	#No. of messages sent after 1st level, dependent upon players.txt used by us in this test case
		echo "Termination-Test-$2: SUCCESS"
	else
		echo "Termination-Test-$2: FAILED"
	fi

}

wait_termination_test (){

    	timeout 1m strace -f -o tests/test$1/straceOut ./umpire2 tests/test$1/players.txt > tests/test$1/output.txt 2> /dev/null
	firstWait=$(grep -n -o "wait4" tests/test$1/straceOut | cut -d':' -f1 | head -n 1)
	tail --line +$firstWait tests/test$1/straceOut > tests/test$1/temp
    	goMsgsSent=$(grep -o ".*write.*GO.*" tests/test$1/temp | wc -l)

  if [[ $goMsgsSent -ge 60 ]]; then	#No. of messages sent after 1st level, dependent upon players.txt used by us in this test case
		echo "Wait-Plus-Termination-Test-$1: SUCCESS"
	else
		echo "Wait-Plus-Termination-Test-$1: FAILED"
	fi

}


make
make test

test 1			#test1
test 2			#test2
test 3			#test3
test 4			#test4
exit_test 5		#test5
exit_test 6		#test6
termination_test 8 7	#test7
exec_test 10 8		#test8
alt_moves_test 11 9	#test9
fork_test 12 10		#test10

play_match 1		#test11
play_match 2		#test12
play_match 3		#test13
play_match 4		#test14
play_match 5		#test15
play_match 6		#test16

play_match_walkover 7	#test17
