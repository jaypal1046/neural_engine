# v8: "The Block Sorter" (BWT)

**v8 (`--best`)** is the absolute workhorse of this compression engine. It runs incredibly fast, uses very little memory, and provides world-class compression ratios specifically for text, source code, and log files. 

## How It Works (The "Alphabetizer" Trick)
Unlike LZ77 (which looks for copy-paste patterns) or v10 (which uses AI to guess bits), v8 completely scrambles the file into a new shape that is mathematically easier to compress. 

It does this in a 4-step assembly line:

### 1. Burrows-Wheeler Transform (BWT) — "The Sorter"
Imagine you have the word `BANANA`.
The BWT creates a list of every possible rotation of the word:
- `BANANA`
- `ANANAB`
- `NANABA`
- ...

Then, it **alphabetizes** that list. Finally, it takes the very last letter of every sorted word and puts them together: `BNNNAA`. 

Notice what happened? All the `N`s clumped together, and all the `A`s clumped together! The BWT magically reorganizes data so that identical characters sit right next to each other.

### 2. Move-To-Front (MTF) — "The Recent List"
Now that identical letters are clumped together, we run a "Move-To-Front" transform. 
Imagine a stack of books. Whenever you read a book, you put it back on the *top* of the stack.
If you read the same book 5 times in a row, its position is always `0` (the top).
MTF changes our letters into numbers. Because BWT clumped the letters together, MTF turns the file into a massive list of zeroes: `0 0 0 0 0 0 0...`

### 3. Run-Length Encoding (RLE) — "The Zipper"
A file full of zeroes is easy to compress. RLE simply looks at the zeroes and says:
*"Instead of writing 1,000 zeroes, I'll just write `[Zero x 1000]`."*

### 4. rANS Entropy Coding — "The Packer"
Finally, whatever is left over is squeezed down into fractional bits using our `rANS` (Asymmetric Numeral Systems) coder. It also uses an **Order-2 Context** (it looks at the last 2 characters to help compress the current one).

## The Cost (Why use v8?)
- **Memory**: Very low. It splits files into 16 MB chunks, sorts them using a blazing fast Radix Sort, and streams them to the disk. 
- **Speed**: Very fast (thanks to SIMD vectorization and C++ optimization).
- **Ratio**: Perfect for text. It consistently saves 60-70% on large datasets.

**Summary**: v8 is the "Daily Driver". It is the best balance of speed and sheer compression power. Use it as your default for backing up project folders and code!
