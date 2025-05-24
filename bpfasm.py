#!/usr/bin/env python
##############################################################################
#
# bpfasm.py
#
# little berkeley packet filter asm -> C struct bpf_insn array converter
#
# Copyright (c) 2011-2021 Jan Seiffert
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# As long as you retain this notice you can do whatever you want with this
# stuff. If we meet some day, and you think this stuff is worth it, you can
# buy me a beer in return.
# ----------------------------------------------------------------------------
#
# $Id:$
#

import sys
from optparse import OptionParser

##############################################################################
# Helper to get an enum
#
if sys.version_info < (2, 4):
	from sets import Set
	class Enum(Set):
		def __getattr__(self, name):
			if name in self:
				return name
			raise AttributeError
else :
	class Enum(set):
		def __getattr__(self, name):
			if name in self:
				return name
			raise AttributeError

##############################################################################
# parser for different instruction forms
#
def parse_aa(args, instr):
	tstr = args[0]
	offset = 0
	if tstr.startswith('net'):
		tstr = tstr[3:]
		offset = -0x100000
	if tstr.startswith('ll'):
		tstr = tstr[2:]
		offset = -0x200000
	if tstr[0] != '[' or tstr[-1] != ']':
		return (False, instr)
	number = tstr[1:-1]
	if number[0].lower() == 'x':
		return (False, instr)
	if offset != 0:
		number = str(offset) + '+(' + number + ')'
	instr[-1] = number
	instr.append("aa")
	return (True, instr)

def parse_ai(args, instr):
	ok, instr = parse_aa(args, instr)
	if not ok:
		return (False, instr)
	instr[-1] = "ai"
	return (True, instr)

def parse_anc(args, instr):
	if args[0][0] != '#':
		return (False, instr)
	number = args[0][1:]
	if number not in anc:
		return (False, instr)
	instr[-1] = anc[number]
	instr.append("aa")
	return (True, instr)

def parse_ax(args, instr):
	tstr = args[0]
	offset = 0
	number = 0
	if tstr.startswith('net'):
		tstr = tstr[3:]
		offset = -0x100000
	if tstr.startswith('ll'):
		tstr = tstr[2:]
		offset = -0x200000
	if tstr[0] != '[' or tstr[-1] != ']':
		return (False, instr)
	number = tstr[1:-1]
	if number[0].lower() != 'x':
		return (False, instr)
	pos = number.find('+')
	if pos < 1:
		number = 0
	else :
		number = number[pos+1:].strip()
	if offset != 0:
		number = str(offset) + '+(' + number + ')'
	instr[-1] = number
	instr.append("ax")
	return (True, instr)

def parse_a(args, instr):
	if len(args[0]) > 1 or args[0][0].lower() != 'a':
		return (False, instr)
	instr[-1] = 0
	instr.append("a")
	return (True, instr)

def parse_c(args, instr):
	if args[0][0] != '#':
		return (False, instr)
	number = args[0][1:]
	if number == "len":
		return (False, instr)
	if number in anc:
		return (False, instr)
	instr[-1] = number
	instr.append("c")
	return (True, instr)

def parse_e(args, instr):
	ok, instr = parse_c(args, instr)
	if not ok:
		return (False, instr)
	instr[-1] = args[1].strip()
	instr.append(args[2].strip())
	instr.append("e")
	return (True, instr)

def parse_ex(args, instr):
	if args[0][0].lower() != 'x':
		return (False, instr)
	instr[-1] = 0
	instr.append(args[1].strip())
	instr.append(args[2].strip())
	instr.append("ex")
	return (True, instr)

def parse_i(args, instr):
	if args[0][0] != '#':
		return (False, instr)
	number = args[0][1:]
	if number == "len":
		return (False, instr)
	if number in anc:
		return (False, instr)
	instr[-1] = number
	instr.append("i")
	return (True, instr)

def parse_l(args, instr):
	if args[0][0] != '#':
		return (False, instr)
	number = args[0][1:]
	if number in anc:
		return (False, instr)
	if number != "len":
		return (False, instr)
	instr[-1] = 0
	instr.append("l")
	return (True, instr)

