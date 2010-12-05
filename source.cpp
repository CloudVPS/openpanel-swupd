// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include "swupd.h"
#include <grace/process.h>

extern bool SHUTDOWN_REQUESTED;

// ==========================================================================
// CONSTRUCTOR updatesource
// ==========================================================================
updatesource::updatesource (void)
{
	if (fs.exists (PATH_CACHEFILE)) cache.loadshox (PATH_CACHEFILE);
}

// ==========================================================================
// DESTRUCTOR updatesource
// ==========================================================================
updatesource::~updatesource (void)
{
}

// ==========================================================================
// METHOD updatesource::update
// ==========================================================================
void updatesource::update (const value &list)
{
	throw (baseClassException());
}

// ==========================================================================
// METHOD updatesource::getlist
// ==========================================================================
value *updatesource::getlist (void)
{
	throw (baseClassException());
}

// ==========================================================================
// METHOD updatesource::getdesc
// ==========================================================================
string *updatesource::getdesc (const statstring &i)
{
	throw (baseClassException());
}

// ==========================================================================
// METHOD updatesource::resolvedeps
// ==========================================================================
void updatesource::resolvedeps (const statstring &i, value &v)
{
	throw (baseClassException());
}

// ==========================================================================
// METHOD updatesource::list
// ==========================================================================
const value &updatesource::list (void)
{
	value list;
	value outargs;
	
	list = getlist ();
	
	foreach (update, list)
	{
		if (cache.exists (update.id()))
		{
			update = cache[update.id()];
		}
		else
		{
			update["description"] = getdesc (update.id());
			resolvedeps (update.id(), list);
		}
	}
	
	cache = list;
	cache.saveshox (PATH_CACHEFILE);
	return cache;
}

// ==========================================================================
// CONSTRUCTOR yumsource
// ==========================================================================
yumsource::yumsource (void)
{
}

// ==========================================================================
// DESTRUCTOR yumsource
// ==========================================================================
yumsource::~yumsource (void)
{
}

// ==========================================================================
// METHOD yumsource::update
// ==========================================================================
void yumsource::update (const value &list)
{
	value outargs;
	outargs.newval() = "/usr/bin/yum";
	outargs.newval() = "-y";
	outargs.newval() = "update";
	foreach (update, list)
	{
		outargs.newval() = update;
	}
	
	systemprocess yump (outargs, true);
	yump.run ();
	
	while (! yump.eof())
	{
		string line = yump.gets();
	}
	
	yump.serialize();
}

// ==========================================================================
// METHOD yumsource::getlist
// ==========================================================================
const value &yumsource::list (void)
{
	value list;
	value infoargs;
	string name, summary;
	
	list = getlist();
	if (! list.count())
	{
		cache = list;
		return cache;
	}

	APP->log (log::info, "yum-src", "Getting descriptions");

	infoargs.newval() = "/usr/bin/yum";
	infoargs.newval() = "-C";
	infoargs.newval() = "info";
	infoargs.newval() = "available";
	foreach (update, list)
	{
		infoargs.newval() = update.id();
	}

	systemprocess infop (infoargs, true);
	infop.run();


	while (! infop.eof())
	{
		string line = infop.gets();
		if (line.strlen())
		{
			if (line.strncmp ("Name   :", 8) == 0)
			{
				line = line.mid (9);
				name = line;
			}
			else if (line.strncmp ("Summary:", 8) == 0)
			{
				line = line.mid (9);
				list[name]["description"] = line;
			}
		}
	}

	infop.serialize();
	
	resolvealldeps (list);
	
	cache = list;
	cache.saveshox (PATH_CACHEFILE);

	return cache;
}

