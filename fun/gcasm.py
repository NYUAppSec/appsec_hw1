#!/usr/bin/env python3

# An assembler for the THX-1138 CPU used in NYU AppSec's gift card reader.
# From giftcard.h:
# programs operate on the message buffer
# note!! it is the gift card producers job to make sure that
# programs in the gift card are VALID
# each "instruction" is 3 bytes
# arithmetic instructions set the zero flag if the result is 0
# opcode (1 byte) arg1 (1 byte) arg2 (1 byte)
# 0x00  no-op (if there is extra space at end of program this should be used)
# 0x01  get current char into register "arg1" (16 registers)
# 0x02  put registar "arg1" into current char
# 0x03  move cursor by "arg1" bytes (can be negative)
# 0x04  put constant "arg1" into register "arg2"
# 0x05  xor register "arg1" with register "arg2" and store result in register "arg1"
# 0x06  add register "arg1" to register "arg2" and store result in register "arg1"
# 0x07  display the current message
# 0x08  end program
# 0x09  jump "arg1" bytes relative to the end of this instruction
# 0x10  if the zero flag is set, jump "arg1" bytes relative to the end of this instruction

import sys
import re
import struct
import argparse
from enum import Enum

ArgTypes = Enum('ArgTypes', 'REG CONST LABEL NONE')

# Opcode definitions: (opcode, arg1_type, arg2_type, num_args)
opcode_defs = {
    "nop":         (0x00, ArgTypes.NONE, ArgTypes.NONE, 0),
    "getch":       (0x01, ArgTypes.REG, ArgTypes.NONE, 1),
    "putch":       (0x02, ArgTypes.REG, ArgTypes.NONE, 1),
    "movcurs":     (0x03, ArgTypes.CONST, ArgTypes.NONE, 1),
    "mov":         (0x04, ArgTypes.CONST, ArgTypes.REG, 2),
    "xor":         (0x05, ArgTypes.REG, ArgTypes.REG, 2),
    "add":         (0x06, ArgTypes.REG, ArgTypes.REG, 2),
    "disp":        (0x07, ArgTypes.NONE, ArgTypes.NONE, 0),
    "end":         (0x08, ArgTypes.NONE, ArgTypes.NONE, 0),
    "jmp":         (0x09, ArgTypes.LABEL, ArgTypes.NONE, 1),
    "jz":          (0x10, ArgTypes.LABEL, ArgTypes.NONE, 1),
}

opcode_bytes = {v[0]: (k,v[1],v[2]) for k, v in opcode_defs.items()}

# Take a byte in the range 0-255 and make it signed
def signed(b):
    return struct.unpack("b", bytes([b]))[0]

# Take a (possibly negative) byte and make it unsigned
def unsigned(b):
    if b < 0:
        return struct.pack("b", b)
    else:
        return struct.pack("B", b)

def format_arg(arg, arg_type, label=None):
    if arg_type == ArgTypes.REG:
        return f"r{arg}"
    elif arg_type == ArgTypes.CONST:
        return f"{arg:#02x}"
    elif arg_type == ArgTypes.LABEL:
        return label if label else f"{arg:#02x}"
    elif arg_type == ArgTypes.NONE:
        return ""
    else:
        raise Exception(f"unknown arg type: {arg_type}")

class Instruction:
    def __init__(self, opcode, arg1, arg2, label=None, filename="unknown", line_num=0):
        self.opcode = opcode
        self.arg1 = arg1
        self.arg2 = arg2
        self.label = label
        self.label_resolved = False if label else True
        self.filename = filename
        self.line_num = line_num

    def __str__(self):
        opspec = opcode_bytes[self.opcode]
        opstr = opspec[0]
        arg1str = format_arg(self.arg1, opspec[1], self.label)
        arg2str = format_arg(self.arg2, opspec[2])
        s = f"{opstr}"
        if arg1str:
            s += f" {arg1str}"
        if arg2str:
            s += f", {arg2str}"
        return s

    def assemble(self):
        if self.label and not self.label_resolved:
            raise Exception("Label not resolved")
        return bytes([self.opcode]) + unsigned(self.arg1) + unsigned(self.arg2)
    
    @staticmethod
    def from_bytes(bytes_):
        opcode = bytes_[0]
        arg1 = bytes_[1]
        arg2 = bytes_[2]
        return Instruction(opcode, arg1, arg2)

