// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2007 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#include "swupd.h"
#include "version.h"
#include <grace/tcpsocket.h>

APPOBJECT(swupdApp);

bool SHUTDOWN_REQUESTED;
swupdApp *APP;

void handle_SIGTERM (int sig)
{
	SHUTDOWN_REQUESTED = true;
}

// ==========================================================================
// FUNCTION getsource
// ==========================================================================
updatesource *getsource (void)
{
	updatesource *src = NULL;
	#ifdef __FLAVOR_LINUX_REDHAT_YUM
		src = new yumsource;
	#endif
	
	#ifdef __FLAVOR_LINUX_REDHAT_UP2DATE
		src = new rhnsource;
	#endif
	
	#ifdef __FLAVOR_LINUX_DEBIAN
		src = new aptsource;
	#endif
	return src;
}

// ==========================================================================
// METHOD swupdApp::main
// ==========================================================================
int swupdApp::main (void)
{
	if (argv.exists ("--version"))
	{
		fout.printf ("OpenPanel swupd %s\n", SWUPD_VERSION_FULL);
		return 0;
	}
	string conferr;
	
	conf.addwatcher ("system/eventlog", &swupdApp::confLog);
	
	if (! conf.load ("com.openpanel.svc.swupd", conferr))
	{
		ferr.printf ("%% Error loading configuration: %s\n", conferr.str());
		return 1;
	}
	
	log (log::info, "main", "OpenPanel swupd %s starting", SWUPD_VERSION);
	src = getsource ();

	if (! src)
	{
		log (log::critical, "main", "Unsupported OS Distribution");
		stoplog ();
		return 1;
	}
	
	SHUTDOWN_REQUESTED = false;
	signal (SIGTERM, handle_SIGTERM);
	
	if (fs.exists (PATH_SOCKET)) fs.rm (PATH_SOCKET);
	tcplistener listen (PATH_SOCKET);
	tcpsocket s;
	updatethread thr (src, this);
	
	if (! fs.chgrp (PATH_SOCKET, "authd"))
	{
		log (log::critical, "main", "Error setting group on socket");
		SHUTDOWN_REQUESTED = true;
	}
	if (! fs.chmod (PATH_SOCKET, 0660))
	{
		log (log::critical, "main", "Error setting permissions on "
			 "socket");
		SHUTDOWN_REQUESTED = true;
	}
	
	if (! SHUTDOWN_REQUESTED) // Initial run.
	{
		value outev;
		outev["cmd"] = "getupdates";
		thr.sendevent (outev);
	}

	// Main loop.
	while (! SHUTDOWN_REQUESTED)
	{
		// Calculate current time and time for next update fetch.
		time_t now = kernel.time.now ();
		time_t nextround = now + 7200;
		
		// Idle-loop
		while ((! SHUTDOWN_REQUESTED) && (now < nextround))
		{
			now = kernel.time.now ();
			
			// Fish for a connection on our unix domain socket
			s = listen.tryaccept (5.0);
			if (s)
			{
				string ln; // Let's wait 10 seconds for some input.
				if (s.waitforline (ln, 10000, 1024))
				{
					// Is it an update command?
					if (ln == "update")
					{
						// So it is, load the list file.
						value list;
						list.loadshox (PATH_LISTFILE);
						
						// Create the event data
						value outev;
						outev["cmd"] = "update";
						
						// Add the updates from the cache file
						foreach (update, list["flags"])
						{
							if (update == true)
							{
								outev["list"].newval() = update.id();
							}
						}
						
						int cnt = outev["list"].count();
						
						// Any updates selected?
						if (cnt)
						{
							// Yes, send the event
							log (log::info, "main", "Starting update of %i "
								 "packages", cnt);
								 
							thr.sendevent (outev);
						}
						else
						{
							// No, whine
							log (log::info, "main", "Update requested with "
								 "no selected updates");
						}
						
						// Send OK result (if the bastard didn't hang up)
						try { s.writeln ("+OK"); }
						catch (...) { }
					}
					else
					{
						// Send error result.
						try { s.writeln ("-ERROR"); }
						catch (...) { }
					}
				}
				s.close ();
			}
		}
		
		// Our time elapsed, let's schedule a new fetch.
		if (! SHUTDOWN_REQUESTED)
		{
			value outev;
			outev["cmd"] = "getupdates";
			thr.sendevent (outev);
		}
	}
	
	// Someone tripped on the SIGTERM, let's boogie.
	log (log::info, "main", "Shutting down");
	thr.shutdown ();
	stoplog ();
	
	return 0;
}

// ==========================================================================
// METHOD swupdApp::confLog
// ==========================================================================
bool swupdApp::confLog (config::action act, keypath &kp,
						const value &nval, const value &oval)
{
	string tstr;
	
	switch (act)
	{
		case config::isvalid:
			tstr = strutil::makepath (nval.sval());
			if (! tstr.strlen()) return true;
			if (! fs.exists (tstr))
			{
				ferr.printf ("%% Event log path %s does not exist",
							 tstr.str());
				return false;
			}
			return true;
			
		case config::create:
			addlogtarget (log::file, nval.sval(), 0xff, 1024*1024);
			daemonize();
			return true;
			
		default:
			return false;
	}
	
	// unreachable
	return false;
}

// ==========================================================================
// CONSTRUCTOR updatethread
// ==========================================================================
updatethread::updatethread (updatesource *s, swupdApp *papp) : thread ()
{
	src = s;
	app = papp;
	spawn();
}

// ==========================================================================
// DESTRUCTOR updatethread
// ==========================================================================
updatethread::~updatethread (void)
{
}

// ==========================================================================
// METHOD updatethread::run
// ==========================================================================
void updatethread::run (void)
{
	value ev;
	value updates;
	
	app->log (log::info, "updater", "Starting thread");
	
	// One Infinite Loop (will break out on a die)
	while (true)
	{
		// Wait for a new event, we're a passive thread.
		ev = waitevent();
		caseselector (ev["cmd"])
		{
			incaseof ("die") :
				// That's it, life will end.
				app->log (log::info, "updater", "Shutting down thread");
				shutdowncond.broadcast ();
				return;
				
			incaseof ("getupdates") :
				// Fetch updates.
				app->log (log::info, "updater", "Getting new updates");
				updates = src->list ();
				app->log (log::info, "updater", "Fetch complete, %i "
						  "packages available", updates.count());
				break;
				
			incaseof ("update") :
				// Install a list of updates.
				app->log (log::info, "updater", "Installing %i new updates",
						  ev["list"].count());
				src->update (ev["list"]);
				src->list ();
				app->log (log::info, "updater", "Update completed");
				break;
				
			defaultcase :
				// Wrong-headed command, bitch and moan.
				app->log (log::warning, "updater", "Received unknown event "
						  "cmd=<%S>", ev["cmd"].cval());
				break;
		}
	}
}

// ==========================================================================
// METHOD updatethread::shutdown
// ==========================================================================
void updatethread::shutdown (void)
{
	value out;
	out["cmd"] = "die";
	sendevent (out);
	shutdowncond.wait ();
}