def parse_L(args, instr):
	if args[0][0] == '#':
		if args[0][1:] != next:
			return (False, instr)
	if args[0][0] == '[':
		return (False, instr)
	instr[-1] = args[0].strip()
	instr.append(0)
	instr.append(0)
	instr.append("L")
	return (True, instr)

def parse_m(args, instr):
	if args[0][0].lower() != 'm' or args[0][1] != '[' or args[0][-1] != ']':
		return (False, instr)
	number = args[0][2:-1]
	instr[-1] = number
	instr.append("m")
	return (True, instr)

def parse_n(args, instr):
	print("nibble load mode needs to be implemented")
	return (False, instr)

def parse_t(args, instr):
	instr[-1] = 0
	instr.append("t")
	return (True, instr)

def parse_x(args, instr):
	if len(args[0]) > 1 or args[0][0].lower() != 'x':
		return (False, instr)
	instr[-1] = 0
	instr.append("x")
	return (True, instr)

##############################################################################
# Instruction type and foo enums
#
I_T  = Enum(['LD', 'LDX', 'ST', 'STX', 'ALU', 'JMP', 'RET', 'MISC'])
I_S  = Enum(['N', 'W', 'H', 'B'])
I_O  = Enum(['N', 'ADD', 'SUB', 'MUL', 'DIV', 'OR', 'AND', 'LSH', 'RSH', 'NEG', 'JA', 'JEQ', 'JGT', 'JGE', 'JSET', 'TAX', 'TXA'])
I_SO = Enum(['N', 'A', 'K', 'X', 'LEN', 'MEM', 'ABS', 'MSH', 'IND', 'IMM'])

anc = dict({\
		'protocol':    -0x1000 +  0, \
		'pkttype':     -0x1000 +  4, \
		'ifindex':     -0x1000 +  8, \
		'nlattr':      -0x1000 + 12, \
		'nlattr_nest': -0x1000 + 16, \
		'mark':        -0x1000 + 20, \
		'queue':       -0x1000 + 24, \
		'hatype':      -0x1000 + 28, \
		'rxhash':      -0x1000 + 32, \
		'cpu':         -0x1000 + 36, \
		})

##############################################################################
# instruction tables
#
forms = dict({\
		'aa':  (1, I_SO.ABS, parse_aa), \
		'ai':  (1, I_SO.IMM, parse_ai), \
		'anc': (1, I_SO.ABS, parse_anc), \
		'ax':  (1, I_SO.IND, parse_ax), \
		'a':   (1, I_SO.A, parse_a), \
		'c':   (1, I_SO.K, parse_c), \
		'e':   (3, I_SO.K, parse_e), \
		'ex':  (3, I_SO.X, parse_ex), \
		'i':   (1, I_SO.IMM, parse_i), \
		'l':   (1, I_SO.LEN, parse_l), \
		'L':   (1, I_SO.N, parse_L), \
		'm':   (1, I_SO.MEM, parse_m), \
		'n':   (1, I_SO.MSH, parse_n), \
		't':   (0, I_SO.N, parse_t), \
		'x':   (1, I_SO.X, parse_x) \
		})

