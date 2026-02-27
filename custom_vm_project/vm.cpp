#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <string>
#include <algorithm>

using namespace std;

vector<uint16_t> memory(1<<16,0);
uint16_t R[8]={0};
uint16_t PC=0;
int FLAG_Z=1, FLAG_N=0, FLAG_P=0;

void updateFlags(uint16_t r){
    if(r==0){ FLAG_Z=1; FLAG_N=FLAG_P=0; }
    else if((r>>15)&1){ FLAG_N=1; FLAG_Z=FLAG_P=0; }
    else{ FLAG_P=1; FLAG_Z=FLAG_N=0; }
}

ofstream trace("trace.jsonl");

void resetState(){
    fill(memory.begin(), memory.end(), 0);
    for(int i=0;i<8;i++) R[i]=0;
    PC=0;
    FLAG_Z=1;
    FLAG_N=0;
    FLAG_P=0;
}

void logStep(int step,uint16_t pc,uint16_t ins){
    trace<<"{\"step\":"<<step<<",\"pc\":"<<pc<<",\"ins\":"<<ins<<",\"regs\":[";
    for(int i=0;i<8;i++){
        trace<<R[i]; if(i<7) trace<<",";
    }
    trace<<"]}\n";
}

void run(){
    int step=0;
    while(true){
        uint16_t pc_before=PC;
        uint16_t ins=memory[PC++];
        logStep(step++,pc_before,ins);
        int op=(ins>>12)&0xF;

        if(op==0) break;

        int rd=(ins>>9)&7;
        int rs=(ins>>6)&7;
        int imm6=ins&0x3F;

        switch(op){
            case 1: R[rd]+=R[rs]; updateFlags(R[rd]); break;
            case 2: R[rd]&=R[rs]; updateFlags(R[rd]); break;
            case 3: R[rd]=~R[rs]; updateFlags(R[rd]); break;
            case 4: if(R[rd]==0) PC+=imm6; break;
            case 5: PC=R[rd]; break;
            case 6: R[rd]=imm6; updateFlags(R[rd]); break;
            // CHANGE: Multiply register by register for dynamic Factorial
            case 7: R[rd]=R[rd]*R[rs]; updateFlags(R[rd]); break; 
            // CHANGE: Added SUB to allow for counters/loops
            case 8: R[rd]-=imm6; updateFlags(R[rd]); break; 
        }
    }
    trace.close();
    cout<<"Trace written\n";
}

void loadFact(int n){
    resetState();
    n = max(0, min(n, 63));
    memory[0]=(6<<12)|(0<<9)|1;      // LOAD R0, 1 (Accumulator)
    memory[1]=(6<<12)|(1<<9)|n;      // LOAD R1, n (Counter)
    memory[2]=(6<<12)|(2<<9)|3;      // LOAD R2, 3 (Loop Address)
    memory[3]=(7<<12)|(0<<9)|(1<<6); // MUL R0, R1 (R0 = R0 * R1)
    memory[4]=(8<<12)|(1<<9)|1;      // SUB R1, 1  (Decrement)
    memory[5]=(4<<12)|(1<<9)|1;      // JZ R1, skip 1 (Exit if R1 == 0)
    memory[6]=(5<<12)|(2<<9);        // JMP R2 (Back to MUL)
    memory[7]=0;                     // HALT
}

void loadFib(int steps){
    resetState();
    steps = max(0, min(steps, 63));
    memory[0]=(6<<12)|(0<<9)|0;      // R0 = 0 (Start)
    memory[1]=(6<<12)|(1<<9)|1;      // R1 = 1 (Start)
    memory[2]=(6<<12)|(4<<9)|steps;  // R4 = steps (Loop counter)
    memory[3]=(6<<12)|(5<<9)|4;      // R5 = 4 (Loop Address)
    // LOOP START
    memory[4]=(6<<12)|(2<<9)|0;      // R2 = 0
    memory[5]=(1<<12)|(2<<9)|(0<<6); // R2 += R0
    memory[6]=(1<<12)|(2<<9)|(1<<6); // R2 += R1 (R2 is next Fib)
    memory[7]=(6<<12)|(0<<9)|0;      // Reset R0
    memory[8]=(1<<12)|(0<<9)|(1<<6); // R0 = R1 (Shift)
    memory[9]=(6<<12)|(1<<9)|0;      // Reset R1
    memory[10]=(1<<12)|(1<<9)|(2<<6);// R1 = R2 (Shift)
    memory[11]=(8<<12)|(4<<9)|1;     // R4 -= 1
    memory[12]=(4<<12)|(4<<9)|1;     // JZ R4, Exit
    memory[13]=(5<<12)|(5<<9);       // JMP R5
    memory[14]=0;                    // HALT
}

int main(int argc,char** argv){
    if(argc<3){
        cout<<"Usage: vm fact|fib <number>\n";
        return 0;
    }
    string p=argv[1];
    int val = 0;
    try {
        val = stoi(argv[2]);
    } catch(...) {
        cout<<"Invalid number: "<<argv[2]<<"\n";
        return 1;
    }
    if(p=="fact") loadFact(val);
    else if(p=="fib") loadFib(val);
    else {
        cout<<"Unknown program: "<<p<<"\n";
        cout<<"Usage: vm fact|fib <number>\n";
        return 1;
    }
    run();
}