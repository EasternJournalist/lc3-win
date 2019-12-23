#include "sim.h"
#include <cstdio>
#include <sstream>
#include <windows.h>
#include <iostream>
#include <fstream>

void Simulator::initialize()
{
    memset(gen_reg, 0, sizeof(word)*8);
    memset(mem, 0, sizeof(word)*0x10000);
    memset(breakpoints, 0, sizeof(bool)*0x10000);

    sim_status = Normal;

    PC = 0x3000;
    PSR = 0x8002;
    IR = 0;
    HistoryCount = 0;

    stdOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    load_os();

    Saved_SSP = 0x1000;

    vtrackPC = true;

    asm_map["BR"] = 30;
    asm_map["BRp"] = 31;
    asm_map["BRz"] = 32;
    asm_map["BRzp"] = 33;
    asm_map["BRn"] = 34;
    asm_map["BRnp"] = 35;
    asm_map["BRnz"] = 36;
    asm_map["BRnzp"] = 37;
    asm_map["ADD"] = 1;
    asm_map["LD"] = 2;
    asm_map["ST"] = 3;
    asm_map["JSR"] = 4;
    asm_map["JSRR"] = 40;
    asm_map["AND"] = 5;
    asm_map["LDR"] = 6;
    asm_map["STR"] = 7;
    asm_map["RTI"] = 8;
    asm_map["NOT"] = 9;
    asm_map["LDI"] = 10;
    asm_map["STI"] = 11;
    asm_map["RET"] = 12;
    asm_map["LEA"] = 14;
    asm_map["TRAP"] = 15;

    asm_map[".FILL"] = 20;
    asm_map[".BLKW"] = 21;
    asm_map[".STRINGZ"] = 22;
    asm_map[".ORIG"] = 23;
    asm_map[".END"] = 24;
    asm_map["~SKIP"] = 25;
}

void Simulator::load_os()
{
    load_bin("lc3sys_mem.bin");
}
void Simulator::process_instr(word instr)
{
    switch(instr>>12)
    {
    case 0:         //0000 BR
        BR(slice(instr, 9, 12), slice(instr, 0, 9));
        break;
    case 1:         //0001 ADD
        if(32&instr)
            ADDimm(slice(instr, 9, 12), slice(instr, 6, 9), slice(instr, 0, 5));
        else
            ADD(slice(instr, 9, 12), slice(instr, 6, 9), slice(instr, 0, 3));
        break;
    case 2:         //0010 LD
        LD(slice(instr, 9, 12), slice(instr, 0, 9));
        break;
    case 3:         //0011 ST
        ST(slice(instr, 9, 12), slice(instr, 0, 9));
        break;
    case 4:         //0100 JSR/JSRR
        if(bit(instr, 11))
            JSR(slice(instr, 0, 11));
        else
            JSRR(slice(instr, 6, 9));
        break;
    case 5:         //0101 AND
        if(bit(instr, 5))
            ANDimm(slice(instr, 9, 12), slice(instr, 6, 9), slice(instr, 0, 5));
        else
            AND(slice(instr, 9, 12), slice(instr, 6, 9), slice(instr, 0, 3));
        break;
    case 6:         //0110 LDR
        LDR(slice(instr, 9, 12), slice(instr, 6, 9), slice(instr, 0, 6));
        break;
    case 7:         //0111 STR
        STR(slice(instr, 9, 12), slice(instr, 6, 9), slice(instr, 0, 6));
        break;
    case 8:         //1000 RTI
        RTI();
        break;
    case 9:         //1001 NOT
        NOT(slice(instr, 9, 12), slice(instr, 6, 9));
        break;
    case 10:        //1010 LDI
        LDI(slice(instr, 9, 12), slice(instr, 0, 9));
        break;
    case 11:        //1011 STI
        STI(slice(instr, 9, 12), slice(instr, 0, 9));
        break;
    case 12:        //1100 RET
        RET();
        break;
    case 13:        //1101 reserved

        break;
    case 14:        //1110 LEA
        LEA(slice(instr, 9, 12), slice(instr, 0, 9));
        break;
    case 15:        //1111 TRAP
        TRAP(slice(instr, 0, 8));
        break;
    }
}

