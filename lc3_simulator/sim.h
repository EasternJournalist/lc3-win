#ifndef SIM_H_INCLUDED
#define SIM_H_INCLUDED

#include <conio.h>
#include <windows.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <sstream>

typedef unsigned short int word;

class Simulator
{
public:
    enum status_code
    {
        Breakpoint,
        Normal,
        Priviledge_Exception,
        User_Interrupt,
        Exit,
        Select
    };

    const int DSR_ = 0xfe04;
    const int DDR_ = 0xfe06;
    const int KBSR_ = 0xfe00;
    const int KBDR_ = 0xfe02;

    std::map<std::string, word> asm_map;

    word gen_reg[8];                //general purpose register R0-R7
    word PC, MAR, MDR, IR, PSR, Saved_USP, Saved_SSP;          //

    HANDLE stdOutputHandle;
    status_code sim_status;

    int vstart;
    bool vtrackPC;
    int vselect;
    word vjump;
    int HistoryCount;



    std::stringstream message;
    word mem[0x10000];                  //memory x0000-xFFFF. x0000-xFDFF for memory locations, xFE00-xFFFF for device registers.
    bool breakpoints[0x10000];          //is breakpoint
    std::set<word> breakpoints_set;     //set of breakpoints;

    void cursor_xy(int x, int y);
    void text_color(unsigned short int color);

    void initialize();                          //initialize the simulator.

    bool isNOP(word h);                         //check if a 16-bit data is an operation

    bool check_interrupt();                     //check and process an interrupt.
    void process_interrupt(word INTV, word Priority);   //process an interrupt
    void process_instr(word instr);             //process an instruction.
    void device_keyboard();
    void device_monitor();

    void load_os();                             //load the operating system code.
    void load_bin(std::string filename);        //load an .bin file
    void load_obj(std::string filename);        //load an .obj file
    void save_mem(std::string filename, word _start, word _end);

    word slice(word x, short int a, short int b){return ((x%(1<<b))>>a);}
    word sign_extend(word x, int n){return bit(x, n-1)?(((0xffff>>n)<<n) + x):x;}
    word bin_to_word(char str[]);
    word to_word(std::string str);                          // transform number string e.g.  "#102", "x8000"
    std::string word_to_bin(word x);
    std::vector<std::string> instr_to_asm(word h);          //return the asm string of an instruction
    std::string str_reg(word x, int _start, int _end);
    std::string str_imm(word x, int _start, int _end);      //decimal
    std::string str_imm(word x);
    std::string str_fulhex(word);                           //hex
    bool bit(word x, int i){return (x&(1<<i)) != 0;}
    void setcc(word x)
    {
        PSR = PSR&0xfff8;
        if(bit(x, 15))
            PSR += 4;
        else if(x == 0)
            PSR += 2;
        else
            PSR += 1;
    }

    void assembler(std::string filename, std::string ofilename);
    void add_overflow_err(int line, int bitn);
    std::vector<std::string> split(std::string str);
    std::string split_first(std::string& str);
    bool check_number(word x, int n);
    word to_reg(std::string s);
    std::string trim_space(std::string s);

