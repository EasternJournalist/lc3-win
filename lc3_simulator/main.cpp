#include <iostream>
#include "sim.h"
#include <stdlib.h>
#include <stdio.h>

using namespace std;

int main()
{
    Simulator sim;
    sim.initialize();
    char cmdstr[50];

    system("mode con cols=90");

    while(true)
    {
        system("cls");
        sim.show_status();
        sim.show_mem();
        sim.show_message();
        printf("\n");
        sim.text_color(0x80);
        printf("Command:");
        sim.text_color(0x07);
        if(!cin.getline(cmdstr, 50))
            continue;
        if(!sim.cmd(cmdstr))
        {
            continue;
        }
        if(sim.sim_status == sim.status_code::Exit)
            break;
        else if(sim.sim_status == sim.status_code::Select)
        {
            bool changed_flag = true;           //not to refresh the window every time.
            int ch;
            sim.vselect = sim.vstart+1;
            sim.vtrackPC = false;
            while (1)
            {
                if(changed_flag)
                {
                    system("cls");
                    sim.show_status();
                    sim.show_mem();
                    sim.show_message();
                    changed_flag = false;
                }
                if (_kbhit()){
                    ch = _getch();
                    if(ch==22472||ch=='w')
                    {
                        if(sim.vselect>0)sim.vselect--;
                        if(sim.vselect<sim.vstart+2 && sim.vstart>0)sim.vstart--;
                        changed_flag =  true;
                    }
                    else if(ch==22480||ch=='s')
                    {
                        if(sim.vselect<0xfffe)sim.vselect++;
                        if(sim.vselect>sim.vstart+23 && sim.vstart<0xfffe)sim.vstart++;
                        changed_flag =  true;
                    }
                    else if (ch == 27)
                    {
                        sim.sim_status = sim.status_code::Normal;
                        break;
                    }
                    if((!sim.isNOP(sim.mem[sim.vselect]))&&sim.slice(sim.mem[sim.vselect],12,16)==0)
                    {
                        sim.vjump = sim.vselect + sim.sign_extend(sim.slice(sim.mem[sim.vselect],0,9),9) +1;
                    }
                    else
                    {
                        sim.vjump = -1;
                    }
                }
            }
        }
        else if(sim.sim_status == sim.status_code::User_Interrupt)
        {
            printf("\n Program was interrupted by pressing ESC. Press Enter to back to CMD mode.");
            cin.getline(cmdstr, 50);
        }
    }
    system("pause");
    return 0;
}
