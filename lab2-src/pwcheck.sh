#!/bin/bash

#DO NOT REMOVE THE FOLLOWING TWO LINES
git add $0 >> .local.git.out
git commit -a -m "Lab 2 commit" >> .local.git.out
git push >> .local.git.out || echo


#Your code here
word=$(cat<$1)
length=${#word}

min=6
max=32

score=0

#if password length is less than 6 or more than 32 characters long
if (($length < $min)); then
	echo "Error: Password length invalid"
	exit
elif (($length > $max)); then
	echo "Error: Password length invalid"
	exit
fi

#add 1 pt for each character in string
score=$length

#if password contains special characters
if egrep -q [#$+%@] $1; then
	((score+=5))
fi

#if password contains at least one number
if egrep -q [0-9] $1; then
	((score+=5))
fi

#if password contains at least one alpha character
if egrep -q [A-Za-z] $1; then
	((score+=5))
fi

#if password contains a repeated alphanumeric character
if egrep -q '([A-Za-z0-9])\1+' $1; then
	((score-=10))
fi

#if password contains 3 or more consecutive lowercase characters
if egrep -q '[a-z][a-z][a-z]' $1; then
	((score-=3))
fi

#if password contains 3 or more consecutive uppercase characters
if egrep -q '[A-Z][A-Z][A-Z]' $1; then
	((score-=3))
fi

#if password contains 3 or more consecutive numbers
if egrep -q '[0-9][0-9][0-9]' $1; then
	((score-=3))
fi

#Print password score
echo "Password Score: $score"