    /////////                           the LC-3 instructions               //////////
    void ADD(int DR, int SR1, int SR2)
    {
        setcc(gen_reg[DR] = gen_reg[SR1] + gen_reg[SR2]);
    }
    void ADDimm(int DR, int SR1, word imm5)
    {
        setcc(gen_reg[DR] = gen_reg[SR1] + sign_extend(imm5, 5));
    }
    void AND(int DR, int SR1, int SR2)
    {
        setcc(gen_reg[DR] = gen_reg[SR1] & gen_reg[SR2]);
    }
    void ANDimm(int DR, int SR1, word imm5)
    {
        setcc(gen_reg[DR] = gen_reg[SR1] & sign_extend(imm5, 5));
    }
    void NOT(int DR, int SR)
    {
        setcc(gen_reg[DR] = ~(gen_reg[SR]));
    }
    void BR(word nzp, word PCoffset9)
    {
        if(nzp&PSR)
            PC += sign_extend(PCoffset9, 9);
    }
    void JMP(int BaseR)
    {
        PC = gen_reg[BaseR];
    }
    void JSR(word PCoffset11)
    {
        gen_reg[7] = PC;
        PC += sign_extend(PCoffset11, 11);
    }
    void JSRR(int BaseR)
    {
        gen_reg[7] = PC;
        PC = gen_reg[BaseR];
    }
    void LD(int DR, word PCoffset9)
    {
        word tar = PC + sign_extend(PCoffset9, 9);
        if(tar == KBDR_)
            mem[KBSR_] = mem[KBSR_] & 0x7fff;
        setcc(gen_reg[DR] = mem[tar]);
    }
    void LDI(int DR, word PCoffset9)
    {
        word tar = mem[PC + sign_extend(PCoffset9, 9)];
        if(tar == KBDR_)
            mem[KBSR_] = mem[KBSR_] & 0x7fff;
        setcc(gen_reg[DR] = mem[tar]);
    }
    void LDR(int DR, int BaseR, word offset6)
    {
        word tar =  gen_reg[BaseR] + sign_extend(offset6, 6);
        if(tar == KBDR_)
            mem[KBSR_] = mem[KBSR_] & 0x7fff;
        setcc(gen_reg[DR] = mem[tar]);
    }
    void LEA(int DR, word PCoffset9)
    {
        setcc(gen_reg[DR] = PC + sign_extend(PCoffset9, 9));
    }
    void RET()
    {
        PC = gen_reg[7];
    }
    void RTI()
    {
        if(!bit(PSR, 15))
        {
            PC = mem[gen_reg[6]];
            gen_reg[6]++;
            PSR = mem[gen_reg[6]];
            gen_reg[6]++;
            if(bit(PSR, 15))
            {
                Saved_SSP  = gen_reg[6];
                gen_reg[6] = Saved_USP;
            }
        }
        else
            sim_status = Priviledge_Exception;
    }
    void ST(int SR, word PCoffset9)
    {
        word tar = PC + sign_extend(PCoffset9, 9);
        if(tar == DDR_)
            mem[DSR_] = mem[DSR_] & 0x7fff;
        mem[tar] = gen_reg[SR];
    }
    void STI(int SR, word PCoffset9)
    {
        word tar = mem[PC + sign_extend(PCoffset9, 9)];
        if(tar == DDR_)
            mem[DSR_] = mem[DSR_] & 0x7fff;
        mem[tar] = gen_reg[SR];
    }
    void STR(int SR, int BaseR, word offset6)
    {
        word tar = gen_reg[BaseR] + sign_extend(offset6, 6);
        if(tar == DDR_)
            mem[DSR_] = mem[DSR_] & 0x7fff;
        mem[gen_reg[BaseR] + sign_extend(offset6, 6)] = gen_reg[SR];
    }
    void TRAP(word trapvect8)
    {
        gen_reg[7] = PC;
        PC = mem[trapvect8];
    }
    ///////////////////////////////////////////////////////////////////////////////////


    /////////                           Commands                           ////////////
    void step_over();                   //
    void run();                         //run the simulator till breakpoint or interrupted by the user
    void run(int i);                    //run i steps or till breakpoint or interrupted by the user
    void set_bk(word loc);              //set breakpoint
    void cancel_bk(word loc);           //cancel breakpoint
    void cancel_all_bk();               //cancel all breakpoints
    void clear_count();                 //clear the count of instructions executed
    void set_value(std::string t, std::string v);   //set the value of t
    bool cmd(char str[]);               //execute a command
    void show_mem(int _start, int _end);        //show part of mem
    void show_mem();        //show part of mem
    void show_message();
    void show_status();
    /////////                           simulator info                      ///////////

private:

};

#endif // SIM_H_INCLUDED