void Simulator::step_over()
{
    sim_status = Normal;

    device_keyboard();
    device_monitor();

    MAR = PC;
    PC += 1;
    if(!check_interrupt())
    {
        MDR = mem[MAR];
        IR = MDR;
        process_instr(IR);

        HistoryCount++;
    }
    if(sim_status==Normal && breakpoints[PC])
    {
        sim_status = Breakpoint;
    }
}

void Simulator::device_keyboard()
{
    unsigned int kb;
    //dealing with InputZ
    if(_kbhit())
    {
        kb = _getch();
        if(kb==27)          //press esc to cause User_Interrupt(suspend the program and into the command mode)
        {
            sim_status = User_Interrupt;
            return;
        }
        else
        {
            //INPUT
            mem[KBSR_] = (mem[KBSR_] & 0x7fff) + 0x8000;
            mem[KBDR_] =  kb&0x00ff;
        }
    }
}

void Simulator::device_monitor()
{
    if((mem[DSR_]&0x8000) == 0)
    {
        printf("%c", mem[DDR_]&0x00ff);
        mem[DSR_] = (mem[DSR_] & 0x7fff) + 0x8000;
    }
}

bool Simulator::check_interrupt()
{
    if(bit(mem[KBSR_],14)&&bit(mem[KBSR_], 15))
    {//check the keyboard interrupt
        if(0x0001<=slice(PSR, 8, 11))
            return false;
        else
        {
            process_interrupt(1, 0x0001);
            return true;
        }
    }
    else
    {
        return false;
    }
}


void Simulator::process_interrupt(word INTV, word Priority)
{
    MDR = PSR;
    PSR = (PSR & 63743) + (Priority<<8);
    if(bit(PSR, 15))
    {
        PSR = PSR & 0x0fff;

        Saved_USP = gen_reg[6];
        gen_reg[6] = Saved_SSP;

    }
    gen_reg[6]--;
    MAR = gen_reg[6];
    mem[MAR] = MDR;

    gen_reg[6]--;
    MDR = PC - 1;
    MAR = gen_reg[6];
    mem[MAR] = MDR;

    MAR = 0x0100 + INTV;
    MDR = mem[MAR];
    PC  = MDR;

}
void Simulator::run()
{
    sim_status = Normal;
    while(true)
    {
        step_over();
        if(sim_status != Normal)
            break;
    }
}

void Simulator::run(int i)
{
    sim_status = Normal;
    for(;i>0;i--)
    {
        step_over();
        if(sim_status != Normal)
            break;
    }
}

std::string Simulator::word_to_bin(word x)
{
    std::string ret = "0000000000000000";
    for(int i = 0; i<16 ;i++)
    {
        if((x&(1<<(15-i)))>0)
           ret[i] = '1';
    }
    return ret;
}
word Simulator::bin_to_word(char str[])
{
    word ret = 0;
    for(int i=0;i<16;i++)
    {
        if(str[i]=='1')
            ret += 1<<(15-i);
        else if(str[i]!='0')
        {
            throw 99;
        }
    }
    return ret;
}

void Simulator::load_bin(std::string filename)        //load an .bin file
{
    FILE* fp;
    word start_loc;

    fp = fopen(filename.c_str(), "r");
    if(fp == NULL)
    {
        message << "File error when opening \"" << filename << "\""<< std::endl;
        return;
    }
    char buf[17];
    try
    {
        message << "Loading \"" << filename << "\"...";
        if(fgets(buf, 18, fp)!=NULL)
        {
            start_loc = bin_to_word(buf);
        }
        std::vector<word> data;
        while(fgets(buf,18,fp)!=NULL)
        {
            data.push_back(bin_to_word(buf));
        }
        if(data.size()+start_loc > 0xFE00)
        {
            message << "Illegal memory space" << std::endl;
            return;
        }
        for(int i=0;i<data.size();i++)
        {
            mem[start_loc+i] = data[i];
        }
        fclose(fp);
    }
    catch(int ex)
    {
        if(ex==99)
            message << "Format error" << std::endl;
        return;
    }

    message << "Done" << std::endl;
}

