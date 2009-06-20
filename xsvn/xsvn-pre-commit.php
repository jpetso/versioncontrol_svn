<?php
// $Id$

/**
 * @file
 * Provides access checking for 'svn commit' commands.
 *
 * Copyright 2009 by Daniel Hackney ('chrono325', http://drupal.org/user/384635)
 */

/**
 * Prints usage of this program.
 */
function xsvn_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: php $cli <config file> REPO_PATH TX_NAME\n\n");
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

  if ($argc != 4) {
    xsvn_help($this_file, STDERR);
    exit(3);
  }

  $config_file = array_shift($argv); // argv[1]
  $repo        = array_shift($argv); // argv[2]
  $tx          = array_shift($argv); // argv[3]

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, t('Error: failed to load configuration file.') ."\n");
    exit(4);
  }
  include_once $config_file;

  // Third argument is FALSE to indicate a transaction.
  $username    = xsvn_get_commit_author($tx, $repo, FALSE);
  $item_paths  = xsvn_get_commit_files($tx, $repo, FALSE);

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
