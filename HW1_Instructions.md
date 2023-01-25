# Homework 1: Beware of Geeks Bearing Gift Cards

## Introduction

You've been handed the unpleasant task of performing an audit of code
written by your company's least-favorite contractor, Shoddycorp's
Cut-Rate Contracting. The program is designed to read in gift card files
in a legacy binary file format, and then display them in either a simple
text output format, or in JSON format. Unfortunately, Shoddycorp isn't
returning your calls, so you'll have to make do with the comments in the
file and your own testing.

Justin Cappos (JAC) and Brendan Dolan-Gavitt (BDG) have read through the
code already. It's a mess. We've tried to annotate a few places that
look suspicious, but there's no doubt that there are many bugs lurking
in this code. Make no mistake, this is *not* an example of C code that
you want to imitate.

## Part 1: Setting up Your Environment

In order to complete this assignment, you are required to use the git
VCS. Before beginning to write your code, you should first install git
and clone the repository from GitHub classroom. The git binaries can
be installed by your local package manager or at
https://git-scm.com/downloads. For a cheat-sheet of git commands, please
see https://github.com/nyutandononline/CLI-Cheat-Sheet/blob/master/git-commands.md.
Although we will not be checking your commit messages or git log, it is
recommended that you write descriptive commit messages so that the
evolution of your repository is easy to understand. For a guide to
writing good commit messages, please read
https://chris.beams.io/posts/git-commit/ and the [Linux kernel's advice
on writing good commit messages](https://git.kernel.org/pub/scm/git/git.git/tree/Documentation/SubmittingPatches?id=b23dac905bde28da47543484320db16312c87551#n134).

After git is installed, you will want to configure your git user to sign
your commits. This can be done by following the guide at
https://git-scm.com/book/en/v2/Git-Tools-Signing-Your-Work. You will also
need to [add your GPG public key to your GitHub profile](https://github.com/settings/keys),
and make sure that the email address set in your GitHub account matches
the one you specified when generating your keys. You can find more information
about about this in [GitHub's documentation on commit signature verification](https://docs.github.com/en/authentication/managing-commit-signature-verification/about-commit-signature-verification). To avoid having to
type in your password all the time you, may also want to [set up SSH key
access to your GitHub account](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/about-ssh).

After accepting the invitation on GitHub Classroom, you can clone the
assignment repository by clicking the green "Code" button, copying the
repository URL under "Clone", and then running:

```
git clone <your_repository_url>
```

Note that if you have set up an SSH key, you will want to make sure you
copy the SSH URL (which looks like `git@github.com:NYUAppSec/...`).

The next step is to set up Github Actions to automatically build and test
your code when you push a commit. You can find a [tutorial on GitHub Actions here](https://docs.github.com/en/actions/learn-github-actions/introduction-to-github-actions).

For now, you should set up GitHub Actions to just run

```
echo "Hello world"
```

whenever a new commit is pushed to the repository. To do this, you'll create a
file named `.github/workflows/hello.yml`. Check that the Action is running
correctly in the GitHub interface.

## Part 2: Auditing and Test Cases

Read through the `giftcardreader.c` program (and its accompanying
header, `giftcard.h`) to get a feel for it. You should also try building
and running it with the included `examplefile.gft` file, to see what its
normal output is. You may find it helpful to use a debugger like `gdb`
or `lldb` to step through the program as it executes, to get an understanding of
its normal flow of control.

There is also a `Makefile` included in the repository. You can use this
to build the program by typing `make`. You can also use it to run the
(very minimal and incomplete) test suite for the program by typing
`make test`, which uses `runtests.sh` to run the gift card reader on the
gift cards found in `testcases/valid` and `testcases/invalid`:

```
$ make test
$ ./runtests.sh
Running tests on valid gift cards (expected return value: 0)...
Testcase                                           Pass? Exit Status
examplefile.gft                                    PASS  0

Running tests on invalid gift cards (expected return value: nonzero)...
Testcase                                           Pass? Exit Status
badtype.gft                                        PASS  1

TESTING SUMMARY:
Passed: 2
Failed: 0
Total:  2
```

For this part, your job will be to find some flaws in the program, and
then create test cases (i.e., binary gift cards) that expose flaws in
the program. You should write:

1. *Two* test cases, `crash1.gft` and `crash2.gft`, that cause the
   program to crash; each crash should have a different root cause.
2. One test case, `hang.gft`, that causes the program to loop
   infinitely. (Hint: you may want to examine the "animation" record type
   to find a bug that causes the program to loop infinitely.)
3. A text file, `part2.txt` explaining the bug triggered by each of your
   three test cases.

To create your own test files, you may want to refer to the `gengift.py`
and `genanim.py` programs, which are Python scripts that create gift
card files of different types.

Finally, fix the bugs that are triggered by your test cases, and verify
that the program no longer crashes / hangs on your test cases. To make
sure that these bugs don't come up again as the code evolves, have
Github Actions automatically build and run the program on your test suite.
You can do this by placing the new test cases in the `testcases/valid` or
`testcases/invalid` directories (depending on whether the gift card reader
should accept or reject them). Then have GitHub Actions run `make test`. Note
that you do *not* need to run your tests on the unfixed version of the
code---the tests are intended to verify that the code is fixed and prevent
the bugs from being reintroduced in later versions (known as *regression
tests*).

## Part 3: Fuzzing and Coverage

As discussed in class, an important part of understanding how well your
test suite exercises your program's behaviors is *coverage*. To start
off, measure the coverage that your program achieves with the test cases
you created in Part 2. To do this, you should build `giftcardreader`
with the `--coverage` option to `gcc`, run your test suite, and then
produce a coverage report using `lcov` (details on how to do this can be
found in the lecture slides).

You should notice that there are portions of the program that are
*uncovered* (i.e., the code was not executed while processing your test
suite). Pick two lines of code from the program that are currently
not covered and create test cases (`cov1.gft` and `cov2.gft`) that cover
them. You should add these test cases to your test suite by placing them
in the `testcases/valid` or `testcases/invalid` directory, as appropriate.

An easy and effective way of finding crashes and getting higher coverage
in a program is to *fuzz* it with a fuzzer like AFL++. Fuzz the program
using AFL++, following the [quick-start
instructions](https://github.com/AFLplusplus/AFLplusplus#quick-start-fuzzing-with-afl). To
make the fuzzing more effective, you should provide AFL
with all of the test files you have created in its input directory. Let
the fuzzer run for at least two hours, and then examine the test cases
(in the `queue` directory) and crashes/hangs (in the `crashes` and
`hangs` directories).

Run the gift card reader on the test cases in the `queue` directory. You
can do this with a for loop like this:

```
for f in output/queue/id*; do ./giftcardreader 1 "$f"; done
```

And then produce a new coverage report. You should see that the tests
generated by the fuzzer reach more parts of the gift card program.

Finally, pick two crashes/hangs and fix the bugs in the program that
cause them. You should include these test cases in the tests you run
in GitHub Actions (as `fuzzer1.gft` and `fuzzer2.gft`).

To complete the assignment, commit your updated code, your handmade
tests (`cov1.gft` and `cov2.gft`), the fuzzer-generated tests (`fuzzer1.gft`
and `fuzzer2.gft`), and a brief writeup explaining the bugs you found
and fixed in this part (`part3.txt`). You do not need to commit all the test
cases generated by the fuzzer, or the coverage reports.

## Hints

1. What counts as two different bugs? A general rule of thumb is that
   if you can fix one of them without fixing the other, then they will
   be counted as distinct bugs.
2. Some crashes may not occur consistently every time you run the program,
   or may not occur when you run the program in a different environment or
   with different compile flags. One way to make a crash more reproducible
   is to use Address Sanitizer (ASAN), which we will cover in class. The
   `Makefile` also includes a target that will build the gift card reader
   using ASAN, which you can invoke with `make asan`.
3. When fixing a crash, you should try to understand what the root cause is.
   You will probably find it helpful to look at the address sanitizer output,
   which will usually tell you exactly what line of the program is accessing
   invalid memory. You may also want to try using the `gdb` or `lldb` debuggers;
   guides and tutorials can be found online. Your IDE (if you use one) may also
   provide a built-in debugger.
4. The gift card reader does *not* need to attempt to parse or "fix" invalid gift
   card files; you can simply reject these by printing an error and exiting with
   a non-zero exit code (e.g., `exit(1)`).

## Grading

Total points: 100

Part 1 is worth 20 points:

* 10 points for signed commits
* 10 points for GitHub Actions configuration

Part 2 is worth 40 points:

* 15 points for your test cases and fixes
* 15 points for the bug writeup
* 10 points for GitHub Actions regression testing

Part 3 is worth 40 points:

* 10 points for handwritten tests
* 10 points for fuzzer-generated tests
* 10 points for your code fixes
* 10 points for writeup

## What to Submit

Your repository should contain:

* Part 1
  * Your `.github/workflows/hello.yml`
  * At least one signed commit
* Part 2
  * In `testcases/invalid`: `crash1.gft`, `crash2.gft`, and `hang.gft`.
  * A text file named `part2.txt` that contains the bug descriptions
    for each of the three test cases
  * A GitHub Actions YML that runs your tests
  * A commit with the fixed version of the code (if you like, this
    commit can also contain the files mentioned above)
* Part 3
  * In `testcases/invalid` or `testcases/valid`: `cov1.gft`, `cov2.gft`,
    `fuzzer1.gft`, and `fuzzer2.gft`
  * A text file named `part3.txt` that contains your writeup
  * An updated Actions YML that runs the new tests
  * A commit with the fixed version of the code (if you like, this
    commit can also contain the files mentioned above)

**Each part must be committed by its deadline.**

## Concluding Remarks

Despite the fixes you've made, there are almost certainly still many
bugs lurking in the program. Although it is possible to get to a secure
program by repeatedly finding and fixing bugs (at least when the program
is this small), it's a lot of work, and just because a fuzzer stops
finding bugs doesn't mean that the program is bug-free!

Realistically, this program is probably unsalvageable in its current
state. It would be better in this case to rewrite it from scratch,
either in C using a very defensive programming style, or in a safer
language like Python or Rust. In the "clean" directory, you can find
a cleanly-written version of the program (written in C) that
should be relatively bug-free [1]. You'll notice that it's a lot more
verbose, and checks for many more errors than the buggy
version---writing safe C code is difficult!

[1] Although you are encouraged to try to prove us wrong by finding bugs
    in it!