std::vector<std::string> Simulator::instr_to_asm(word h)
{
    std::vector<std::string> ret;
    std::string s;
    if(isNOP(h))
    {
        s = "NOP";
        ret.push_back(s);
        return ret;
    }
    switch(h>>12)
    {
    case 0:         //0000 BR
        s = "BR";
        if(bit(h, 11)) s += "n";
        if(bit(h, 10)) s += "z";
        if(bit(h, 9))  s += "p";
        ret.push_back(s);
        ret.push_back(str_imm(h, 0, 9));
        break;
    case 1:         //0001 ADD
        s = "ADD";
        ret.push_back(s);
        if(bit(h, 5))
        {
            ret.push_back(str_reg(h, 9, 12));
            ret.push_back(str_reg(h, 6, 9));
            ret.push_back(str_imm(h, 0, 5));
        }
        else
        {
            ret.push_back(str_reg(h, 9, 12));
            ret.push_back(str_reg(h, 6, 9));
            ret.push_back(str_reg(h, 0, 3));
        }
        break;
    case 2:         //0010 LD
        s = "LD";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_imm(h, 0, 9));
        break;
    case 3:         //0011 ST
        s = "ST";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_imm(h, 0, 9));
        break;
    case 4:         //0100 JSR/JSRR
        s = "JSR";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_imm(h, 0, 9));
        if(bit(h, 11))
        {
            s = "JSR";
            ret.push_back(s);
            ret.push_back(str_imm(h, 0, 11));
        }
        else
        {
            s = "JSRR";
            ret.push_back(s);
            ret.push_back(str_reg(h, 6, 9));
        }
        break;
    case 5:         //0101 AND
        s = "AND";
        ret.push_back(s);
        if(bit(h, 5))
        {
            ret.push_back(str_reg(h, 9, 12));
            ret.push_back(str_reg(h, 6, 9));
            ret.push_back(str_imm(h, 0, 5));
        }
        else
        {
            ret.push_back(str_reg(h, 9, 12));
            ret.push_back(str_reg(h, 6, 9));
            ret.push_back(str_reg(h, 0, 3));
        }
        break;
    case 6:         //0110 LDR
        s = "LDR";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_reg(h, 6, 9));
        ret.push_back(str_imm(h, 0, 6));
        break;
    case 7:         //0111 STR
        s = "STR";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_reg(h, 6, 9));
        ret.push_back(str_imm(h, 0, 6));
        break;
    case 8:         //1000 RTI
        s = "RTI";
        ret.push_back(s);
        break;
    case 9:         //1001 NOT
        s = "NOT";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_reg(h, 6, 9));
        break;
    case 10:        //1010 LDI
        s = "LDI";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_imm(h, 0, 9));
        break;
    case 11:        //1011 STI
        s = "STI";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_imm(h, 0, 9));
        break;
    case 12:        //1100 RET
        s = "RET";
        ret.push_back(s);
        break;
    case 13:        //1101 reserved
        s = "NOP";
        ret.push_back(s);
        break;
    case 14:        //1110 LEA
        s = "LEA";
        ret.push_back(s);
        ret.push_back(str_reg(h, 9, 12));
        ret.push_back(str_imm(h, 0, 9));
        break;
    case 15:        //1111 TRAP
        s = "TRAP";
        ret.push_back(s);
        ret.push_back(str_imm(slice(h, 0, 8)));
        break;
    }
    return ret;
}


std::string  Simulator::str_imm(word x, int _start, int _end)
{
    char buf[16];
    sprintf(buf, "#%hd", sign_extend(slice(x, _start, _end), _end - _start));
    std::string s(buf);
    return s;
}
std::string  Simulator::str_imm(word x)
{
    char buf[16];
    sprintf(buf, "#%hd", x);
    std::string s(buf);
    return s;
}
std::string  Simulator::str_fulhex(word x)
{
    char buf[8];
    sprintf(buf, "x%4hx", x);
    std::string s(buf);
    for(int i=1;buf[i]==' ';i++)
        s[i]='0';
    return s;
}

std::string Simulator::str_reg(word x, int _start, int _end)
{
    char buf[4];
    sprintf(buf, "R%hd", slice(x, _start, _end));
    std::string s(buf);
    return s;
}

