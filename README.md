# SandBox-UASM v2
Improved universal program assembler for Sand:Box CPUs


## Syntax

Each line should start with an instruction template or a compiler command.
 
The `\` symbol can be used to connect multiple lines in the script.

`//` and `/**/` can be used to mark single line and multi-line comments.

The compiler supports mathematical expressions.\
Supported operators include `+`, `-`, `*`, `/`, `**`, `==`, `!=`, `>`, `<`, `>=`, `<=`, `||`, `&&`, `|`, `&`, `^`, `~`, `!` and grouping with `()`.\
`int`, `float`, `string` and `id` operators can be used to convert expressions between different types.


## Compiler commands

Compiler commands can be used to control the compilation process and manage different aspects of code assembly.

- `%define ['global'] ['eval'] <macro> [value...]`\
  Defines a macro.\
  Macro can be defined as global using the `global` keyword.\
  Local macros are automatically undefined when exiting the file's scope, while global macros persist until explicitly undefined.\
  The `eval` keyword can be used to evaluate all macros used in the new macro's value.

- `%undef ['global'] <macro>`\
  Undefines the specified macro.\
  `global` keyword can be used to remove global macros.

- `%include <file> [args...]`\
  Includes the content of the specified file.\
  Arguments can be passed after the file path to customize the included code. The arguments will be passed to the file as local macros in the form of `%arg[num]`.\
  Additionally, the command will define `%path`, `%name`, and `%argn` macros, evaluating to the path to the current file, the name of the file and the number of passed arguments respectively.\
  To specify the full path to file with the root being the working directory of the compilation, it should be preceeded by a `/` symbol.

- `%if <cond>`\
  If the condition `<cond>` is true, the program after the command is processed normally.\
  Otherwise, the compiler jumps to the corresponding `%endif` command.
  
- `%marker <text>`\
  Adds a marker to the compiled code, which can help with debugging and identyfying the locations of selected code fragments.

- `%error [code] <description>`\
  Generates an error with the specified code.\
  This command can be used to trigger errors for debugging purposes.

- `%file_push <path> [args...]`\
  Pushes a new file onto the stack for processing. The file's path and optional parameters can be provided. Each file push isolates macros and other settings within its scope.\
  **However, it's important to remember that manually managing the file stack can lead to many potential errors, and it is strongly recommended to use `%include` command instead.**

- `%file_pop`
  Pops the most recently pushed file off the stack.


## Instruction format

Instructions can be manually constructed using the following format:
The instruction needs to start with `_` symbol, followed by the number of bytes it occupies and the list of expected arguments.
Each parameter is specified by a letter: `i` for integer, `r` for register, and `n` for null/empty, along with the number of bits it occupies in the generated instruction.
If the provided arguments don't match the expected format, an appriopriate error is returned.

Here are some examples:

`_2i4r4r4r4 5 R2 R6 R8`\
  Compiles to `0x5268`.

`_2i5i10 0b10001 0b10110111`\
  Compiles to `0x896E` (`0b10001 0010110111 0`).

`_3i1n5r3 1 R4`\
  Compiles to `0x820000` (`0b1 00000 100 0000000 00000000`).

`_2i1r2 R5 R2`\
  Returns `UnexpectedToken` error.

`_2i4 0b11111`\
  Returns `InvalidRange` error.
