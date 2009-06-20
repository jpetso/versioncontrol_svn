$Id$

xsvn-* scripts
--------------

This directory contains a set of scripts that can be used via the hooks provided
by SVN to enable access control for commit and tag operations, and to record log
messages and other information into the Drupal database for proper integration
with the Version Control API.

These scripts are required if you enable the "Use external script to insert
data" option on the respective repository edit page, and should be copied to the
$SVN_DIR/hooks directory, as well as the file "xsvn-config.php". Remember to
make these files executable (with chmod).

Any bug reports or feature requests concerning the xsvn-* scripts or the SVN
backend in general should be submitted to the SVN backend issue queue:
http://drupal.org/project/issues/versioncontrol_svn.

If you know that the functionality is (or should be) provided by the Version
Control API (and not by the SVN backend), please submit an issue there:
http://drupal.org/project/issues/versioncontrol.


Each script, and the SVN_DIR/hooks hook required for its use, are described
below:

--------------------
xsvn-config.php
--------------------

  A configuration file that all of the other scripts depend on. The file is
  heavily commented with instructions on each possible configuration setting,
  and also contains a few minor bits of shared code that is used by all other
  scripts. There are a few required settings at the top which must be
  customized to your site before anything else will work.

--------------------
xsvn-start-commit.php
--------------------

  TODO: Currently unused. This one can only check the user name and the
        repository. Could be used as a speedup for commit access checking: it
        could be tested whether a user has access to the repository at all,
        though this would be redundant for sites with a small number of
        repositories (like drupal.org).

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-start-commit.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@

--------------------
xsvn-pre-commit.php
--------------------

  A script to enforce access controls for users attempting to commit. This hook
  is triggered before a transaction is saved as a commit. To enable it, add this
  line to your SVN_DIR/hooks/pre-commit file.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-pre-commit.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


--------------------
xsvn-post-commit.php
--------------------

  A script to insert info from a commit into the Drupal database by passing it
  to the Version Control API. To enable this script, add the following line to
  your SVN_DIR/hooks/post-commit file.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-post-commit.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


--------------------
xsvn-pre-revprop-change.php
--------------------

  TODO: Currently unused. By default, SVN disallows revision property changes
        unless enabled with this hook.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-pre-revprop-change.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


--------------------
xsvn-post-revprop-change.php
--------------------

  TODO: Currently unused. Runs after a revision property change.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-post-revprop-change.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


--------------------
xsvn-pre-lock.php
--------------------

  TODO: Currently unused.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-pre-lock.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


--------------------
xsvn-post-lock.php
--------------------

  TODO: Currently unused.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-post-lock.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


--------------------
xsvn-pre-unlock.php
--------------------

  TODO: Currently unused.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-pre-unlock.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@

--------------------
xsvn-post-unlock.php
--------------------

  TODO: Currently unused.

#!/bin/sh
php [path_to_svn]/hooks/xsvn/xsvn-post-unlock.php [path_to_svn]/hooks/xsvn/xsvn-config.php $@


AUTHOR
------
Daniel Hackney ("chrono325", http://drupal.org/user/384635)