instructions = dict({\
		'ldb':  [I_T.LD,  I_S.B, I_O.N,    "anc,aa,ax"], \
		'ldh':  [I_T.LD,  I_S.H, I_O.N,    "anc,aa,ax"], \
		'ld':   [I_T.LD,  I_S.W, I_O.N,    "i,l,anc,m,aa,ax"], \
		'ldx':  [I_T.LDX, I_S.W, I_O.N,    "i,l,m,n"], \
		'leax': [I_T.LDX, I_S.W, I_O.N,    "ai"], \
		'stx':  [I_T.STX, I_S.N, I_O.N,    "m"], \
		'st':   [I_T.ST,  I_S.N, I_O.N,    "m"], \
		'jmp':  [I_T.JMP, I_S.N, I_O.JA,   "L"], \
		'jeq':  [I_T.JMP, I_S.N, I_O.JEQ,  "e,ex"], \
		'jgt':  [I_T.JMP, I_S.N, I_O.JGT,  "e,ex"], \
		'jge':  [I_T.JMP, I_S.N, I_O.JGE,  "e,ex"], \
		'jset': [I_T.JMP, I_S.N, I_O.JSET, "e,ex"], \
		'add':  [I_T.ALU, I_S.N, I_O.ADD,  "c,x"], \
		'sub':  [I_T.ALU, I_S.N, I_O.SUB,  "c,x"], \
		'mul':  [I_T.ALU, I_S.N, I_O.MUL,  "c,x"], \
		'div':  [I_T.ALU, I_S.N, I_O.DIV,  "c,x"], \
		'and':  [I_T.ALU, I_S.N, I_O.AND,  "c,x"], \
		'or':   [I_T.ALU, I_S.N, I_O.OR,   "c,x"], \
		'lsh':  [I_T.ALU, I_S.N, I_O.LSH,  "c,x"], \
		'rsh':  [I_T.ALU, I_S.N, I_O.RSH,  "c,x"], \
		'neg':  [I_T.ALU, I_S.N, I_O.NEG,  "t"], \
		'ret':  [I_T.RET, I_S.N, I_O.N,    "c,a,x"], \
		'tax':  [I_T.MISC, I_S.N, I_O.TAX, "t"], \
		'txa':  [I_T.MISC, I_S.N, I_O.TXA, "t"] \
		})


##############################################################################
# Start of foo
#

ousage = "usage: %prog [options] <INFILE>"
oparser = OptionParser(usage=ousage)
oparser.add_option("-o", "--outfile", dest="out_file", help="Target file name", default="-")
oparser.add_option("-c", "--const", action="store_true", dest="make_const", help="make the output structure const")
oparser.add_option("-s", "--static", action="store_true", dest="make_static", help="make the output structure static")

(options, args) = oparser.parse_args()

in_file_name = "-"
if len(sys.argv) > 1:
	for i in sys.argv[1:]:
		in_file_name = i

in_file = None
if in_file_name == "-":
	in_file = sys.stdin
else :
	in_file = open(in_file_name, 'r')

out_file = None
if options.out_file == "-":
	out_file = sys.stdout
else :
	out_file = open(options.out_file, 'w')

# init vars
line_num = 0
inst_num = 0
preprocs = dict()
labels = dict()
inst_list = list()

# for every line
for line in in_file:
	# strip line comments
	line = line[:line.find(';')].strip()
	line_num += 1
	if len(line) < 1:
		continue
	# preproc?
	if line[0] == '#':
		#check for defines
		if line[1:].lstrip().startswith("define"):
			x = line.partition("define")[2].split()
			if x[0][0].isdigit():
				print >> sys.stderr, "line {0}: \"{1}\" is not a legal preprocessor constant".format(line_num, x[0])
				continue
			preprocs[x[0]] = x[1]
		# check for undefines
		if line[1:].lstrip().startswith("undefine"):
			x = line.partition("undefine")[2].split()
			if x[0][0].isdigit():
				print >> sys.stderr, "line {0}: \"{1}\" is not a legal preprocessor constant".format(line_num, x[0])
				continue
			del preprocs[x[0]]
		# handle as comment
		continue
	# look at line
	tok = line.split(None, 1)
	inst_txt = tok[0].lower()
	if not inst_txt in instructions:
		t_end = tok[0].find(':')
		if t_end < 1:
			print >> sys.stderr, "line {0}: \"{1}\" is not a label and i don't know it".format(line_num, tok[0])
			continue
		label_str = tok[0][:t_end]
		if len(label_str) < 1:
			print >> sys.stderr, "line {0}: empty label".format(line_num)
			continue
		labels[label_str] = inst_num
		continue
	# ok, it seems to be an instruction
	found_instr = instructions[inst_txt]
	allowed_forms = found_instr[-1].split(',')
	if len(tok) > 1:
		args = [x.strip() for x in tok[1].split(',')]
	else :
		args = list()
	# copy the instruction we found
	instr = found_instr[:]
	# check which form it is in
	for form in allowed_forms:
		# right amount of args?
		if len(args) != forms[form][0]:
			continue
		# test parser against args
		ok, instr = forms[form][2](args, instr)
		if ok:
			# looks like we found it, add to list
			inst_list.append(instr)
			break
	else :
		# nothing matched, complain
		if len(args) != forms[form][0]:
			print >> sys.stderr, "line {0}: instruction \"{1}\" only takes {2} operands".format(line_num, tok[0], forms[form][0])
		else :
			print >> sys.stderr, "line {0}: operands do not match for instruction \"{1}\"".format(line_num, tok[0])
		continue
	inst_num += 1