void Simulator::show_mem(int _start, int _end)
{
    //cursor_xy(0, 3);

    text_color(0x70);
    printf("    Loc     |        Bin          |   Hex   |       Instruction        \n");
    text_color(0x07);

    std::string fstr;
    unsigned short int bcolor, fcolor;
    for(int i=_start; i<_end; i++)
    {
        //get the foreground and background color
        if(PC==i)
        {
            bcolor =(sim_status==Select&&(vselect==i||vjump==i))?0x00e0:0x0060;
            fcolor = 0x0000;
        }
        else if(sim_status==Select&&vselect==i)
        {
            bcolor = 0x0070;
            fcolor = 0x0000;
        }
        else if(sim_status==Select&&vjump==i)
        {
            bcolor = 0x0080;
            fcolor = 0x0000;
        }
        else
        {
            bcolor = 0x0000;
            fcolor = 0x0007;
        }

        text_color(bcolor+fcolor);

        if(breakpoints[i])
            {text_color(0x04 + bcolor); printf("��");}
        else
            printf("  ");
        if(PC==i)
            {printf(">>>");}
        else
            printf("   ");
        std::cout << fstr;

        text_color(bcolor+fcolor);

        printf("%s        ", str_fulhex(i).c_str());
        std::cout << word_to_bin(mem[i]) << "   " ;
        printf("%s           ", str_fulhex(mem[i]).c_str());
        std::vector<std::string> asmstr = instr_to_asm(mem[i]);
        text_color(0x09+bcolor);
        std::cout << asmstr[0] << "  " ;
        text_color(fcolor+bcolor);
        for(int i = 1; i < asmstr.size(); i++)
        {
            std::cout << asmstr[i] << "  " ;
        }
        std::cout << std::endl;
    }
}

void Simulator::show_mem()
{
    cursor_xy(0, 3);
    if(vtrackPC)
    {
        vstart = PC-5 > 0 ? PC-5 : 0;
    }
    show_mem(vstart, (vstart+25<0x10000)?(vstart+25):0x10000);
}

word Simulator::to_word(std::string str)
{
    word vv;
    if(str[0]=='x')
    {
        if(sscanf(str.c_str()+1,"%hx", &vv)==0)
        {
            throw 98;
        }
    }
    else if(str[0]=='#')
    {
        if(sscanf(str.c_str()+1,"%hd", &vv)==0)
        {
            throw 98;
        }
    }
    else
    {
        if(sscanf(str.c_str(),"%hd", &vv)==0)
        {
            throw 98;
        }
    }
    return vv;
}

void Simulator::clear_count()
{
    HistoryCount = 0;
}

void Simulator::set_value(std::string t, std::string v)
{
    word vv;
    try
    {
        vv = to_word(v);
    }
    catch(int ex)
    {
        //if(ex==98)
        message << "Cannot recognize" << v << std::endl;
    }
    if(t[0]=='R')
    {
        if(t.size()==2)
        {
            int regn = t[1]-'0';
            if(0<=regn&&regn<=7)
            {
                gen_reg[regn] = vv;
                return;
            }
        }
    }
    else if(t[0]=='x')
    {
        if(t.size()==5)
        {
            word memn;
            if(sscanf(t.c_str()+1, "%hx", &memn)==1)
            {
                mem[memn] = vv;
                return;
            }
        }
    }
    else if(t=="PC")
    {
        PC = vv;
        return;
    }

    message << "Unexpected parameter"  << std::endl;
}
bool Simulator::cmd(char str[])
{
    std::string s0(str);
    std::string s;
    std::string p1, p2, p3;
    std::stringstream strm(s0);
    strm >> s;
    if(s=="loadbin")
    {
        strm >> p1;
        load_bin(p1);
    }
    else if(s=="setvalue"||s=="setv"||s=="sv")
    {
        strm >> p1;
        strm >> p2;
        set_value(p1, p2);
    }
    else if(s=="run")
    {
        run();
    }
    else if(s=="setbk"||s=="sbk")
    {
        strm >> p1;
        word v1;
        try
        {
            v1 = to_word(p1);
        }
        catch(int ex)
        {
            message << "Cannot recognize" << p1 << std::endl;
            return false;
        }
        set_bk(v1);
        message << "Add breakpoint at "  << v1 << std::endl;
    }
    else if(s=="showmem"||s=="smm")
    {
        if(strm >> p1)
        {
            word v1;
            try
            {
                v1 = to_word(p1);
                vtrackPC = false;
                vstart = v1;
            }
            catch(int ex)
            {
                message << "Cannot recognize " << p1 << std::endl;
                return false;
            }
        }
        else
        {
            vtrackPC = true;
        }

    }
    else if(s=="cancelbk"||s=="cbk")
    {
        strm >> p1;
        word v1;
        try
        {
            v1 = to_word(p1);
        }
        catch(int ex)
        {
            message << "Cannot recognize " << p1 << std::endl;
            return false;
        }
        cancel_bk(v1);
    }
    else if(s=="cancelallbk"||s=="cabk")
    {
        cancel_all_bk();
    }
    else if(s=="savemem")
    {
        strm >> p1 >> p2 >> p3;
        word v1, v2;
        try
        {
            v1 = to_word(p1);
            v2 = to_word(p2);
        }
        catch(int ex)
        {
            message << "Cannot recognize " << p1 << std::endl;
            return false;
        }
        //save_mem(p3, v1, v2);
    }
    else if(s=="stepover"||s=="n")
    {
        step_over();
    }
    else if(s[0]=='n')
    {
        int i = 0;
        while(s[i]=='n'&&i<s.size())
            i++;
         if(i==s.size())
        {
            run(i);
        }
        else
        {
            message << "Not a command" << std::endl;
            return false;
        }
    }
    else if(s=="select"||s=="w")
    {
        message << "Press ESC to back to CMD mode" <<std::endl;
        sim_status = Select;
    }
    else if(s=="asm"||s=="assemble")
    {
        strm >> p1;
        strm >> p2;
        assembler(p1, p2);
    }
    else if(s=="exit")
    {
        sim_status = Exit;
    }
    else if (s=="help")
    {

    }
    else if (s=="reset")
    {
        initialize();
    }
    else if(s=="clearcount"||s=="cc")
    {
        HistoryCount=0;
    }
    else
    {
        message << "Not a command" << std::endl;
        return false;
    }
    return true;
}

