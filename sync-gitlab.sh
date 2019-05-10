#!/bin/bash

# Congigure to start every day, for example the foolowing in 'crontab -e'
# 00 2 * * * /root/sync-gitlab.sh >> /root/sync.log

echo "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::"
echo ":::: Running sync of current upstream OpenSC to my fork in GitLab ::::"
echo -n "::: Date start: "
date
echo ":::"

## Check the gitlab runner is installed and configured
if ! rpm -qa "gitlab*"; then
	echo "ERROR: Gitlab Runner is not installed"
	echo "Follow the installation instructions from"
	echo "https://docs.gitlab.com/runner/install/linux-repository.html"
	exit
fi

if ! systemctl is-active gitlab-runner; then
	echo "ERROR: Gitlab Runner is not running"
	exit
fi

## Sanity
if [[ ! -d devel ]]; then
	mkdir devel
fi

if [[ ! -d devel/OpenSC ]]; then
	## prepare git reporistory with current version and upstream
	cd devel
	git clone git@gitlab.com:redhat-crypto/OpenSC.git
	cd OpenSC
	git remote add upstream https://github.com/OpenSC/OpenSC.git
else 
	cd devel/OpenSC
	git pull origin
fi

## fetch changes in upstream master
echo "::: git fetch upstream"
git fetch upstream
echo "::: git checkout master"
git checkout master
echo "::: git rebase upstream/master"
git rebase upstream/master

## the access key is in ~/.ssh/id_ecdsa
## -- I don't want to push in my own github from scripts
if [[ "$?" == 0 ]]; then
	echo "::: git push origin master --force"
	git push origin master --force

	echo ":::::: CAC ::::::"
	echo "::: git branch -f cac master"
	git branch -f cac master
	echo "::: git push origin cac --force"
	git push origin cac --force

	echo ":::::: Coolkey ::::::";
	echo "::: git branch -f coolkey master"
	git branch -f coolkey master
	echo "::: git push origin coolkey --force"
	git push origin coolkey --force
else
	echo "::: ERROR: Not pushing. Resolve the conflicts"
	echo Failed to merge upstream changes for Smart Card CI. Please resolve manually | mail -s "SmartCard CI: Update failed" jjelen@redhat.com
fi


echo -n "::: Date end: "
date
echo "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::"
echo
echo
