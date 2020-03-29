#!/bin/bash

CC="./2cc"

try(){
    title=$1
    expected=$2
    input=$3

    tmpfile="temp.c"
    echo -e  "$input" > $tmpfile
    $CC $tmpfile
    gcc -c sample.s
    gcc -o tmp sample.o

    ./tmp
    actual="$?"

    rm $tmpfile sample.s sample.o tmp
    echo -n "$title => "
    if [ $actual == $expected ]; then
          echo -e "\e[32m OK \e[m"
    else
        echo -e "\e[31m NG \e[m: $expected expected, but got $actual"
        exit 1
    fi
}

try "literal test" 1 "int main(){ return 1;}"
try "add test" 5 "int main(){return 2 + 3;}"
try "sub test" 10 "int main(){return 15 - 5;}"
try "mul test" 16 "int main(){return 4 * 4;}"
try "g_var test" 4 "int x = 4; int main(){return x;}"
try "g_var_assign test" 9 "int x = 4; int main(){x = 4 + 5;return x;}"