def parse(line, filename, line_num):
    """
    Parse a line of assembly code into an Instruction object.

    Example input:
        mov r0, 0x12
    """
    line = line.split(';')[0] # Remove comments
    line = line.strip()
    if not line:
        return None
    parts = [p for p in re.split(' |,', line) if p]
    opcode = parts[0].lower()
    if opcode not in opcode_defs:
        raise Exception(f"unknown opcode: {opcode}")
    opspec = opcode_defs[opcode]
    argtypes = [opspec[1], opspec[2]]
    args = parts[1:]
    if len(args) != opspec[3]:
        raise Exception(f"wrong number of arguments for opcode {opcode} in {filename}:{line_num} "
                         f"(expected {opspec[3]}, got {len(args)})")
    parsed_args = [0, 0]
    label = None
    for i, arg in enumerate(args):
        if argtypes[i] == ArgTypes.REG:
            if arg[0] != 'r':
                raise Exception(f"expected register, got {arg} in {filename}:{line_num}")
            try:
                regnum = int(arg[1:])
                parsed_args[i] = regnum
                if regnum > 15:
                    print(f"WARNING: register number too high: {arg} in {filename}:{line_num}")
            except ValueError:
                raise Exception(f"expected register, got {arg} in {filename}:{line_num}")
        elif argtypes[i] == ArgTypes.CONST:
            parsed_args[i] = int(arg, 16)
        elif argtypes[i] == ArgTypes.LABEL:
            # Allow labels to be numeric or symbolic
            try:
                parsed_args[i] = int(arg, 0)
                if parsed_args[i] % 3 != 0:
                    print (f"Warning: numeric label {arg} in {filename}:{line_num} is not a multiple of 3", file=sys.stderr)
            except ValueError:
                parsed_args[i] = 0 # Will be resolved later
                label = arg
        elif argtypes[i] == ArgTypes.NONE:
            pass
        else:
            raise Exception(f"unknown arg type: {argtypes[i]}")
    return Instruction(opcode_defs[opcode][0], *parsed_args, label, filename, line_num)

def assemble(filename):
    """
    Assemble a file of assembly code into bytes.
    """
    with open(filename, "r") as f:
        lines = f.readlines()
    lines = [l.strip() for l in lines]
    instructions = []
    labelmap = {}
    offset = 0
    for line_num, line in enumerate(lines):
        print(f"{line_num}: {line}")
        label = re.match(r"^(\w+):$", line)
        if label:
            label = label.group(1)
            labelmap[label] = offset
            continue
        instruction = parse(line, filename, line_num)
        if not instruction:
            continue
        instructions.append(instruction)
        offset += 1
    for i, instruction in enumerate(instructions):
        if instruction.label:
            # Jumps are relative to the end of the current instruction
            label_offset = (labelmap[instruction.label] - i - 1) * 3
            instruction.arg1 = label_offset
            instruction.label_resolved = True
    return b"".join([instruction.assemble() for instruction in instructions])

def disas(bytes_):
    # Remove trailing nulls
    bytes_ = bytes_.rstrip(b"\x00")
    # Add padding to make it a multiple of 3
    bytes_ += b"\x00" * (3 - (len(bytes_) % 3))

    instructions = []
    for i in range(0, len(bytes_), 3):
        if i > len(bytes_) - 3:
            break
        inst = Instruction.from_bytes(bytes_[i:i+3])
        instructions.append(inst)
    labelmap = {}
    # Resolve labels
    for i, inst in enumerate(instructions):
        if inst.opcode == 0x09 or inst.opcode == 0x10:
            # jmp or jz
            our_offset = (i+1) * 3
            target = our_offset + signed(inst.arg1)
            target_index = target // 3
            inst.label = f"L{target_index}"
            labelmap[target_index] = inst.label
    for i, inst in enumerate(instructions):
        if i in labelmap:
            print(f"   L{i}:")
        ins_bytes = bytes_[i*3:(i+1)*3]
        print(f"{i*3:02x}:    {ins_bytes[0]:02x} {ins_bytes[1]:02x} {ins_bytes[2]:02x}    {inst}")

test_program = """
START:
jmp START
"""

def main():
    parser = argparse.ArgumentParser(description="Assemble a file of assembly code into bytes.")
    parser.add_argument("filename", type=str, help="assembly file to (dis)assemble")
    parser.add_argument("-o", "--output", type=str, default='a.out', help="output file to write to")
    parser.add_argument("-d", "--disassemble", action="store_true", help="disassemble instead of assemble")
    args = parser.parse_args()
    if args.disassemble:
        with open(args.filename, "rb") as f:
            disas(f.read())
    else:
        assembled_bytes = assemble(args.filename)
        # Gift card programs are 256 bytes long, so pad with NOPs
        assembled_bytes = assembled_bytes.ljust(256, b'\x00')
        if len(assembled_bytes) > 256:
            print(f"WARNING: assembled program is longer than 256 bytes ({len(assembled_bytes)})", file=sys.stderr)
        with open(args.output, "wb") as f:
            f.write(assembled_bytes)

if __name__ == "__main__":
    main()
