#! /vendor/bin/sh

# EXPERIMENTAL: Optimize UX task cgroup membership
PIDSS=`ps -AT | grep system_server | awk '{print $3}'`
echo $PIDSS > /dev/cpuset/foreground/cgroup.procs
echo $PIDSS > /dev/stune/foreground/cgroup.procs

PIDAIO=`ps -AT | grep android.io | awk '{print $3}'`
echo $PIDAIO > /dev/stune/foreground/tasks

PIDAA=`ps -AT | grep android.anim | awk '{print $3}'`
echo $PIDAA > /dev/cpuset/top-app/tasks

PIDAALF=`ps -AT | grep android.anim.lf | awk '{print $3}'`
echo $PIDAALF > /dev/cpuset/top-app/tasks

PIDAFG=`ps -AT | grep android.fg | awk '{print $3}'`
echo $PIDAFG > /dev/stune/foreground/tasks

PIDAUI=`ps -AT | grep android.ui | awk '{print $3}'`
echo $PIDAUI > /dev/stune/top-app/tasks

PIDAD=`ps -AT | grep android.display | awk '{print $3}'`
echo $PIDAD > /dev/cpuset/top-app/tasks
echo $PIDAD > /dev/stune/top-app/tasks

PIDRECLAIMD=`ps -AT | grep reclaimd | awk '{print $3}'`
echo $PIDRECLAIMD > /dev/stune/top-app/tasks