#print line_num
#print inst_num
#print preprocs
#print labels
inst_num = 0

# start output by header
out_str  = "/*\n * This file is generated automatically, do not edit\n *\n * for license information refer to the original file \""
out_str += in_file_name
out_str += "\"\n */\n\n"
if options.make_static:
	out_str += "static "
if options.make_const:
	out_str += "const "
out_str += "struct bpf_insn "
if in_file_name == "-":
	out_str += "isns"
else :
	if in_file_name.endswith(".bpf"):
		in_file_name = in_file_name[:-4]
	in_file_name.replace(".", "_")
	if len(in_file_name) == 0:
		in_file_name = "isns"
	out_str += in_file_name
out_str += "[] = {"
print (out_str, file=out_file)

# for every instruction we collected
for instr in inst_list:
	out_str = "\t/* " + str(inst_num).rjust(3) + " */ BPF_"
	if instr[0] != I_T.JMP:
		out_str += "STMT"
	else :
		out_str += "JUMP"
	out_str += "(BPF_"
	out_str += str(instr[0])
	if instr[1] != I_S.N:
		out_str += "|BPF_" + str(instr[1])
	if instr[2] != I_O.N:
		out_str += "|BPF_" + str(instr[2])
	if forms[instr[-1]][1] != I_SO.N:
		out_str += "|BPF_" + str(forms[instr[-1]][1])
	out_str += ", "
	if forms[instr[-1]][0] != 0:
		if instr[0] == I_T.JMP:
			if instr[2] == I_O.JA:
				if instr[3] == "#next":
					out_str += ", 0"
				else :
					if not instr[3] in labels:
						print >> sys.stderr, "label \"{0}\" is not known".format(instr[3])
						continue
					displ = labels[instr[3]] - inst_num + 1
					if displ < 0:
						print >> sys.stderr, "cannot jump backwards!"
#					if displ >= :
#						print >> sys.stderr, "displacement of {} to large for jump".format(displ)
					out_str += str(labels[instr[3]]) + " - " + str(inst_num + 1)
				out_str += ", 0, 0"
			else :
				out_str += str(instr[3]) + ", "
				if instr[4] == "#next":
					out_str += "0"
				else :
					if not instr[4] in labels:
						print >> sys.stderr, "label \"{0}\" is not known".format(instr[4])
						continue
					displ = labels[instr[4]] - inst_num + 1
					if displ < 0:
						print >> sys.stderr, "cannot jump backwards!"
					if displ >= 256:
						print >> sys.stderr, "displacement of {0} to large for jump".format(displ)
					out_str += str(labels[instr[4]]) + " - " + str(inst_num + 1)
				out_str += ", "
				if instr[5] == "#next":
					out_str += "0"
				else :
					if not instr[5] in labels:
						print >> sys.stderr, "label \"{0}\" is not known".format(instr[5])
						continue
					displ = labels[instr[5]] - inst_num + 1
					if displ < 0:
						print >> sys.stderr, "cannot jump backwards!"
					if displ >= 256:
						print >> sys.stderr, "displacement of {0} to large for jump".format(displ)
					out_str += str(labels[instr[5]]) + " - " + str(inst_num + 1)
		else :
			out_str += str(instr[3])
	else :
		out_str += "0"
	out_str += "),"
	inst_num += 1
	print(out_str, file=out_file)

# end output
print("};", file=out_file)
out_file.flush()

