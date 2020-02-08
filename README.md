# Alopecurus [![Build Status](https://www.travis-ci.org/ueyudiud/Alopecurus.svg?branch=master)](https://www.travis-ci.org/ueyudiud/Alopecurus)
Alopecurus is a light script language run on VM.
## Getting Started
### Compilation
**It is only ensured that this project is compilable by gcc now**, and Clang is considered to be supported in the future. In Windows OS, run following command in terminal to build project in 'src' directory:
```
make
```
If your are not Windows users, you need to change some configuration in makefile.
### Run Script
The project can be used independently, run executable file in terminal after compilation:
```
alo
```
Then, a Alopecurus shell will be opened. Insert following code:
```
println 'Hello world'
```
And you will see output message on the shell, you can insert `:q` to quit the shell.