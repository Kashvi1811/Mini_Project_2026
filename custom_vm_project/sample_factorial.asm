// Factorial of 5 -> result in R0
LOAD R0, 1
LOAD R1, 5
LOAD R2, 3
MUL R0, R1
SUB R1, 1
JZ R1, 1
JMP R2
HALT
