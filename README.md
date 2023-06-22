# ForWhile
is a minimalistic Stack based Programming Language.

The language is named after its main control flow block the for-while loop (a mix of a for and do-while loop)

## Example Programs
Hello World:

```
"Hello World!"(,#)
```

Print all Fibonacci-numbers less than `1000`
```
{0:![64(.;10%48+;2+~,1+'10/';)]'.(,#)10#.}0$
1000 :(;'-1'0'(.;+'1).;;>:['0@?])
```

Quine:

```
0~100(.:@#.1-:@).
```

more examples can be found in the examples folder

## Usage
Compile the C source-file `src/ForWhile.c`
for example using GCC:

`gcc -Wall -Wextra "./src/ForWhile.c" -o "./forWhile"`

The run the generated executable

- To execute code directly use `./forWhile <source-code>`
- To execute a file use `./forWhile -f <source-file>`

(replacing `<source-code>`/`<source-file>` with the code/file to execute)

## Syntax
Operations are executed from left to right
the program is terminated when the execution hits a null character (`\0`)

The program ignores white-spaces and unassigned commands, the only place where separators are needed are between consecutive integer literals
and to distinguish the multi-character operators `'>` `'<` `~~` from their components

### Integers
An integer literal is a sequence of digits (`0` to `9` ) integer literals end at the first non-digit character

Example:

```
1
1024
123456789
```
results in the 3 numbers `123456789` `1024` and `1` being on the stack.

### Strings
String literals start with a `"` and end with the next non-escaped `"`
Within a string literal, every character (excluding `\`) directly pushes its corresponding char-code onto the stack.
Within strings `\` can be used to escape `"` and `\` as well as insert the space-characters `\n` `\t` and `\r`.

After the end of the string literal the total number of characters is pushed on the stack.

Examples:

```
"Hello World\n"
```

pushes the char-codes
`72` `101` `108` `108` `111` `32` `87` `111` `114` `108` `100` `10`
with the length `12` being on top of the stack

```
"\""
```

pushes the char-code of `"` (`34`) and the length (`1`).

### Comments

`\` comments out all code until the end of the current line
`\\\` block-comment, all code until next `\\\` will be commented out

### Operators
#### Stack manipulation
- `.` pops the top value from the stack `... <A> <B>` -> `... <A>`
- `:` duplicates the top value on the stack `... <A> <B>` -> `... <A> <B> <B>`
- `'` swaps the top two stack values `... <A> <B>` -> `... <A> <B>`
- `,` rotates the stack, takes the top element on the stack as argument:

    `... <A> <B> <C> <D> 3` -> `... <A> <C> <D> <B>` `... <A> <B> <C> <D> <E> -4` -> `... <A> <E> <B> <C> <D> `
    rotation by a positive amount will move the lowest element to the top and shift all other elements down by one,
    rotation by a negative value will move the top element to the bottom and shift all other elements up by one.

#### Memory
- `@` pushes the values at the address given by the top stack value `... <A> <B>` -> `... <A> memory[<B>]`
- `$` takes the top stack-value as address and stores the value below `... <A> <B> <C>` -> `... <A>` `memory[<C>]=<B>`
#### IO
- `#` prints the lowest byte of top-value on the stack as a character.
- `_` reads one character from standard input.
#### Arithmetic/Logic operations
- `+` `-` `*` `/` `%` binary arithmetic operation on top two stack values `... <A> <B>` -> `... <A <op> B>`
- `` ` `` computes integer powers `... <A> <B>` -> `... <pow(A,B)>`
- `&` `|` `^`  bit-wise logical operations on top two stack values `... <A> <B>` -> `... <A <op> B>`
- `<` `=` `>`  compares the top two stack values, will push `0` (false) or `1` (true) depending on the result of the comparison  `... <A> <B>` -> `... <A <op> B>`
- `'<` `'>`  (logical) bit-shift `... <A> <B>` -> `... <A <op> B>`
- `!` replaces the top stack value with its logical negation (`1` if value is zero otherwise `0`)
- `~` flip all bits in top stack value
- `~~` negate value on top of stack

### Code-Blocks
#### If-Blocks
`[` and `]` can be used to define if-conditions
`[` will jump to the matching `]` if the value on top of the stack is `0`,

<!--XXX? examples-->

#### For-While Loops
`(` and `)` can be used to define "for-while"-loops
When the program reaches a `(` the top stack value will be popped and used as the loop-counter.
While the loop counter is positive the code between `(` and `)` will be executed with the loop-counter as top-value on the stack,
each iteration the loop-counter is deceased by one.
When the instruction pointer reaches `)` the top stack value will be checked, if it is non-zero the program will continue the loop otherwise the loop will be ended.

Examples:

```
10(1)
```
pushes the numbers from `10` to `1` in decreasing order (`1` will be on top of stack)

```
(,#)
```
prints a string that is stored on top of the stack:
`,` rotates the lowest character of the string (indexed by the length in the loop-counter) to the top of the stack
`#` prints that character

#### Procedures
`{` and `}` can be used to define subroutines,
when the execution hits a `{` it pushes the current instruction pointer
on the value-stack and jumps to the matching `}`.

When the execution hits a `?` it jumps to the address given by
the value on top of the stack, and executes the code until it reaches a `}`,
then it jumps back to the address after the `?`.

Each call increases the recursion depth by one, returning from a procedure decreases it.
If the maximum recursion depth (3 by default) is reached `?` will not jump to the given address but simply continue execution at the current position.

Examples:

```
{0:![64(.;10%48+;2+~,1+'10/';)]'.(,#)10#.}0$
```
defines a routine for printing positive integers, that is uses in the following code to print the Fibonacci-numbers
```
1000 :(;'-1'0'(.;+'1).;;>:['0@?])
```

It is possible to use `}` within loop-blocks to define a early return condition


```
{:0<[.0}]}
```
defines a procedure that returns the maximum of zero and the provided argument

## Memory layout

The program runs on a virtual machine with a memory-cell for every signed 64-bit integer
(the cells are allocated in blocks of 4096 bytes when a non-zero value is written to some address within that block).

- The programs source code is stored at the memory addresses from `-1` going downwards (first character at address `-1`, next at `-2` ...)
    when executing the program the instruction pointer is decremented after each instruction
- The call-stack is at address `I64_MIN (-2^63)` growing upwards
- The value-stack is at address `I64_MAX (2^63-1)` growing upwards
- The memory addresses between `0` and `2^62` can be used without impacting the program behavior

## Self modification
The programs source-code and the code-stacks are stored within the program memory and can be modified at run-time

Examples:

The construct
```
0~100(.:@#.1-:@).
```
prints the programs source-code (limited to at most 100 characters), by reading it from the corresponding memory-section

