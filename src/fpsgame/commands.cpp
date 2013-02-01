#include "game.h"
#include "punitiveeffects.h"

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
    
    int parse_ip_mask_string(const char* input, uint* ip, uint* mask, clientinfo** tci)
    {
    	int tcn;
    	uint temp_ip;
    	uchar* ipc = (uchar*)&temp_ip;
    	if(sscanf(input, "%hhu.%hhu.%hhu.%hhu", &ipc[0], &ipc[1], &ipc[2], &ipc[3]) == 4)
    	{
    		*ip = temp_ip;
    	}
    	else if(sscanf(input, "%d", &tcn) == 1)
    	{
    		*tci = getinfo(tcn);
    		if(*tci)
    		{
    			*ip = getclientip((*tci)->clientnum);
    		}
    		else return -1;
    	}
    	else
    	{
    		return -1;
    	}

    	temp_ip = 0;
    	char *maskpos = strchr((char*)input, ':');
    	if(maskpos)
    	{
    		if(sscanf(&maskpos[1], "%hhu.%hhu.%hhu.%hhu", &ipc[0], &ipc[1], &ipc[2], &ipc[3]) == 4)
    		{
    			*mask = temp_ip;
    		}
    	}

    	return 0;
    }

    struct timevalue {

    	char unit;
    };

    int parse_time_string(const char* input, uint* expiry_time)
    {
    	*expiry_time = 0;

    	vector<char> numbers;
    	while(*input)
    	{
    		if(*input >= '0' && *input <= '9')
    		{
    			numbers.add(*input);
    		}
    		else if(*input == 's' || *input == 'S')
    		{
    			*expiry_time += (atoi(numbers.getbuf()));
    			numbers.shrink(0);
    		}
    		else if(*input == 'm' || *input == 'M')
    		{
    			*expiry_time += (atoi(numbers.getbuf())*60);
    			numbers.shrink(0);
    		}
    		else if(*input == 'h' || *input == 'H')
    		{
    			*expiry_time += (atoi(numbers.getbuf())*60*60);
    			numbers.shrink(0);
    		}
    		else if(*input == 'd' || *input == 'D')
    		{
    			*expiry_time += (atoi(numbers.getbuf())*60*60*24);
    			numbers.shrink(0);
    		}
    		else if(*input == 'y' || *input == 'Y')
    		{
    			*expiry_time += (atoi(numbers.getbuf())*60*60*24*365);
    			numbers.shrink(0);
    		}
    		else
    		{
    			return -1;
    		}
    		input++;
    	}

    	if(numbers.length())
    	{
			*expiry_time += (atoi(numbers.getbuf()));
    	}

    	return 0;
    }

    void add_effect(clientinfo *ci, vector<char*> args, const char* effect_type)
    {
		clientinfo* tci = NULL;
		vector<char> reasonchrs;

		uint target_id = 0;
		char* target_name = (char*)"";
		uint target_ip;
		uint target_mask = 0xFFFFFF00;

		uint master_id = ci->uid;
		char* master_name = (char*)ci->name;
		uint master_ip = getclientip(ci->clientnum);

		uint expiry_time = 3600;
		char* reason = NULL;

		if(parse_ip_mask_string(args[1], &target_ip, &target_mask, &tci))
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Invalid player specifier arguments\fr");
			return;
		}

		if(tci)
		{
			target_id = tci->uid;
			target_name = tci->name;
		}

		if(args.length() >= 3)
		{
			if(parse_time_string(args[2], &expiry_time))
			{
				sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Invalid time arguments\fr");
				return;
			}
		}

		if(args.length() >= 4)
		{
			loopv(args) if(i >= 3)
			{
				if(i > 3) reasonchrs.put(" ", 1);
				reasonchrs.put(args[i], strlen(args[i]));
			}
			reason = reasonchrs.getbuf();
		}

        if(!requestlocalmasterf("addeffect %s %u %s %u %u %u %s %u %u %s\n", effect_type, target_id, target_name, target_ip, target_mask, master_id, master_name, master_ip, expiry_time, reason ? reason : "unspecfied"))
        {
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to local master server.");
        }
    }

    void cmd_mute(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
			if(args.length() < 2)
			{
				sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2mute <cn|ip>(:mask) (time) (reason)\fr");
				return;
			}
			add_effect(ci, args, "mute");
        }
        else
        {
            insufficientpermissions(ci);
        }
    }

    void cmd_spec(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
			if(args.length() < 2)
			{
				sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2spec <cn|ip>(:mask) (time) (reason)\fr");
				return;
			}
			add_effect(ci, args, "spectate");
		}
		else
		{
			insufficientpermissions(ci);
		}
    }

    void cmd_ban(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
			if(args.length() < 2)
			{
				sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2ban <cn|ip>(:mask) (time) (reason)\fr");
				return;
			}
			add_effect(ci, args, "ban");
		}
		else
		{
			insufficientpermissions(ci);
		}
    }

    void cmd_limit(clientinfo *ci, vector<char*> args)
    {
        if(hasadmingroup(ci) || hasmastergroup(ci))
        {
			if(args.length() < 2)
			{
				sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2limit <cn|ip>(:mask) (time) (reason)\fr");
				return;
			}
			add_effect(ci, args, "limit");
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

    void cmd_pauseondisconnect(clientinfo *ci, vector<char*> args)
    {
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2pauseondisconnect <enable/disable>\fr");
            return;
        }
        
        if(!strcmp(args[1], "enable"))
        {
            if(server::pauseondisconnect) return;
            sendservmsg("\fs\f1Info:\fr Pause on disconnect \fs\f0enabled\fr.");
            server::pauseondisconnect = true;
        }
        else if(!strcmp(args[1], "disable"))
        {
            if(!server::pauseondisconnect) return;
            sendservmsg("\fs\f1Info:\fr Pause on disconnect \fs\f4disabled\fr.");
            server::pauseondisconnect = false;
        }
        else
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2pauseondisconnect <enable/disable>\fr");
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
        {"mute", PRIV_NONE, &cmd_mute},
        {"ban", PRIV_NONE, &cmd_ban},
        {"spec", PRIV_NONE, &cmd_spec},
        {"limit", PRIV_NONE, &cmd_limit},
        {"persistentintermission", PRIV_MASTER, &cmd_persistentintermission},
        {"persistentteams", PRIV_MASTER, &cmd_persistentteams},
        {"pauseondisconnect", PRIV_MASTER, &cmd_pauseondisconnect},
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
        logoutf("Command: %s: %s", ci->name, cmd);
        
        vector<char*> args;
        
        char* ptr = NULL;
        const char* sep = " ";
        char *cp = strdupa(cmd);
        
        char* arg = strtok_r(cp, (char*)sep, &ptr);
        while(arg)
        {
            args.add(newstring(arg));
            arg = strtok_r(NULL, (char*)sep, &ptr);
        }
        
        bool found = false;
        
        if(args.length() >= 1)
        {
            for(unsigned int i = 0; i < sizeof(commands)/sizeof(command); i++)
            {
                if(!strcmp(commands[i].name, args[0]))
                {
                    found = true;
                    if(commands[i].minprivilege <= ci->privilege)
                    {
                        (*commands[i].functionPtr)(ci, args);
                    }
                    else insufficientpermissions(ci);
                    break;
                }
            }
            if(!found) sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Unknown command.");
        }
        args.deletearrays();
    }
}