// ==========================================================================
// METHOD yumsource::getlist
// ==========================================================================
value *yumsource::getlist (void)
{
	returnclass (value) list retain;

	value outargs;
	bool islist = false;
	outargs.newval() = "/usr/bin/yum";
	outargs.newval() = "check-update";
	
	systemprocess yump (outargs, true);
	yump.run ();
	
	while (! yump.eof())
	{
		string line = yump.gets();
		if (! islist)
		{
			if (! line.strlen()) islist = true;
		}
		else if (line.strlen())
		{
			string name;
			string version;
			string src;
			int i;
			
			name = line.cutat (' ');
			name.cropatlast ('.'); // remove architecture
			for (i=0; (i<line.strlen())&&(line[i] == ' '); ++i);
			if (i) line = line.mid (i);
			version = line.cutat (' ');
			for (i=0; (i<line.strlen())&&(line[i] == ' '); ++i);
			if (i) line = line.mid (i);
			src = line;
			if (src.strchr (' ')) src.cropat (' ');

			list[name]["version"] = version;
			list[name]["source"] = src;
			list[name]["description"] = name;
			
		}
	}
	
	yump.serialize();
	APP->log (log::info, "yum-src", "Fetched %i packages", list.count());
	return &list;
}

// ==========================================================================
// METHOD yumsource::getdesc
// ==========================================================================
string *yumsource::getdesc (const statstring &id)
{
	returnclass (string) res retain;
	
	APP->log (log::info, "yum-src", "Getting description: %S", id.str());
	
	bool gotsummary = false;
	value infoargs;
	infoargs.newval() = "/usr/bin/yum";
	infoargs.newval() = "-C";
	infoargs.newval() = "info";
	infoargs.newval() = id;
	systemprocess infop (infoargs, true);
	infop.run();
	
	while (! infop.eof())
	{
		string line = infop.gets();
		if (line.strlen() &&
			(!gotsummary) && 
			(line.strncmp ("Summary:", 8) == 0))
		{
			line = line.mid (9);
			res = line;
			gotsummary = true;
		}
	}
	
	infop.serialize();
	return &res;
}

// ==========================================================================
// METHOD yumsource::resolvedeps
// ==========================================================================
void yumsource::resolvealldeps (value &list)
{
	value depargs;
	string id = "";
	depargs.newval() = "/usr/bin/yum";
	depargs.newval() = "-C";
	depargs.newval() = "deplist";
	
    foreach (update, list)
	{
		depargs.newval() = update.id();
	}

	systemprocess depp (depargs, true);
	depp.run ();
	
	APP->log (log::info, "yum-src", "Resolving dependencies: %S", id.str());
	
	while (! depp.eof())
	{
		string line = depp.gets();
		if (line.strlen())
		{
			if (line.strncmp ("package:", 8) == 0)
			{
				line = line.mid (9);
				line.cropat (' ');
				line.cropatlast ('.'); // remove architecture
				if (list.exists(line))
					id = line;
				else
					id = "";
			}
			
			if (id.strlen() && line.strncmp ("   provider:", 12) == 0)
			{
				line = line.mid (13);
				line.cropat (' ');
				line.cropatlast ('.'); // remove architecture
				if ((id != line.str()) && 
					(list.exists (line.str())))
				{
					if (! list[line.str()]["deps"].exists (id))
					{
						list[line.str()]["deps"][id] = true;
					}
					/*
					if (! list[id]["deps"].exists (line))
					{
						list[id]["deps"][line] = true;
					}*/
				}
			}
		}
	}
	
	depp.serialize();
}

// ==========================================================================
// CONSTRUCTOR aptsource
// ==========================================================================
aptsource::aptsource (void)
{
}

// ==========================================================================
// DESTRUCTOR aptsource
// ==========================================================================
aptsource::~aptsource (void)
{
}

