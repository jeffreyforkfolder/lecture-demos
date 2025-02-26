#Utility to list all TODOs in sources, headers, scripts and Makefiles
# Andreas Unterweger, 2017-2018
#This code is licensed under the 3-Clause BSD License. See LICENSE file for details.

echo "To-dos in source and header files:"
find . -iname '*.[ch]pp' -print | xargs grep -o --with-filename --line-number --color=always -e '/[/*]TODO: .*'  | sed 's/\/[/*]TODO: \(.*\)/\1/' | sed 's/\*\/$//'
echo "To-dos in script and Make files:"
find . -iname 'Makefile' -print -o -iname '*.mak' -print -o -iname '*.sh' -print | grep -v ./todo.sh | xargs grep -o --with-filename --line-number --color=always -e '#TODO: .*'  | sed 's/#TODO: \(.*\)/\1/'
