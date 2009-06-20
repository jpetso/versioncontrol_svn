<?php

// $Id$
/**
 * @file
 * Configuration variables and bootstrapping code for all SVN hook scripts.
 *
 * Copyright 2007, 2008, 2009 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 * Copyright 2008 by Chad Phillips ("hunmonk", http://drupal.org/user/22079)
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

  // Bootstrap Drupal so we can use Drupal functions to access the database, etc.
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
 * Returns the author of the given revision or transaction in the repository.
 *
 * @param $rev_or_tx
 *   The revision number or transaction ID for which to find the author.
 *
 * @param $is_revision
 *   Is the $rev_or_tx argument a revision number or a transaction identifier?
 *   svnlook needs to know which type of object to look for. True if it is a
 *   revision, false if it is a transaction identifier. Defaults to revision.
 *
 * @param $repo
 *   The repository in which to look for the author.
 */
function xsvn_get_commit_author($rev_or_tx, $repo, $is_revision = TRUE) {
  $rev_str = $is_revision ? '-r' : '-t';
  return trim(shell_exec("svnlook author $rev_str $rev_or_tx $repo"));
}

/**
 * Returns the files and directories which were modified by the commit or
 * transaction with their status.
 *
 * @param $rev_or_tx
 *   The revision number or transaction ID for which to find the modified files.
 *
 * @param repo
 *   The repository.
 *
 * @return
 *   An array of files and directories modified by the commit or
 *   transaction. The keys are the paths of the file and the value is the status
 *   of the item, as returned by 'svnlook changed'.
 */
function xsvn_get_commit_files($rev_or_tx, $repo, $is_revision = TRUE) {
  $rev_str = $is_revision ? '-r' : '-t';
  $str = shell_exec("svnlook changed $rev_str $rev_or_tx $repo");
  $lines = preg_split('/\n/', $str, -1, PREG_SPLIT_NO_EMPTY);

  // Separate the status from the path names.
  foreach ($lines as $line) {
    // Limit to 2 elements to avoid cutting up paths with spaces.
    list($status, $path) = preg_split('/\s+/', $line, 2);
    $items[$path] = $status;
  }
  return $items;
}

/**
 * Fill an item array suitable for versioncontrol_has_write_access from an
 * item's path and status.
 *
 * @param path
 *   The path of the item.
 *
 * @param status
 *   The status of the item, as returned from 'svnlook changed'.
 */
function xsvn_get_operation_item($path, $status) {
  $item['path'] = $path;

  // The item has been deleted.
  if (preg_match('@D@', $status)) {
    // A trailing slash means the item is a directory
    $item['type'] = (preg_match('@/$@', $path)) ?
      VERSIONCONTROL_ITEM_DIRECTORY_DELETED : VERSIONCONTROL_ITEM_FILE_DELETED;
  }
  else {
    $item['type'] = (preg_match('@/$@', $path)) ?
      VERSIONCONTROL_ITEM_DIRECTORY : VERSIONCONTROL_ITEM_FILE;
  }

  switch ($status) {
    case 'A':
      $item['action'] = VERSIONCONTROL_ACTION_ADDED;
      break;
    case 'D':
      $item['action'] = VERSIONCONTROL_ACTION_DELETED;
      break;
    case 'U':
      $item['action'] = VERSIONCONTROL_ACTION_MODIFIED;
      break;
    case '_U':
      // Item properties have changed, but nothing else.
      $item['action'] = VERSIONCONTROL_ACTION_OTHER;
      break;
    case 'UU':
      // Both properties and contents have changed.
      // TODO: Should this count as MODIFIED or OTHER, since it's really both?
      $item['action'] = VERSIONCONTROL_ACTION_MODIFIED;
      break;
    default:
      fwrite(STDERR, t('Error: failed to read the status of the commit.') ."\n");
      exit(4);
  }
  return $item;
}