// ==========================================================================
// METHOD aptsource::update
// ==========================================================================
void aptsource::update (const value &list)
{
	value outargs;
	outargs.newval() = "/usr/bin/apt-get";
	outargs.newval() = "-y";
	outargs.newval() = "--force-yes";
	outargs.newval() = "install";
	
	bool selfupdate = false;
	
	foreach (update, list)
	{
		if (update == "openpanel-swupd") selfupdate = true;
		else outargs.newval() = update;
	}
	
	systemprocess aptp (outargs, true);
	aptp.run ();
	
	while (! aptp.eof())
	{
		string line = aptp.gets();
		log::write (log::info, "update", "apt: %s" %format (line));
	}
	
	aptp.serialize();
	
	if (selfupdate)
	{
		log::write (log::info, "update", "Running self-update in background");
		core.sh ("/usr/bin/apt-get -y --force-yes install "
				 "openpanel-swupd >/var/openpanel/log/swupd.selfupdate.log "
				 "2>&1 </dev/null &");
		
		for (int i=0; i<30; ++i)
		{
			if (! SHUTDOWN_REQUESTED) sleep (1);
			else break;
		}
	}
}

// ==========================================================================
// METHOD aptsource::getlist
// ==========================================================================
value *aptsource::getlist (void)
{
	returnclass (value) list retain;

	core.sh ("/usr/bin/apt-get update");

	value outargs;
	int linecount = 0;
	outargs.newval() = "/usr/bin/apt-get";
	outargs.newval() = "-s";
	outargs.newval() = "upgrade";
	
	systemprocess aptp (outargs, true);
	aptp.run ();
	
	while (! aptp.eof())
	{
		string line = aptp.gets();
		linecount++;
		if (line.strlen() && (line.strncmp ("Inst ", 5) == 0))
		{
			string name;
			string version;
			string src;
			int i;
			
			line = line.mid (5);
			name = line.cutat (' ');
			version = line.cutafter ('(');
			version = version.trim ("()");
			src = version.cutafter (' ');
			src.cropat (' ');
			if (src[-1] == ')') src.cropat (')');
			
			list[name]["version"] = version;
			list[name]["source"] = src;
			list[name]["description"] = name;
			
		}
	}
	
	APP->log (log::info, "apt-src", "Fetched %i packages", list.count());
	
	aptp.serialize();
	return &list;
}

// ==========================================================================
// METHOD aptsource::getdesc
// ==========================================================================
string *aptsource::getdesc (const statstring &id)
{
	returnclass (string) res retain;
	
	bool gotsummary = false;
	value infoargs;
	infoargs.newval() = "/usr/bin/apt-cache";
	infoargs.newval() = "show";
	infoargs.newval() = id;
	systemprocess infop (infoargs, true);
	infop.run();
	
	APP->log (log::info, "apt-src", "Getting description: %S", id.str());
	
	while (! infop.eof())
	{
		string line = infop.gets();
		if (line.strlen() &&
			(!gotsummary) && 
			(line.strncmp ("Description:", 12) == 0))
		{
			line = line.mid (13);
			res = line;
			gotsummary = true;
		}
	}
	
	infop.serialize();
	return &res;
}

// ==========================================================================
// METHOD aptsource::resolvedeps
// ==========================================================================
void aptsource::resolvedeps (const statstring &id, value &list)
{
	value depargs;
	depargs.newval() = "/usr/bin/apt-get";
	depargs.newval() = "-s";
	depargs.newval() = "install";
	depargs.newval() = id;
	
	systemprocess depp (depargs, true);
	depp.run ();
	
	APP->log (log::info, "apt-src", "Resolving dependencies: %S", id.str());

	while (! depp.eof())
	{
		string line = depp.gets();
		if (line.strlen())
		{
			if (line.strncmp ("Inst ", 5) == 0)
			{
				line = line.mid (5);
				line.cropat (' ');
				if ((id != line.str()) && 
					(list.exists (line.str())))
				{
					if (! list[line.str()]["deps"].exists (id))
					{
						list[line.str()]["deps"][id] = true;
					}
					/*
					if (! list[id]["deps"].exists (line))
					{
						list[id]["deps"][line] = true;
					}*/
				}
			}
		}
	}
	
	depp.serialize();
}

// ==========================================================================
// CONSTRUCTOR rhnsource
// ==========================================================================
rhnsource::rhnsource (void)
{
}

