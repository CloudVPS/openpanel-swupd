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

#ifndef _swupd_H
#define _swupd_H 1
#include <grace/daemon.h>
#include <grace/configdb.h>

#define PATH_CACHEFILE "/var/opencore/db/softwareupdate.db"
#define PATH_LISTFILE "/var/opencore/db/updatelist.db"
#define PATH_SOCKET "/var/opencore/sockets/swupd/swupd.sock"

extern bool SHUTDOWN_REQUESTED;

//  -------------------------------------------------------------------------
/// Base class for an update source
//  -------------------------------------------------------------------------
THROWS_EXCEPTION (baseClassException, 0x4f8f0b10, "Base class method called");

class updatesource
{
public:
					 /// Constructor.
					 updatesource (void);
					 
					 /// Destructor.
	virtual			~updatesource (void);
	
					 /// Get a list of pending updates. Combines the
					 /// version information, descriptions and
					 /// dependencies in a single list.
	virtual const value		&list (void);
	
					 /// Run a set of updates.
					 /// \param list An array of package names.
	virtual void	 update (const value &list);
	
protected:
					 /// Virtual method, implementations should use this
					 /// to implement the gathering of available updates.
	virtual value 	*getlist (void);
	
					 /// Virtual method, implementations should use this
					 /// spot to resolve a package name to a description.
	virtual string 	*getdesc (const statstring &);
	
					 /// Virtual method, implementations should use this
					 /// to put the list of interdependencies into the
					 /// existing list of updates.
	virtual void	 resolvedeps (const statstring &, value &);

	value			 cache; ///< The loaded cache file.
};

//  -------------------------------------------------------------------------
/// Implementation of the yum-based update source.
//  -------------------------------------------------------------------------
class yumsource : public updatesource
{
public:
					 /// Constructor.
					 yumsource (void);
					 
					 /// Destructor.
	virtual			~yumsource (void);
	
					 /// Updater. Uses yum update.
	virtual void	 update (const value &list);
	
					 /// Get a list of pending updates. Combines the
					 /// version information, descriptions and
					 /// dependencies in a single list.
	virtual const value		&list (void);
	
protected:
					 /// List query. Uses yum check-update.
	virtual	value	*getlist (void);
	
					 /// Description query. Uses yum -C info.
	virtual string	*getdesc (const statstring &);
	
					 /// Dependency solver. Uses yum -C deplist.
	virtual void	 resolvealldeps (value &);
};

//  -------------------------------------------------------------------------
/// Implementation of the apt-based update source.
//  -------------------------------------------------------------------------
class aptsource : public updatesource
{
public:
					 /// Constructor.
					 aptsource (void);
					 
					 /// Destructor.
	virtual			~aptsource (void);
	
					 /// Updater. Uses apt-get upgrade.
	virtual void	 update (const value &list);
	
protected:
					 /// List query. Uses apt-get -s upgrade.
	virtual	value	*getlist (void);
	
					 /// Description query. Uses apt-cache show.
	virtual string	*getdesc (const statstring &);
	
					 /// Dependency solver. Uses apt-get -s install
					 /// to observe the extra packages installed.
	virtual void	 resolvedeps (const statstring &, value &);
};

//  -------------------------------------------------------------------------
/// Implementation of the up2date-based update source.
//  -------------------------------------------------------------------------
class rhnsource : public updatesource
{
public:
					 /// Constructor.
					 rhnsource (void);
					 
					 /// Destructor.
	virtual			~rhnsource (void);
	
					 /// Updater. Uses apt-get upgrade.
	virtual void	 update (const value &list);
	
protected:
					 /// List query. Uses dry-run of up2date -u.
	virtual	value	*getlist (void);
	
					 /// Description query. Uses rpm -qi.
	virtual string	*getdesc (const statstring &);
	
					 /// Dependency solver. Uses a dry-run of up2date -u
					 /// to ovserve extra packages installed.
	virtual void	 resolvedeps (const statstring &, value &);
};

//  -------------------------------------------------------------------------
/// Worker thread that serializes the access to the distro upgrade system.
//  -------------------------------------------------------------------------
class updatethread : public thread
{
public:
					 /// Constructor. Couples to the source and
					 /// parent application.
					 updatethread (updatesource *src,
					 			   class swupdApp *papp);
					 			   
					 /// Completely uninteresting virtual destructor.
					~updatethread (void);
					
					 /// Run method. Accepts three commands:
					 /// - 'die' shutds down the thread
					 /// - 'getupdates' refreshes the list/cache file
					 /// - 'update' executes the updates
	void			 run (void);
	
					 /// Sends a 'die' event and waits for the thread
					 /// to finish.
	void			 shutdown (void);
	
protected:
	updatesource	*src; ///< The update source
	class swupdApp	*app; ///< The application object
	conditional		 shutdowncond; ///< Will be broadcast on shutdown
};

//  -------------------------------------------------------------------------
/// Implementation template for application config.
//  -------------------------------------------------------------------------
typedef configdb<class swupdApp> appconfig;

extern swupdApp *APP;
//  -------------------------------------------------------------------------
/// Main daemon class.
//  -------------------------------------------------------------------------
class swupdApp : public daemon
{
public:
		 				 swupdApp (void)
		 				 	: daemon ("com.openpanel.svc.swupd"),
		 				 	  conf (this)
		 				 {
		 				 	APP = this;
		 				 }
		 				 
		 				~swupdApp (void)
		 				 {
		 				 }
		 	
	int					 main (void);
	
protected:
	bool				 confLog (config::action act, keypath &path,
								  const value &nval, const value &oval);

	appconfig			 conf;
	updatesource		*src;
};


#endif
