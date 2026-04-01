#!/bin/sh
### BEGIN INIT INFO
# Provides:          database
# Required-Start:    $local_fs $syslog
# Required-Stop:     $local_fs
# Default-Start:     S
# Default-Stop:      K
# Description:       Manage database service
### END INIT INFO

NAME=database
DAEMON=/root/app/database
PIDFILE=/var/run/$NAME.pid
LOGFILE=/var/log/$NAME.log

case "$1" in
	start)
		echo "Starting $NAME..."
		start-stop-daemon -S -b -m -p $PIDFILE -x $DAEMON -- > $LOGFILE 2>&1
		;;
	stop)
		echo "Stopping $NAME..."
		start-stop-daemon -K -p $PIDFILE -x $DAEMON
		;;
	restart)
		$0 stop
		sleep 1
		$0 start
		;;
	*)
		echo "Usage: $0 {start|stop|restart}" >&2
		exit 3
		;;
esac

: