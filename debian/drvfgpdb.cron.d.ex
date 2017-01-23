#
# Regular cron jobs for the drvfgpdb package
#
0 4	* * *	root	[ -x /usr/bin/drvfgpdb_maintenance ] && /usr/bin/drvfgpdb_maintenance
