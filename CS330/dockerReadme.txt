STEP 4: Using the container
=======================
$docker start mycs330    // Starts the container
$ssh -p 2020 osws@localhost       // should take you to ssh prompt... use password 'cs330'  
$docker stop mycs330     //Stops the container



STEP 5: Executing gemOS
=====================
Start the container and login
After login:
$ cd gem5;
$./run.sh /home/osws/gemOS/src/gemOS.kernel




Open another ssh terminal to the container
$telnet localhost 3456     // will show you the gemOS console
