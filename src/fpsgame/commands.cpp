#include "game.h"
#include "punitiveeffects.h"

#define MINUTES 60
#define HOURS 3600
#define DAYS 86400
#define YEARS 31536000

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
        if(!hasadmingroup(ci) && !hasmastergroup(ci)) {
        	insufficientpermissions(ci);
        	return;
        }

		if(args.length() < 2)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2ip <cn>\fr");
			return;
		}

		int tcn = atoi(args[1]);
		clientinfo *tci = getinfo(tcn);

		if(!tci) {
			invalidclient(ci);
			return;
		}

		uint ip = getclientip(tci->clientnum);
		uchar* ipc = (uchar*)&ip;
		sendcnservmsgf(ci->clientnum, "\fs\f2Info:\fr Client(%i) ip: %hhu.%hhu.%hhu.%hhu", tci->clientnum, ipc[0], ipc[1], ipc[2], ipc[3]);
    }
    
    void cmd_names(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }

		if(args.length() < 2)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2names <cn>\fr");
			return;
		}

		int tcn = atoi(args[1]);
		clientinfo *tci = getinfo(tcn);

		if(!tci) {
			invalidclient(ci);
			return;
		}

		uint ip = getclientip(tci->clientnum);
		uint mask = 0xFFFF;

		if(!nextlocalmasterreq) nextlocalmasterreq = 1;
		ci->localmasterreq = nextlocalmasterreq++;

		if(!requestlocalmasterf("names %u %u %u\n", ci->localmasterreq, ip, mask))
		{
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr not connected to local master server.\fr");
		}
    }
    
    void cmd_master(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		setmaster(ci, true, "", ci->localauthname, ci->localauthdesc, PRIV_MASTER, true);
    }

    void cmd_admin(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		setmaster(ci, true, "", ci->localauthname, ci->localauthdesc, PRIV_ADMIN, true);
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

    int parse_time_string(const char* input, uint* expiry_time)
    {
    	*expiry_time = 0;
    	vector<char> numbers;
    	while(*input)
    	{
    		if(*input >= '0' && *input <= '9') {
    			numbers.add(*input);
    			input++;
    			continue;
    		}
    		else if((*input == 's' || *input == 'S') && numbers.length())
    			*expiry_time += (atoi(numbers.getbuf()));
    		else if((*input == 'm' || *input == 'M') && numbers.length())
    			*expiry_time += (atoi(numbers.getbuf())*MINUTES);
    		else if((*input == 'h' || *input == 'H') && numbers.length())
    			*expiry_time += (atoi(numbers.getbuf())*HOURS);
    		else if((*input == 'd' || *input == 'D') && numbers.length())
    			*expiry_time += (atoi(numbers.getbuf())*DAYS);
    		else if((*input == 'y' || *input == 'Y') && numbers.length())
    			*expiry_time += (atoi(numbers.getbuf())*YEARS);
    		else return -1;
    		numbers.shrink(0);
    		input++;
    	}
    	if(numbers.length()) *expiry_time += (atoi(numbers.getbuf()));
    	return 0;
    }

    void apply_effect(clientinfo *ci, const char *effect_type, uint target_id, const char *target_name, uint target_ip, uint target_mask, uint master_id, const char *master_name, uint master_ip, uint expiry_time, const char *reason)
    {
        if(!requestlocalmasterf("addeffect %s %u %s %u %u %u %s %u %u %s\n", effect_type, target_id, target_name, target_ip, target_mask, master_id, master_name, master_ip, expiry_time, reason ? reason : "unspecfied"))
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr not connected to local master server.\fr");
        }
    }

    void add_effect(clientinfo *ci, vector<char*> args, const char* effect_type)
    {
		clientinfo* tci = NULL;
		vector<char> reasonchrs;

		uint target_id = 0;
		char* target_name = (char*)"";
		uint target_ip;
		uint target_mask = 0x00FFFFFF;

		uint master_id = ci->uid;
		char* master_name = (char*)ci->name;
		uint master_ip = getclientip(ci->clientnum);

		uint expiry_time = 1*HOURS;
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
				if(i > 3) reasonchrs.put(' ');
				reasonchrs.put(args[i], strlen(args[i]));
			}
			reasonchrs.put(0);
			reason = reasonchrs.getbuf();
		}

		apply_effect(ci, effect_type, target_id, target_name, target_ip, target_mask, master_id, master_name, master_ip, expiry_time, reason ? reason : "unspecfied");
    }

    void cmd_mute(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		if(args.length() < 2)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2mute <cn|ip>(:mask) (time) (reason)\fr");
			return;
		}
		add_effect(ci, args, "mute");
    }

    void cmd_spec(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		if(args.length() < 2)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2spec <cn|ip>(:mask) (time) (reason)\fr");
			return;
		}
		add_effect(ci, args, "spectate");
    }

    void cmd_ban(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		if(args.length() < 2)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2ban <cn|ip>(:mask) (time) (reason)\fr");
			return;
		}
		add_effect(ci, args, "ban");
    }

    void cmd_limit(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		if(args.length() < 2)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2limit <cn|ip>(:mask) (time) (reason)\fr");
			return;
		}
		add_effect(ci, args, "limit");
    }

    void complaint_notify(clientinfo *ci, const char *target_name, uint target_ip, const char *master_name, uint master_ip, const char *complaint)
    {
        string srvdesc;
        filtertext(srvdesc, serverdesc, false);
        if(!requestlocalmasterf("complaint %s %s %u %s %u %s\n", srvdesc, target_name, target_ip, master_name, master_ip, complaint))
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr not connected to local master server.\fr");
        }
        else
        {
        	sendcnservmsg(ci->clientnum, "\fs\f2Info:\fr A complaint has been filed against the specified player.\fr");
        }
    }

    void easy_add_effect_or_notify(clientinfo *ci, vector<char*> args, const char* effect_type, uint expiry_time, const char* reason)
    {
		clientinfo* tci = NULL;

		uint target_id = 0;
		char* target_name = (char*)"";
		uint target_ip;
		uint target_mask = 0x00FFFFFF;

		uint master_id = ci->uid;
		char* master_name = (char*)ci->name;
		uint master_ip = getclientip(ci->clientnum);

		if(args.length() < 2)
		{
			sendcnservmsgf(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2%s <cn>\fr", args[0]);
			return;
		}

		int val = parse_ip_mask_string(args[1], &target_ip, &target_mask, &tci);

		if(val || !tci)
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Invalid player specifier arguments\fr");
			return;
		}

		target_id = tci->uid;
		target_name = tci->name;
		target_ip = getclientip(tci->clientnum);

        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	if(!punitiveeffects::search(getclientip(ci->clientnum), punitiveeffects::LIMIT)) {
        		complaint_notify(ci, target_name, target_ip, master_name, master_ip, reason);
        	}
        }
        else
        {
        	apply_effect(ci, effect_type, target_id, target_name, target_ip, target_mask, master_id, master_name, master_ip, expiry_time, reason);
        }
    }

    void cmd_cheater(clientinfo *ci, vector<char*> args)
    {
    	easy_add_effect_or_notify(ci, args, "ban", 4*YEARS, "cheating");
    }

    void cmd_spammer(clientinfo *ci, vector<char*> args)
    {
    	easy_add_effect_or_notify(ci, args, "mute", 4*HOURS, "spamming");
    }

    void cmd_teamkiller(clientinfo *ci, vector<char*> args)
    {
    	easy_add_effect_or_notify(ci, args, "spectate", 4*HOURS, "teamkilling");
    }

    void cmd_unbalancer(clientinfo *ci, vector<char*> args)
    {
    	easy_add_effect_or_notify(ci, args, "limit", 4*HOURS, "unbalancing teams");
    }

    void cmd_remumble(clientinfo *ci, vector<char*> args)
    {
        if(!hasadmingroup(ci) && !hasmastergroup(ci))
        {
        	insufficientpermissions(ci);
        	return;
        }
		loopv(clients) sendchangeteam(clients[i]);
    }

    void cmd_pause(clientinfo *ci, vector<char*> args)
    {
    	if(resumetime || gamepaused) return;
    	sendservmsgf("\fs\f1Info:\fr Game \fs\f3paused\fr by \fs\f0%s\fr.", colorname(ci));
    	forcepaused(true);
    }

    void cmd_resume(clientinfo *ci, vector<char*> args)
    {
    	if(resumetime || !gamepaused) return;
    	sendservmsgf("\fs\f1Info:\fr \fs\f0%s\fr resumed the game.", colorname(ci));
    	resumetime = totalmillis + resumedelay*1000;
    	resumecounter = resumedelay;
    }

    // returns 0 and sets toggleable if a valid value is specified.
    // returns -1 if an invalid arg is specified.
    // returns 1 if the arg is already in the state specified.
    int parse_toggle_arg(char *arg, int *toggleable)
    {
    	char *t = arg;
    	while(*t) { *t = tolower(*t); t++; }

    	if(!strcmp(arg, "enable") || !strcmp(arg, "yes") || !strcmp(arg, "true") || !strcmp(arg, "1")) {
    		if(*toggleable) return 1;
    		*toggleable = 1;
    	}
    	else if(!strcmp(arg, "disable") || !strcmp(arg, "no") || !strcmp(arg, "false") || !strcmp(arg, "0")) {
    		if(!(*toggleable)) return 1;
    		*toggleable = 0;
    	}
    	else return -1;
    	return 0;
    }

    void cmd_persistentintermission(clientinfo *ci, vector<char*> args)
    {
    	int v = -1;
        if(args.length() >= 2) v = parse_toggle_arg(args[1], &server::persistentintermission);

        if(!v){
        	sendservmsgf("\fs\f1Info:\fr Peristent intermission \fs\f0%s\fr by \fs\f0%s\fr.", server::persistentintermission ? "enabled" : "disabled" , colorname(ci));
        }
        else if(v == -1) {
        	sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2persistentintermission <enable/disable>\fr");
        }
        else if(v == 1) {
        	sendcnservmsgf(ci->clientnum, "\fs\f3Error:\fr State: Peristent intermission is already \fs\f0%s\fr.", server::persistentintermission ? "enabled" : "disabled");
        }
    }
    
    void cmd_persistentteams(clientinfo *ci, vector<char*> args)
    {
    	int v = -1;
        if(args.length() >= 2) v = parse_toggle_arg(args[1], &server::persistentteams);

        if(!v){
        	sendservmsgf("\fs\f1Info:\fr Persistent teams \fs\f0%s\fr by \fs\f0%s\fr.", server::persistentteams ? "enabled" : "disabled" , colorname(ci));
        }
        else if(v == -1) {
        	sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2persistentteams <enable/disable>\fr");
        }
        else if(v == 1) {
        	sendcnservmsgf(ci->clientnum, "\fs\f3Error:\fr State: Persistent teams is already \fs\f0%s\fr.", server::persistentteams ? "enabled" : "disabled");
        }
    }

    void cmd_mutespectators(clientinfo *ci, vector<char*> args)
    {
    	int v = -1;
        if(args.length() >= 2) v = parse_toggle_arg(args[1], &server::mutespectators);

        if(!v){
        	sendservmsgf("\fs\f1Info:\fr Mute spectators \fs\f0%s\fr by \fs\f0%s\fr.", server::mutespectators ? "enabled" : "disabled" , colorname(ci));
        }
        else if(v == -1) {
        	sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2mutespectators <enable/disable>\fr");
        }
        else if(v == 1) {
        	sendcnservmsgf(ci->clientnum, "\fs\f3Error:\fr State: Mute spectators is already \fs\f0%s\fr.", server::mutespectators ? "enabled" : "disabled");
        }
    }

    void cmd_pauseondisconnect(clientinfo *ci, vector<char*> args)
    {
    	int v = -1;
        if(args.length() >= 2) v = parse_toggle_arg(args[1], &server::pauseondisconnect);

        if(!v){
        	sendservmsgf("\fs\f1Info:\fr Pause on disconnect \fs\f0%s\fr by \fs\f0%s\fr.", server::pauseondisconnect ? "enabled" : "disabled" , colorname(ci));
        }
        else if(v == -1) {
        	sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2pauseondisconnect <enable/disable>\fr");
        }
        else if(v == 1) {
        	sendcnservmsgf(ci->clientnum, "\fs\f3Error:\fr State: Pause on disconnect is already \fs\f0%s\fr.", server::pauseondisconnect ? "enabled" : "disabled");
        }
    }
    
    void cmd_racemode(clientinfo *ci, vector<char*> args)
    {
    	int v = -1;
        if(args.length() >= 2) v = parse_toggle_arg(args[1], &server::racemode);

        if(!v){
        	sendservmsgf("\fs\f1Info:\fr Race mode \fs\f0%s\fr by \fs\f0%s\fr.", server::racemode ? "enabled" : "disabled" , colorname(ci));
        }
        else if(v == -1) {
        	sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2racemode <enable/disable>\fr");
        }
        else if(v == 1) {
        	sendcnservmsgf(ci->clientnum, "\fs\f3Error:\fr State: Race mode is already \fs\f0%s\fr.", server::racemode ? "enabled" : "disabled");
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
        
        sendservmsgf("\fs\f1Info:\fr Insta weapon now set to: \fs\f4%s\fr by \fs\f0%s\fr.", guns[w].name, colorname(ci));
    }

    void strip_commas(char *str)
    {
    	char *p = str;
    	while(*p++) if(*p == ',') *p = ' ';
    }

    void cmd_tagdemo(clientinfo *ci, vector<char*> args)
    {
        if(!ci->uid)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr You must be verified to use this feature.");
            return;
        }
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2tagdemo <tag>\fr");
            return;
        }

        vector<char> tag_text;
        loopv(args)
        {
        	if(!i) continue; // skip the first arg
        	tag_text.put(args[i], strlen(args[i]));
        	tag_text.put(' ');
        }
        tag_text[tag_text.length()-1] = '\0';

        // since comma is used to separate these later, strip any input commas out
        strip_commas(tag_text.getbuf());

        dmo_tag dt;
        dt.uid = ci->uid;
        dt.tag = (char*)malloc(sizeof(char)*tag_text.length());
        strncpy(dt.tag, tag_text.getbuf(), tag_text.length());

        dmo_tags.add(dt);

        sendcnservmsgf(ci->clientnum, "\fs\f1Info:\fr tagged demo: '%s'", dt.tag);
    }
    
    void cmd_resumedelay(clientinfo *ci, vector<char*> args)
    {
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2resumedelay <seconds>\fr");
            return;
        }

        int w = atoi(args[1]);

        if (w < 0 || w >= 15)
        {
        	sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr you must specify a number of seconds between 0 and 15.");
            return;
        }

        server::resumedelay = w;

        sendservmsgf("\fs\f1Info:\fr resumedelay set to: \fs\f4%d\fr by \fs\f0%s\fr.", server::resumedelay, colorname(ci));
    }

    void cmd_timeleft(clientinfo *ci, vector<char*> args)
    {
        if(args.length() < 2)
        {
            sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Usage: \fs\f2resumedelay <seconds>\fr");
            return;
        }

        uint w = 0;

		if(parse_time_string(args[1], &w))
		{
			sendcnservmsg(ci->clientnum, "\fs\f3Error:\fr Invalid time arguments\fr");
			return;
		}

		fprintf(stderr, "Got a number of seconds %u\n", w);

        gamelimit = gamemillis + w*1000;
        sendf(-1, 1, "ri2", N_TIMEUP, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);

        sendservmsgf("\fs\f1Info:\fr timeleft set to: \fs\f4%d\fr seconds by \fs\f0%s\fr.", gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0, colorname(ci));
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
        {"names", PRIV_NONE, &cmd_names},

        {"master", PRIV_NONE, &cmd_master},
        {"admin", PRIV_NONE, &cmd_admin},

        {"mute", PRIV_NONE, &cmd_mute},
        {"ban", PRIV_NONE, &cmd_ban},
        {"spec", PRIV_NONE, &cmd_spec},
        {"limit", PRIV_NONE, &cmd_limit},

        {"cheater", PRIV_NONE, &cmd_cheater},
        {"spammer", PRIV_NONE, &cmd_spammer},
        {"teamkiller", PRIV_NONE, &cmd_teamkiller},
        {"unbalancer", PRIV_NONE, &cmd_unbalancer},

        {"remumble", PRIV_NONE, &cmd_remumble},

        {"pause", PRIV_MASTER, &cmd_pause},
        {"resume", PRIV_MASTER, &cmd_resume},

        {"persistentintermission", PRIV_MASTER, &cmd_persistentintermission},
        {"persistentteams", PRIV_MASTER, &cmd_persistentteams},
        {"mutespectators", PRIV_MASTER, &cmd_mutespectators},
        {"pauseondisconnect", PRIV_MASTER, &cmd_pauseondisconnect},

        {"racemode", PRIV_MASTER, &cmd_racemode},
        {"instaweapon", PRIV_MASTER, &cmd_instaweapon},

        {"tagdemo", PRIV_NONE, &cmd_tagdemo},

        {"resumedelay", PRIV_MASTER, &cmd_resumedelay},
        {"timeleft", PRIV_MASTER, &cmd_timeleft},

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
        commandlist.put('\0');
        sendcnservmsgf(ci->clientnum, "\fs\f4Available commands:\fr %s", commandlist.getbuf());
    }
    
    void trycommand(clientinfo *ci, const char *cmd)
    {
    	logclientf(ci, "cmd: %s", cmd);
        
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
