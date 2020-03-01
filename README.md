# Alopecurus [![Build Status](https://www.travis-ci.org/ueyudiud/Alopecurus.svg?branch=master)](https://www.travis-ci.org/ueyudiud/Alopecurus)
Alopecurus is a light script language run on VM.
## Getting Started
### Compilation
**This project is only compilable with gcc or Clang for now.**

To build the project in Microsoft Windows, run the following command in `src` directory .
```
make
```
If you're not using Microsoft Windows, you may need to reconfigurate the makefile.

### Running Interactive Interpreter
The resulting excutive file of the compile process can be used as an interpreter. Running the folloing command will start a interactive interpreter session.
```
alo
```
 Insert following code
```
println 'Hello world'
```
and see the result. Use `:q` to quit the shell.