void Simulator::show_status()
{
    cursor_xy(0, 0);
    printf("PC: %s         IR: %s        PSR: %s         CC: %c\n", str_fulhex(PC).c_str(), str_fulhex(IR).c_str(), str_fulhex(PSR).c_str(), bit(PSR, 0)?'p':(bit(PSR, 1)?'z':'n'));
    for(int i = 0;i<=3;i++)
    {
        printf("R%d: %s %6hd  ",i, str_fulhex(gen_reg[i]).c_str(), gen_reg[i]);
    }
    printf("\n");
    for(int i = 4;i<=7;i++)
    {
        printf("R%d: %s %6hd  ",i, str_fulhex(gen_reg[i]).c_str(), gen_reg[i]);
    }
    printf("\n");

}

/*void Simulator::save_mem(std::string filename, word _start, word _end)
{
    FILE* fp;
    if()
}*/
void Simulator::cursor_xy(int x, int y)
{
	COORD coord;
	coord.X = x;
	coord.Y = y;
	SetConsoleCursorPosition(stdOutputHandle, coord);
}

void Simulator::text_color(unsigned short int color)
{
    SetConsoleTextAttribute(stdOutputHandle, color);
}

void Simulator::set_bk(word loc)
{
    breakpoints[loc] = true;
    breakpoints_set.insert(loc);
}
void Simulator::cancel_bk(word loc)
{
    breakpoints[loc] = false;
    if(breakpoints_set.count(loc)>0)
        breakpoints_set.erase(loc);
}
void Simulator::cancel_all_bk()
{
    memset(breakpoints, 0, sizeof(bool)*0x10000);
    breakpoints_set.clear();
}

void Simulator::show_message()
{
    //cursor_xy(0, 29);
    //std::string s;
    printf("\n");
    text_color(0x00b0);
    printf("                                Message                                \n");
    text_color(0x0007);
    std::cout << message.str();
    printf("\n");
    message.str("");
    printf("Instructions Executed: %d                  Status: ", HistoryCount);
    if(sim_status==Normal) printf("Normal\n");
    else if(sim_status==Breakpoint)printf("Breakpoint\n");
    else if(sim_status==Priviledge_Exception)printf("Priviledge_Exception\n");
    else if(sim_status==User_Interrupt)printf("User Interrupt\n");
    else if(sim_status==Select)printf("Select\n");
}

bool Simulator::isNOP(word h)
{
    switch(h>>12)
    {
    case 0:         //0000 BR
        return (h&3584)==0;
        break;
    case 1:         //0001 ADD
        if(!bit(h, 5))
        {
            return (h&24)>0;
        }
        break;
    case 4:         //0100 JSR/JSRR
        if(bit(h, 11))
        {
            return false;
        }
        else
        {
            return (h&1599)>0;
        }
        break;
    case 5:         //0101 AND
        if(!bit(h, 5))
        {
            return (h&24)>0;
        }
        break;
    case 8:         //1000 RTI
        return slice(h, 0, 12)!=0;
        break;
    case 9:         //1001 NOT
        return slice(h, 0, 6)!=63;
        break;
    case 12:        //1100 RET
        return h!=49600;
        break;
    case 13:        //1101 reserved
        return true;
        break;
    case 15:        //1111 TRAP
        return slice(h, 8, 12)!=0;
        break;
    }
    return false;
}

