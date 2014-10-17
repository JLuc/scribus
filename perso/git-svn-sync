JL/DataBeaver

Q: the need is to be able to port the patches from git to the current svn
so we need to be able to update git branches to current state of svn (svn or git svn branch) before proposing a patch
A : git rebase should do the trick there.
In the simplest case, if you're on branch foo, git rebase svn will make sure that all commits in svn's history will also be in foo's history

Q : When does the simplest case fit or not fit ? Conflicts ?
A : It doesn't fit if there are some commits in foo's history you don't want included in the patch.  For example, suppose branch bar was merged into master and then foo was created off it; git rebase svn would now also take bar's commits since they're not in svn.
If there are conflicts during a rebase, the user will be asked to resolve them before continuing.
If the simplest case doesn't work, you can use git rebase master foo --onto svn, which will take commits between master and foo and rebase those on top of svn.  And if even that is not enough, adding -i will let you hand-pick the commits you want.
