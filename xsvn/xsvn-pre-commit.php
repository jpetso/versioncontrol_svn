#!/usr/bin/env php
<?php
// $Id$

/**
 * @file xsvn-pre-commit.php
 *
 * Provides access checking for 'svn commit' commands.
 *
 * Copyright 2009 by Daniel Hackney ("chrono325", http://drupal.org/user/384635)
 */

function xsvn_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: $cli <config file> REPO_PATH TX_NAME\n\n");
}

/**
 * Returns the author of the given transaction in the repository.
 *
 * @param tx
 *   The transaction ID for which to find the author.
 *
 * @param repo
 *   The repository in which to look for the author.
 */
function xsvn_get_commit_author($tx, $repo) {
  return trim(shell_exec("svnlook author -t $tx $repo"));
}

/**
 * Returns the files and directories which were modified by the transaction with
 * their status.
 *
 * @param tx
 *   The transaction ID for which to find the modified files.
 *
 * @param repo
 *   The repository.
 *
 * @return
 *   An array of files and directories modified by the transaction. The keys are
 *   the paths of the file and the value is the status of the item, as returned
 *   by "svnlook changed".
 */
function xsvn_get_commit_files($tx, $repo) {
  $str = shell_exec("svnlook changed -t $tx $repo");
  $lines = preg_split("/\n/", $str, -1, PREG_SPLIT_NO_EMPTY);

  // Separate the status from the path names.
  foreach ($lines as $line) {
    list($status, $path) = preg_split("/\s+/", $line);
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
 *   The status of the item, as returned from "svnlook changed".
 */
function xsvn_get_operation_item($path, $status) {
  $item['path'] = $path;

  // The item has been deleted.
  if (preg_match("@D@", $status)) {
    // A trailing slash means the item is a directory
    $item['type'] = (preg_match("@/$@", $path)) ?
      VERSIONCONTROL_ITEM_DIRECTORY_DELETED : VERSIONCONTROL_ITEM_FILE_DELETED;
  } else {
    $item['type'] = (preg_match("@/$@", $path)) ?
      VERSIONCONTROL_ITEM_DIRECTORY : VERSIONCONTROL_ITEM_FILE;
  }

  switch ($status) {
    case "A":
      $item['action'] = VERSIONCONTROL_ACTION_ADDED;
      break;
    case "D":
      $item['action'] = VERSIONCONTROL_ACTION_DELETED;
      break;
    case "U":
      $item['action'] = VERSIONCONTROL_ACTION_MODIFIED;
      break;
    case "_U":
      // Item properties have changed, but nothing else.
      $item['action'] = VERSIONCONTROL_ACTION_OTHER;
      break;
    case "UU":
      // Both properties and contents have changed.
      // TODO: Should this count as MODIFIED or OTHER, since it's really both?
      $item['action'] = VERSIONCONTROL_ACTION_MODIFIED;
      break;
    default:
      fwrite(STDERR, "Error: failed to read the status of the commit.\n");
      exit(4);
  }

  return $item;
}


/**
 * The main function of the hook.
 *
 * Expects the following arguments:
 *
 *   - $argv[1] - The path of the configuration file, xsvn-config.php.
 *   - $argv[2] - The path of the subversion repository.
 *   - $argv[3] - Commit transaction name.
 *
 * @param argc
 *   The number of arguments on the command line.
 *
 * @param argv
 *   Array of the arguments.
 */
function xsvn_init($argc, $argv) {
  $this_file = array_shift($argv);   // argv[0]

  if ($argc < 4) {
    xsvn_help($this_file, STDERR);
    exit(3);
  }

  $config_file = array_shift($argv); // argv[1]
  $repo        = array_shift($argv); // argv[2]
  $tx          = array_shift($argv); // argv[3]
  $username    = xsvn_get_commit_author($tx, $repo);
  $item_paths  = xsvn_get_commit_files($tx, $repo);

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, "Error: failed to load configuration file.\n");
    exit(4);
  }
  include_once $config_file;

  // Check temporary file storage.
  $tempdir = xsvn_get_temp_directory($xsvn['temp']);

    // Admins and other privileged users don't need to go through any checks.
  if (!in_array($username, $xsvn['allowed_users'])) {
    // Do a full Drupal bootstrap.
    xsvn_bootstrap($xsvn);

    // Construct a minimal commit operation array.
    $operation = array(
      'type' => VERSIONCONTROL_OPERATION_COMMIT,
      'repo_id' => $xsvn['repo_id'],
      'username' => $username,
      'labels' => array(), // TODO: don't support labels yet.
    );

    // Set the $operation_items array from the item path and status.
    foreach ($item_paths as $path => $status) {
      $item = xsvn_get_operation_item($path, $status);
      $operation_items[$path] = $item;
    }
    $access = versioncontrol_has_write_access($operation, $operation_items);

    // Fail and print out error messages if commit access has been denied.
    if (!$access) {
      fwrite(STDERR, implode("\n\n", versioncontrol_get_access_errors()) ."\n\n");
      exit(6);
    }
  }
}
xsvn_init($argc, $argv);
