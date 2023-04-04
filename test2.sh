#!/bin/bash

if [ $# -ne 4 ]; then
  echo "Usage: test.sh <type(a2a|tree)> <num super peers> <num peers per super peer> <clients issuing queries> "
  exit 1
fi
if [ $1 != "a2a" ] && [ $1 != "tree" ]; then
  echo "Usage: test.sh <type(a2a|tree)> <num super peers> <num peers per super peer> <clients issuing queries> "
  exit 1
fi

if [ $2 -lt 1 ] || [ $2 -gt 10 ]; then
  echo "Number of super peers must be between 1 and 10"
  exit 1
fi

if [ $3 -lt 1 ] || [ $3 -gt 5 ]; then
  echo "Number of peers per super peer must be between 1 and 5"
  exit 1
fi

if [ $4 -lt 1 ] || [ $4 -gt $(($2 * $3)) ]; then
  echo "Number of clients issuing queries must be between 1 and $(($2 * $3))"
  exit 1
fi

./init.sh $1 $2 $3

numbers=($(seq 1 $(($2 * $3))))
shuf_numbers=($(shuf -e "${numbers[@]}"))
result=()

for i in "${!shuf_numbers[@]}"; do
  if [[ "${shuf_numbers[i]}" -ne "$i"+1 ]]; then
    result+=("${shuf_numbers[i]}")
  else
    for j in "${!shuf_numbers[@]}"; do
      if [[ "${shuf_numbers[j]}" -ne "$j"+1 ]] && [[ "${shuf_numbers[j]}" -ne "${shuf_numbers[i]}" ]]; then
        temp="${shuf_numbers[j]}"
        shuf_numbers[j]="${shuf_numbers[i]}"
        shuf_numbers[i]="$temp"
        result+=("${shuf_numbers[i]}")
        break
      fi
    done
  fi
done

numbers=($(seq 1 $(($2 * $3))))
shuf_numbers=($(shuf -e "${numbers[@]}"))
result2=()

for i in "${!shuf_numbers[@]}"; do
  if [[ "${shuf_numbers[i]}" -ne "$i"+1 ]]; then
    result2+=("${shuf_numbers[i]}")
  else
    for j in "${!shuf_numbers[@]}"; do
      if [[ "${shuf_numbers[j]}" -ne "$j"+1 ]] && [[ "${shuf_numbers[j]}" -ne "${shuf_numbers[i]}" ]]; then
        temp="${shuf_numbers[j]}"
        shuf_numbers[j]="${shuf_numbers[i]}"
        shuf_numbers[i]="$temp"
        result2+=("${shuf_numbers[i]}")
        break
      fi
    done
  fi
done

numbers=($(seq 1 $(($2 * $3))))
shuf_numbers=($(shuf -e "${numbers[@]}"))
result3=()

for i in "${!shuf_numbers[@]}"; do
  if [[ "${shuf_numbers[i]}" -ne "$i"+1 ]]; then
    result3+=("${shuf_numbers[i]}")
  else
    for j in "${!shuf_numbers[@]}"; do
      if [[ "${shuf_numbers[j]}" -ne "$j"+1 ]] && [[ "${shuf_numbers[j]}" -ne "${shuf_numbers[i]}" ]]; then
        temp="${shuf_numbers[j]}"
        shuf_numbers[j]="${shuf_numbers[i]}"
        shuf_numbers[i]="$temp"
        result3+=("${shuf_numbers[i]}")
        break
      fi
    done
  fi
done

echo "Starting ${2} super peers"

for i in $(seq 1 $2); do
  ./superpeer.o peer_configs/super_peers/superpeer${i}.cfg > logs/superpeer${i}.log 2>&1 &
done

sleep $2

j=$(($2 * $3))

querying_peers=()

while [ ${#querying_peers[@]} -lt $4 ]; do
  r=$((1 + RANDOM % j))
  if [[ ! " ${querying_peers[@]} " =~ " $r " ]]; then
    querying_peers+=("$r")
  fi
done

echo "Starting ${j} peers"

echo "querying peer numbers are ${querying_peers[@]}"

testfilesnum=$(((100 / 5) / $4))

for x in $(seq 1 $j); do
    if [[ " ${querying_peers[@]} " =~ " ${x} " ]]; then
        ./peernode.o peer_configs/weak_peers/peer${x}.cfg tests/test${result[$x-1]}.txt,tests/test${result2[$x-1]}.txt,tests/test${result3[$x-1]}.txt > logs/peer${x}.log 2>&1 &
    else
        ./peernode.o peer_configs/weak_peers/peer${x}.cfg -h > logs/peer${x}.log 2>&1 &
    fi
done