// ==========================================================================
// DESTRUCTOR rhnsource
// ==========================================================================
rhnsource::~rhnsource (void)
{
}

// ==========================================================================
// METHOD rhnsource::update
// ==========================================================================
void rhnsource::update (const value &list)
{
	value outargs;
	outargs.newval() = "/usr/bin/up2date";
	outargs.newval() = "-u";
	foreach (update, list)
	{
		outargs.newval() = update;
	}
	
	systemprocess rhnp (outargs, true);
	rhnp.run ();
	
	while (! rhnp.eof())
	{
		string line = rhnp.gets();
	}
	
	rhnp.serialize();
}

// ==========================================================================
// METHOD rhnsource::getlist
// ==========================================================================
value *rhnsource::getlist (void)
{
	returnclass (value) list retain;

	value outargs;
	int linecount = 0;
	outargs.newval() = "/usr/bin/up2date";
	outargs.newval() = "--dry-run";
	outargs.newval() = "-u";
	bool gotfirstlist = false;
	bool inlist = false;
	
	systemprocess yump (outargs, true);
	yump.run ();
	
	while (! yump.eof())
	{
		string line = yump.gets();
		linecount++;
		if (line.strlen() && (line.strncmp ("------", 6) == 0))
		{
			if (! gotfirstlist)
			{
				gotfirstlist = true;
			}
			else
			{
				inlist = true;
			}
		}
		
		if (inlist)
		{
			string name;
			string version;
			string rel;
			string src;
			int i;
			
			name = line.cutat (' ');
			for (i=0; (i<line.strlen())&&(line[i] == ' '); ++i);
			if (i) line = line.mid (i);
			version = line.cutat (' ');
			for (i=0; (i<line.strlen())&&(line[i] == ' '); ++i);
			if (i) line = line.mid (i);
			rel = line.cutat (' ');
			
			version.printf ("-%s", rel.str());

			src = "up2date";

			list[name]["version"] = version;
			list[name]["source"] = src;
			list[name]["description"] = name;
		}
	}
	
	yump.serialize();
	return &list;
}

// ==========================================================================
// METHOD rhnsource::getdesc
// ==========================================================================
string *rhnsource::getdesc (const statstring &id)
{
	returnclass (string) res retain;
	
	bool gotsummary = false;
	value infoargs;
	infoargs.newval() = "/bin/rpm";
	infoargs.newval() = "-qi";
	infoargs.newval() = id;
	systemprocess infop (infoargs, true);
	infop.run();
	
	while (! infop.eof())
	{
		string line = infop.gets();
		if (line.strlen() &&
			(!gotsummary) && 
			(line.strncmp ("Summary     : ", 14) == 0))
		{
			line = line.mid (14);
			res = line;
			gotsummary = true;
		}
	}
	
	infop.serialize();
	return &res;
}

// ==========================================================================
// METHOD rhnsource::resolvedeps
// ==========================================================================
void rhnsource::resolvedeps (const statstring &id, value &list)
{
	value depargs;
	depargs.newval() = "/usr/bin/up2date";
	depargs.newval() = "--dry-run";
	depargs.newval() = "-u";
	depargs.newval() = id;
	int listno = 0;
	bool inlist = false;
	
	systemprocess depp (depargs, true);
	depp.run ();
	
	while (! depp.eof())
	{
		string line = depp.gets();
		if (line.strlen() && (line.strncmp ("------", 6) == 0))
		{
			listno++;
			if (listno == 3) inlist = true;
		}
		
		if (inlist)
		{
			line = line.cutat (' ');

			if ((line.strlen()) &&
			    (id != line.str()) && 
				(list.exists (line.str())))
			{
				if (! list[line.str()]["deps"].exists (id))
				{
					list[line.str()]["deps"][id] = true;
				}/*
				if (! list[id]["deps"].exists (line))
				{
					list[id]["deps"][line] = true;
				}*/
			}
		}
	}
	
	depp.serialize();
}

