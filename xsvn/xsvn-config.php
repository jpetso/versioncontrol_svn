<?php

// $Id$
/**
 * @file
 * Configuration variables and bootstrapping code for all SVN hook scripts.
 *
 * Copyright 2009 by Daniel Hackney ("chrono325", http://drupal.org/user/384635)
 */

// ------------------------------------------------------------
// Required customization
// ------------------------------------------------------------

// Base path of drupal directory (no trailing slash)
$xsvn['drupal_path'] = '/home/username/public_html';

// File location where to store temporary files.
$xsvn['temp'] = '/tmp';

// Drupal repository id that this installation of scripts is going to interact
// with. In order to find out the repository id, go to the "VCS repositories"
// administration page, then click on the "edit" link of the concerned
// repository, and notice the final number in the resulting URL.
$xsvn['repo_id'] = 1;


// ------------------------------------------------------------
// Optional customization
// ------------------------------------------------------------

// These users are always allowed full access, even if we can't connect to the
// DB. This optional list should contain the SVN usernames (not the Drupal
// username if they're different).
$xsvn['allowed_users'] = array();

// If you run a multisite installation, specify the directory name that your
// settings.php file resides in (ex: www.example.com) If you use the default
// settings.php file, leave this blank.
$xsvn['multisite_directory'] = '';

// ------------------------------------------------------------
// Shared code
// ------------------------------------------------------------

// Store the current working directory at include time, because it's being
// changed when Drupal is bootstrapped. Note that even though this file is in
// the "hooks" subdirectory, getcwd() will still return the path of the
// repository root, not the directory which contains this script.
$xsvn['cwd'] = getcwd();

/**
 * Bootstrap all of Drupal (DRUPAL_BOOTSTRAP_FULL phase) and set the current
 * working directory to the Drupal root path.
 */
function xsvn_bootstrap($xsvn) {
  chdir($xsvn['drupal_path']);

  // Bootstrap Drupal so we can use Drupal functions to access the databases,
  // etc.
  if (!file_exists('./includes/bootstrap.inc')) {
    fwrite(STDERR, "Error: failed to load Drupal's bootstrap.inc file.\n");
    exit(1);
  }

  // Set up a few variables, Drupal might not bootstrap without those.
  // Copied from scripts/drupal.sh.
  $_SERVER['HTTP_HOST']       = 'default';
  $_SERVER['PHP_SELF']        = '/index.php';
  $_SERVER['REMOTE_ADDR']     = '127.0.0.1';
  $_SERVER['SERVER_SOFTWARE'] = 'PHP CLI';
  $_SERVER['REQUEST_METHOD']  = 'GET';
  $_SERVER['QUERY_STRING']    = '';
  $_SERVER['PHP_SELF']        = $_SERVER['REQUEST_URI'] = '/';

  // Set up the multisite directory if necessary.
  if ($xsvn['multisite_directory']) {
    $_SERVER['HTTP_HOST'] = $xsvn['multisite_directory'];
    // Set a dummy script name, so the multisite configuration
    // file search will always trigger.
    $_SERVER['SCRIPT_NAME'] = '/foo';
  }

  require_once './includes/bootstrap.inc';
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

  // Overwrite db_prefix if this is a simpletest run.
  if (isset($GLOBALS['simpletest_db_prefix'])) {
    $GLOBALS['db_prefix'] = $GLOBALS['simpletest_db_prefix'];
  }

}

/**
 * Returns the path of the temporary directory and checks that it is writable.
 *
 * @param temp_path
 *   Path to the temporary directory.
 *
 * @return
 *   The input directory, stripped of trailing slashes.
 */
function xsvn_get_temp_directory($temp_path) {
  $tempdir = preg_replace('/\/+$/', '', $temp_path); // strip trailing slashes
  if (!(is_dir($tempdir) && is_writeable($tempdir))) {
    fwrite(STDERR, "Error: failed to access the temporary directory ($tempdir).\n");
    exit(2);
  }
  return $tempdir;
}

// TODO: what do I do?
function xsvn_log_add($filename, $dir, $mode = 'w') {
  $fd = fopen($filename, $mode);
  fwrite($fd, $dir);
  fclose($fd);
}

// TODO: Explain this more.
function xsvn_is_last_directory($logfile, $dir) {
  if (file_exists($logfile)) {
    $fd = fopen($logfile, 'r');
    $last = fgets($fd);
    fclose($fd);
    return $dir == $last ? TRUE : FALSE;
  }
  return TRUE;
}
