#include "game.h"

namespace server
{
    void insufficientpermissions(clientinfo *ci)
    {
        sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Insufficient permissions.");
    }
    
    void invalidclient(clientinfo *ci)
    {
        sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Invalid client specified.");
    }
    
    void cmd_ip(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
            if(args.length() < 2)
            {
                sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2ip <cn>\fr");
                return;
            }
            
            int tcn = atoi(args[1]);
            clientinfo *tci = getinfo(tcn);
            
            if(tci)
            {
                uint ip = getclientip(tci->clientnum);
                uchar* ipc = (uchar*)&ip;
                sendcnservmsgf(ci->clientnum, "\fs\f2Info:\fr Client(%i) ip: %hhu.%hhu.%hhu.%hhu", tci->clientnum, ipc[0], ipc[1], ipc[2], ipc[3]);
            }
            else
            {
                invalidclient(ci);
            }
        }
        else
        {
            insufficientpermissions(ci);
        }
    }
    
    void cmd_master(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
            setmaster(ci, true, "", ci->localauthname, ci->localauthdesc, PRIV_MASTER, true);
        }
        else
        {
            insufficientpermissions(ci);
        }
    }
    
    void cmd_admin(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci))
        {
            setmaster(ci, true, "", ci->localauthname, ci->localauthdesc, PRIV_ADMIN, true);
        }
        else
        {
            insufficientpermissions(ci);
        }
    }
    
    void cmd_names(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
            if(args.length() < 2)
            {
                sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2names <cn>\fr");
                return;
            }
            
            int tcn = atoi(args[1]);
            clientinfo *tci = getinfo(tcn);
            
            if(tci)
            {
                uint ip = getclientip(tci->clientnum);
                uint mask = 0xFFFF;
                
                if(!nextlocalmasterreq) nextlocalmasterreq = 1;
                ci->localmasterreq = nextlocalmasterreq++;
                
                if(!requestlocalmasterf("names %u %u %u\n", ci->localmasterreq, ip, mask))
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to local master server.");
                }
            }
            else
            {
                invalidclient(ci);
            }
        }
        else
        {
            insufficientpermissions(ci);
        }
    }
    
    void cmd_persistentintermission(clientinfo *ci, vector<char*> args)
    {
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2persistentintermission <enable/disable>\fr");
            return;
        }
        
        if(!strcmp(args[1], "enable"))
        {
            if(server::persistentintermission) return;
            sendservmsg("\fs\f1Info:\fr Peristent intermission \fs\f0enabled\fr.");
            server::persistentintermission = true;
        }
        else if(!strcmp(args[1], "disable"))
        {
            if(!server::persistentintermission) return;
            sendservmsg("\fs\f1Info:\fr Peristent intermission \fs\f4disabled\fr.");
            server::persistentintermission = false;
        }
        else
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2persistentintermission <enable/disable>\fr");
            return;
        }
    }
    
    void cmd_persistentteams(clientinfo *ci, vector<char*> args)
    {
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2persistentteams <enable/disable>\fr");
            return;
        }
        
        if(!strcmp(args[1], "enable"))
        {
            if(server::persistentteams) return;
            sendservmsg("\fs\f1Info:\fr Peristent teams \fs\f0enabled\fr.");
            server::persistentteams = true;
        }
        else if(!strcmp(args[1], "disable"))
        {
            if(!server::persistentteams) return;
            sendservmsg("\fs\f1Info:\fr Peristent teams \fs\f4disabled\fr.");
            server::persistentteams = false;
        }
        else
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2persistentteams <enable/disable>\fr");
            return;
        }
    }
    
    void cmd_instaweapon(clientinfo *ci, vector<char*> args)
    {
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2instaweapon <weapon num>\fr");
            return;
        }
        
        int w = atoi(args[1]);
        
        if (w < GUN_FIST || w >= NUMGUNS)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2instaweapon <weapon num>\fr");
            return;
        }
        
        server::instaweapon = w;
        
        sendservmsgf("\fs\f1Info:\fr Insta weapon now set to: \fs\f4%s\fr.", guns[w].name);
    }
    
    struct command
    {
        const char *name;
        int minprivilege;
        void (*functionPtr)(clientinfo *ci, vector<char*> args);
    };
    
    void cmd_listcommands(clientinfo *ci, vector<char*> args);
    
    command commands[] = {
        {"ip", PRIV_NONE, &cmd_ip},
        {"master", PRIV_NONE, &cmd_master},
        {"admin", PRIV_NONE, &cmd_admin},
        {"names", PRIV_NONE, &cmd_names},
        {"persistentintermission", PRIV_MASTER, &cmd_persistentintermission},
        {"persistentteams", PRIV_MASTER, &cmd_persistentteams},
        {"instaweapon", PRIV_MASTER, &cmd_instaweapon},
        {"listcommands", PRIV_NONE, &cmd_listcommands}
    };
    
    void cmd_listcommands(clientinfo *ci, vector<char*> args)
    {
        vector<char> commandlist;
        
        bool first = true;
        for(unsigned int i = 0; i < sizeof(commands)/sizeof(command); i++)
        {
            if(commands[i].minprivilege <= ci->privilege)
            {
                if(first) first = false;
                else commandlist.put(", ", 2);
                
                char *colorstart;
                
                switch(commands[i].minprivilege){
                    case PRIV_MASTER:
                        colorstart = (char*)"\fs\f0";
                        break;
                    case PRIV_AUTH:
                        colorstart = (char*)"\fs\f5";
                        break;
                    case PRIV_ADMIN:
                        colorstart = (char*)"\fs\f6";
                        break;
                    case PRIV_NONE:
                    default:
                        colorstart = (char*)"\fs\f7";
                        break;
                }
                
                commandlist.put(colorstart, 4);
                commandlist.put(commands[i].name, strlen(commands[i].name));
                commandlist.put("\fr", 2);
            }
        }
        sendcnservmsgf(ci->clientnum, "\fs\f4Available commands:\fr %s", commandlist.getbuf());
    }
    
    void trycommand(clientinfo *ci, const char *cmd) 
    {
        logoutf("Command: %s: %s", colorname(ci), cmd);
    
        vector<char*> args;
        explodelist(cmd, args);
        
        if(args.length() < 1) return;
        
        for(unsigned int i = 0; i < sizeof(commands)/sizeof(command); i++)
        {
            if(!strcmp(commands[i].name, args[0]))
            {
                if(commands[i].minprivilege <= ci->privilege)
                {
                    (*commands[i].functionPtr)(ci, args);
                }
                else insufficientpermissions(ci);
                return;
            }
        }
        sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Unknown command.");
    }
}
