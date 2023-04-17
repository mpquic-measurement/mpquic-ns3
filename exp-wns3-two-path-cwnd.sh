
#!/bin/bash

FOLD="cwnd-$1-$2"
DIR="results-wns3/$FOLD/"
RATE0a="10.0"
RATE0b="10.0"
RATE1a="10.0"
RATE1b="10.0"
DELAY0a="10.0"
DELAY0b="10.0"
DELAY1a="10.0"
DELAY1b="10.0"
LOSS="0.00000"
BLambda="200"
BVar="0"
Size="0"
SEED=$2
CWND=$1

mkdir ${DIR}
for i in 0
do
    LOG="$i"
    LOG1="scheduler${LOG}-queue.txt"
    LOG2="scheduler${LOG}-flowsum.txt"
    ./waf --run "scratch/wns3-mpquic-two-path-cwnd.cc --Seed=$SEED --Size=$Size --BVar=$BVar --BLambda=$BLambda --SchedulerType=${LOG} --Rate0a=${RATE0a} --Rate1a=${RATE1a} --Delay0a=${DELAY0a} --Delay1a=${DELAY1a} --Rate0b=${RATE0b} --Rate1b=${RATE1b} --Delay0b=${DELAY0b} --Delay1b=${DELAY1b} --CcType=${CWND} --LossRate=$LOSS" >$LOG1 2>$LOG2

    FILE="scheduler${LOG}-rx.txt"
    cp $FILE "${DIR}${FILE}"
    rm $FILE

    FILE="scheduler${LOG}-cwnd-change-0.txt"
    cp $FILE "${DIR}${FILE}"
    rm $FILE

    FILE="scheduler${LOG}-cwnd-change-1.txt"
    cp $FILE "${DIR}${FILE}"
    rm $FILE

    cp $LOG1 "${DIR}${LOG1}"
    rm $LOG1

    cp $LOG2 "${DIR}${LOG2}"
    rm $LOG2

done