void Simulator::assembler(std::string filename, std::string ofilename)
{
    std::ifstream f(filename.c_str());

    if(!f.is_open())
    {
        message << "File error when opening \"" << filename << "\""<< std::endl;
        return;
    }

    int errnum = 0;

    int line_n = 0;

    std::map<std::string, word> symmap;
    std::vector<std::string> syms;
    std::vector<int> symloc;

    char buf[256];
    std::vector<std::string> lines;
    std::vector<int> line_c;

    std::string s;

    std::vector<word> bin;
    std::vector<std::string> ps;
    word nzp;
    word pcoffset,imm,offset;
    word r1, r2, r3;

    std::string ss;
    std::vector<word> instr;
    std::vector<std::string> params;
    std::vector<word> addr;

    word caddr, temp;
    int instr_n;
    bool endedflag = false;
    // input from file
    message << "Reading from" << "\""<< filename << "\"..."<< std::endl;
    while(1)
    {
        f.getline(buf, 256);
        if(f.fail())
        {
            message << "Error at line " << line_n<< ": Too long in a line." <<std::endl;
        }

        int ln = 0;

        while(buf[ln]!='\0'&&buf[ln]!=';')
            ln++;
        buf[ln] = '\0';
        s = buf;
        s = trim_space(s);

        if(s!="")
        {
            lines.push_back(s);
            line_c.push_back(line_n);
        }
        line_n++;
        if(f.eof())
            break;
    }
    f.close();
    message << " Reading done"<< std::endl;


    // check instruction
    for(int i = 0; i<lines.size(); i++)
    {
        ss = lines[i].c_str();
        s = split_first(ss);
        if(asm_map.find(s)==asm_map.end())
        {
            syms.push_back(s);
            symloc.push_back(i);
            if(ss=="")
            {
                s = "~SKIP";
            }
            else
            {
                s = split_first(ss);
                if(asm_map.find(s)==asm_map.end())
                {
                    message << "Error at line " << line_c[i] << ": Unrecoginized instruction \"" << s << "\""<<std::endl;
                    errnum ++;
                }
            }
        }
        instr.push_back(asm_map[s]);
        params.push_back(ss);
    }

    //assign address
    if(instr[0]!=23)
    {
        message << "Error at line " << line_c[0] << ": Expected \".ORIG\" "<< std::endl;
        errnum++;
        goto Abort;
    }
    else
    {
        try
        {
            caddr = to_word(params[0]);
        }
        catch(int ex)
        {
            message << "Error at line " << line_c[0] << ": Unrecoginized syntax" << params[0] <<std::endl;
            errnum++;
            goto Abort;
        }
    }
    bin.push_back(caddr);
    instr_n = instr.size();
    for(int i = 1; i<instr_n; i++)
    {
        addr.push_back(caddr);
        switch(instr[i])
        {
        case 21:        // .BLKW
            try
            {
                temp = to_word(params[i]);
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecoginized instruction \"" << s << "\""<<std::endl;
                errnum ++;
            }
            caddr += temp;
            break;
        case 22:        // .STRINGZ
            s = trim_space(params[i]);
            if(s[0]='"'&&s[s.length()-1]=='"')
            {
                caddr += s.length()-2;
            }
            else
            {
                message << "Error at line " << line_c[i] << ":  Expected string constant, but \"" << s << "\" found instead." << std::endl;
                errnum++;
            }
            break;
        case 25:        // ~SKIP

            break;
        case 24:        // .END
            instr_n = i;
            if(instr_n < lines.size()-1)
            {
                message << "Warning at line " << line_c[i] << ": The following code after .END is ignored." << std::endl;
            }
            endedflag = true;
            break;
        case 23:        // .ORIG
            message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
            errnum++;
            goto Abort;
            break;
        default:
            caddr += 1;
        }
    }

    if(!endedflag)
    {
        message << "Error at line " << line_c[instr_n-1] << ": Expected .END before end of file." << std::endl;
        errnum++;
        goto Abort;

    }

    for(int i = 0; i < syms.size(); i++)
    {
        if(symmap.find(syms[i])==symmap.end())
        {
            symmap[syms[i]] = addr[symloc[i]];
        }
        else
        {
            message << "Error at line " << symloc[i] << ": A label was defined more than once." << std::endl;
            goto Abort;
            errnum++;
        }
    }

    //assemble
    for(int i = 1; i < instr_n; i++)
    {
        switch(instr[i])
        {
        case 30:        //BR
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            nzp = instr[i]-30;
            try
            {
                pcoffset = symmap.find(params[i])!=symmap.end()?(symmap[params[i]]-addr[i]-1):(to_word(params[i]));
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            if(check_number(pcoffset, 9))
                bin.push_back(0+(nzp<<9)+slice(pcoffset, 0, 9));
            else
            {
                add_overflow_err(line_c[i], 9);
                errnum++;
            }
            break;
        case 1:         //AND ADD
        case 5:
            ps = split(params[i]);
            if(ps.size()!=3)
            {
                message << "Error at line " << line_c[i] << ": Expected 3 parameters but " << ps.size() << "found" << std::endl;
                errnum++;
            }
            r1 = to_reg(ps[0]);
            r2 = to_reg(ps[1]);
            r3 = to_reg(ps[2]);
            if(r1<=7&&r2<=7)
            {
                if(r3<=7)
                {
                    bin.push_back((instr[i]<< 12) + (r1<<9)+ (r2<<6) + r3);
                }
                else
                {
                    try
                    {
                        imm = to_word(ps[2]);
                    }
                    catch(int ex)
                    {
                        message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                        errnum++;
                    }
                    if(check_number(imm, 5))
                    {
                        bin.push_back((instr[i]<<12) + (r1<<9)+ (r2<<6)+ (1<<5) + slice(imm, 0, 5));
                    }
                    else
                    {
                        add_overflow_err(line_c[i], 5);
                    }
                }
            }
            else
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            break;
        case 2:         //LD ST LEA
        case 3:
        case 14:
            ps = split(params[i]);
            if(ps.size()!=2)
            {
                message << "Error at line " << line_c[i] << ": Expected 2 parameters but " << ps.size() << "found" << std::endl;
                errnum++;
            }
            r1 = to_reg(ps[0]);
            if(r1>7)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            try
            {
                pcoffset = symmap.find(ps[1])!=symmap.end()?symmap[ps[1]]-addr[i]-1:to_word(ps[1]);
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            if(check_number(pcoffset, 9))
                bin.push_back((instr[i]<<12)+(r1<<9)+slice(pcoffset, 0, 9));
            else
            {
                add_overflow_err(line_c[i], 9);
                errnum++;
            }
            break;

        case 6:         //LDR STR LDI STI
        case 7:
        case 10:
        case 11:
            ps = split(params[i]);
            if(ps.size()!=3)
            {
                message << "Error at line " << line_c[i] << ": Expected 2 parameters but " << ps.size() << " found" << std::endl;
                errnum++;
            }
            r1 = to_reg(ps[0]);
            r2 = to_reg(ps[1]);
            if(r1>7||r2>7)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            try
            {
                pcoffset = symmap.find(ps[2])!=symmap.end()?symmap[ps[2]]-addr[i]-1:to_word(ps[2]);
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            if(check_number(pcoffset, 6))
                bin.push_back((instr[i]<<12)+(r1<<9) + (r2<<6) + slice(pcoffset, 0, 6));
            else
            {
                add_overflow_err(line_c[i], 6);
                errnum++;
            }
            break;

        case 9:         //NOT
            ps = split(params[i]);
            if(ps.size()!=2)
            {
                message << "Error at line " << line_c[i] << ": Expected 2 parameters but " << ps.size() << " found" << std::endl;
                errnum++;
                goto Abort;
            }
            r1 = to_reg(ps[0]);
            r2 = to_reg(ps[1]);
            if(r1>7||r2>7)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
                goto Abort;
            }
            bin.push_back((instr[i]<<12) + (r1<<9)+(r2<<6)+((1<<6)-1));
            break;
        case 8:         //RTI
            bin.push_back(instr[i]<<12);
            break;
        case 12:        //RET
            bin.push_back((instr[i]<<12) + (7<<6));
            break;
        case 15:        //TRAP
            ps = split(params[i]);
            if(ps.size()!=1)
            {
                message << "Error at line " << line_c[i] << ": Expected 1 parameters but " << ps.size() << " found" << std::endl;
                errnum++;
            }
            try
            {
                imm = to_word(ps[0]);
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            if(!check_number(imm, 8))
            {
                add_overflow_err(line_c[i], 8);
            }
            bin.push_back((instr[i]<<12) + slice(imm,0,8));
            break;
        case 20:        //.FILL
            try
            {
                imm = to_word(params[i]);
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            bin.push_back(imm);
            break;
        case 21:        //.BLKW
            try
            {
                imm = to_word(params[i]);
            }
            catch(int ex)
            {
                message << "Error at line " << line_c[i] << ": Unrecognized syntax" << std::endl;
                errnum++;
            }
            for(int j = 0;j<imm;j++)
            {
                bin.push_back(0);
            }
            break;
        case 22:        //.STRINGZ
            for(int j =1;j<params[i].size()-1;j++)
            {
                bin.push_back(params[i][j]);
            }
            break;
        case 25:        //~SKIP

            break;
        }

    }
    Abort:

    if(errnum == 0)
    {
        message << "Done. " << errnum << " error(s)" <<std::endl;
        std::ofstream ofile(ofilename);
        message << "Written to file. \""<< ofilename <<"\"" << std::endl;
        for(int i = 0;i<bin.size(); i++)
        {
            ofile << word_to_bin(bin[i]) << std::endl;
        }
        ofile.close();
        message << "Done." <<std::endl;
    }
    else
    {
        message << "Failed. " << errnum << " error(s)" <<std::endl;
    }

}

std::string Simulator::trim_space(std::string s)
{
    int l = 0, r = s.length()-1;
    while(s[l]==' '||s[l]=='\t')l++;
    while(s[r]==' '||s[r]=='\t')r--;
    return s.substr(l, r-l+1);
}

word Simulator::to_reg(std::string s)
{
    if(s.size()==2 && s[0]=='R')
    {
        if(0<=s[1]-'0' && s[1]-'0'<=7)
            return s[1]-'0';
    }
    return 99;
}

bool Simulator::check_number(word x, int n)
{
    if(bit(x, 15))
        x = (~x)+1;
    return x<(1<<n);
}

std::vector<std::string> Simulator::split(std::string str)
{
    std::vector<std::string> ret;
    int n = 0, nn;
    while(1)
    {
        nn = str.find(',', n);
        if(nn==str.npos)
            break;
        ret.push_back(trim_space(str.substr(n, nn-n)));
        n = nn+1;
    }
    ret.push_back(trim_space(str.substr(n, str.size()-n)));
    return ret;
}

std::string Simulator::split_first(std::string& str)
{
    std::string ret;
    int nn;
    str = trim_space(str);
    nn = std::min(str.find('\t', 0),str.find(' ', 0));
    if(nn!=str.npos)
    {
        ret = trim_space(str.substr(0, nn));
        str = trim_space(str.substr(nn+1, str.size()-nn-1));
    }
    else
    {
        ret = str;
        str = "";
    }
    return ret;
}
void Simulator::add_overflow_err(int line, int bitn)
{
    message << "Error at line " << line << ": Cannot represent in "<< bitn << "bit number" << std::endl;
}
/*
    asm_map["BR"] = 30;
    asm_map["BRp"] = 31;
    asm_map["BRz"] = 32;
    asm_map["BRzp"] = 33;
    asm_map["BRn"] = 34;
    asm_map["BRnp"] = 35;
    asm_map["BRnz"] = 36;
    asm_map["BRnzp"] = 37;
    asm_map["ADD"] = 1;
    asm_map["LD"] = 2;
    asm_map["ST"] = 3;
    asm_map["JSR"] = 4;
    asm_map["JSRR"] = 40;
    asm_map["AND"] = 5;
    asm_map["LDR"] = 6;
    asm_map["STR"] = 7;
    asm_map["RTI"] = 8;
    asm_map["NOT"] = 9;
    asm_map["LDI"] = 10;
    asm_map["STI"] = 11;
    asm_map["RET"] = 12;
    asm_map["LEA"] = 14;
    asm_map["TRAP"] = 15;

    asm_map[".FILL"] = 20;
    asm_map[".BLKW"] = 21;
    asm_map[".STRINGZ"] = 22;
    asm_map[".ORIG"] = 23;
    asm_map[".END"] = 24;
    asm_map["~SKIP"] = 25;
 */

