#!/usr/bin/env bash

# This script runs the gift card reader (original, UBSAN, and ASAN versions) on the gft files found in
# the testcases directory. It is intended to be run from the
# top-level directory of the giftcard project. It can also be
# invoked by running the "make test" command.

# Color support
if [ -t 1 ] && [ -n "$(tput colors)" ]; then
    # stdout is a terminal with colors
    RED="$(tput setaf 1)"
    GREEN="$(tput setaf 2)"
    YELLOW="$(tput setaf 3)"
    BLUE="$(tput setaf 4)"
    MAGENTA="$(tput setaf 5)"
    CYAN="$(tput setaf 6)"
    WHITE="$(tput setaf 7)"
    BOLD="$(tput bold)"
    RESET="$(tput sgr0)"
else
    # stdout is not a terminal or does not support colors
    RED=""
    GREEN=""
    YELLOW=""
    BLUE=""
    MAGENTA=""
    CYAN=""
    WHITE=""
    BOLD=""
    RESET=""
fi

# Check to see if a file exists.
exists() {
    [ -e "$1" ]
}

# Run command with timeout
function timeout() { perl -e 'alarm shift; exec @ARGV' "$@"; }

# Convert the exit status of the last command to a human-readable string.
check_exit_status ()
{
    local status="$1"
    local msg=""
    local signal=""
    if [ ${status} -lt 128 ]; then
        msg="${status}"
    elif [ $((${status} - 128)) -eq $(builtin kill -l SIGALRM) ]; then
        msg="TIMEOUT"
    else
        signal="$(builtin kill -l $((${status} - 128)) 2>/dev/null)"
        if [ "$signal" ]; then
            msg="CRASH:SIG${signal}"
        fi
    fi
    echo "${msg}"
    return 0
}

# Check to make sure that the giftcard executables exist.
for exe in giftcardreader.original giftcardreader.ubsan giftcardreader.asan; do
    if [ ! -x ./$exe ]; then
        echo "Error: $exe executable not found."
        exit 1
    fi
done

VALID=./testcases/valid
INVALID=./testcases/invalid
# If a test case takes longer than this to run, assume it is stuck in an infinite loop.
MAX_RUNTIME=30

# Check to make sure that the testcase directories exist.
if [ ! -d "$VALID" ]; then
    echo "Error: '$VALID' directory not found."
    exit 1
fi
if [ ! -d "$INVALID" ]; then
    echo "Error: '$INVALID' directory not found."
    exit 1
fi

# Run the giftcardreader versions on each of the testcases. Valid gift cards
# should return 0, invalid gift cards should return non-zero. Crashes
# are detected by checking for a return value greater than or equal to
# 128, and are never considered a pass.
# Credit to Clark Cone (NYU Student) for improving the count function
passed=0
failed=0
echo "Running tests on ${BOLD}valid${RESET} gift cards (expected return value: 0)..."
if exists "$VALID"/* ; then
    printf "${BOLD}%-50s %-5s %-20s %s${RESET}\n" "Testcase" "Pass?" "Version" "Exit Status"
    for gft in "$VALID"/* ; do
        [ -f "$gft" ] || break
        test_passed=false
        for exe in giftcardreader.original giftcardreader.ubsan giftcardreader.asan; do
            timeout $MAX_RUNTIME ./$exe 1 "$gft" &> /dev/null
            rv=$?
            if [ $rv -eq 0 ]; then
                pmsg="PASS"
                test_passed=true
                rowcolor=$GREEN
                printf "${rowcolor}%-50s %-5s %-20s %s${RESET}\n" "$(basename "$gft")" "$pmsg" "$exe" "$(check_exit_status $rv)"
                passed=$((passed+1))
                break
            else
                pmsg="FAIL"
                rowcolor=$RED
                printf "${rowcolor}%-50s %-5s %-20s %s${RESET}\n" "$(basename "$gft")" "$pmsg" "$exe" "$(check_exit_status $rv)"
                failed=$((failed+1))
            fi
        done
    done
else
    echo "${YELLOW}Skipped: no testcases found in ${VALID}${RESET}"
fi
echo
echo "Running tests on ${BOLD}invalid${RESET} gift cards (expected return value: nonzero)..."
if exists "$INVALID"/* ; then
    printf "${BOLD}%-50s %-5s %-20s %s${RESET}\n" "Testcase" "Pass?" "Version" "Exit Status"
    for gft in "$INVALID"/* ; do
        [ -f "$gft" ] || break
        test_passed=false
        for exe in giftcardreader.original giftcardreader.ubsan giftcardreader.asan; do
            timeout $MAX_RUNTIME ./$exe 1 "$gft" &> /dev/null
            rv=$?
            if [ $rv -gt 0 ] && [ $rv -lt 128 ]; then
                pmsg="PASS"
                test_passed=true
                rowcolor=$GREEN
                printf "${rowcolor}%-50s %-5s %-20s %s${RESET}\n" "$(basename "$gft")" "$pmsg" "$exe" "$(check_exit_status $rv)"
                passed=$((passed+1))
                break
            else
                pmsg="FAIL"
                rowcolor=$RED
                printf "${rowcolor}%-50s %-5s %-20s %s${RESET}\n" "$(basename "$gft")" "$pmsg" "$exe" "$(check_exit_status $rv)"
                failed=$((failed+1))
            fi
        done
    done
else
    echo "${YELLOW}Skipped: no testcases found in ${INVALID}${RESET}"
fi
echo
echo "TESTING SUMMARY:"
echo "${GREEN}Passed: ${passed}${RESET}"
echo "${RED}Failed: ${failed}${RESET}"
echo "Total:  $(($passed+$failed))"

# Exit with 0 if all tests passed, 1 if any failed.
if [ $failed -eq 0 ]; then
    exit 0
else
    exit 1
fi
