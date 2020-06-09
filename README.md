# Alopecurus [![Build Status](https://www.travis-ci.org/ueyudiud/Alopecurus.svg?branch=master)](https://www.travis-ci.org/ueyudiud/Alopecurus)
Alopecurus is a light script language run on VM.
## Getting Started
### Compilation
**This project is compatible with gcc and Clang.**

To build the project in Microsoft Windows, run the following command in `src` directory .
```
make
```
The project should support to be built on Linux and MacOS by the same process, but it has been not examined yet.
If you are not using these OS, you may need to reconfigurate the makefile.

### Running Interactive Interpreter
The resulting excutive file of the compile process can be used as an interpreter. Running the folloing command will start a interactive interpreter session.
```
alo
```
 Insert following code
```
println 'Hello world'
```
and see the result. Use `sys.exit()` to quit the shell.

