@echo off
rem Github repo: https://github.gatech.edu/cs6290/cs6290-docker (PRs welcome)

rem Change this to somewhere else if you want. By default, this is mounting
rem your current directory in the container when you run this script
set workdir=%cd%
set image=ausbin/cs6290

if not exist %workdir% md %workdir%

docker image inspect "$image" > nul 2>&1 || docker pull %image%
if "%1"=="--pull" docker pull %image%

docker run -it --rm --mount "type=bind,src=%workdir%,dst=/home/student/workdir" %image%